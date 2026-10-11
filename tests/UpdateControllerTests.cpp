#include <QTest>
#include <QSignalSpy>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include "app/controllers/UpdateController.h"

namespace {
struct Response {
  QByteArray body;
  QNetworkReply::NetworkError error = QNetworkReply::NoError;
  int chunkSize = 32;
  int httpStatus = 200;
};
class Reply final : public QNetworkReply {
public:
  Reply(const QNetworkRequest& request, Response response, QObject* parent)
      : QNetworkReply(parent), response_(std::move(response)) {
    setRequest(request);
    setUrl(request.url());
    setAttribute(QNetworkRequest::HttpStatusCodeAttribute, response_.httpStatus);
    open(QIODevice::ReadOnly);
    QTimer::singleShot(0, this, [this] { deliver(); });
  }
  void abort() override {
    if (isFinished())
      return;
    setError(OperationCanceledError, "模拟取消");
    setFinished(true);
    emit finished();
  }
  qint64 bytesAvailable() const override {
    return buffer_.size() + QIODevice::bytesAvailable();
  }

protected:
  qint64 readData(char* data, qint64 size) override {
    const qint64 n = qMin<qint64>(size, buffer_.size());
    if (n == 0)
      return isFinished() ? -1 : 0;
    memcpy(data, buffer_.constData(), size_t(n));
    buffer_.remove(0, n);
    return n;
  }

private:
  void deliver() {
    if (isFinished())
      return;
    if (response_.error != NoError) {
      setError(response_.error, "模拟网络错误");
      setFinished(true);
      emit finished();
      return;
    }
    const auto chunk = response_.body.mid(offset_, response_.chunkSize);
    offset_ += chunk.size();
    buffer_.append(chunk);
    emit readyRead();
    if (isFinished())
      return;
    emit downloadProgress(offset_, response_.body.size());
    if (isFinished())
      return;
    if (offset_ >= response_.body.size()) {
      setFinished(true);
      emit finished();
    } else
      QTimer::singleShot(0, this, [this] { deliver(); });
  }
  Response response_;
  QByteArray buffer_;
  qint64 offset_ = 0;
};
class Network final : public QNetworkAccessManager {
public:
  QList<Response> responses;
  QList<QUrl> requests;

protected:
  QNetworkReply* createRequest(Operation, const QNetworkRequest& req, QIODevice*) override {
    requests.append(req.url());
    return new Reply(req,
                     responses.isEmpty() ? Response{{}, QNetworkReply::ContentNotFoundError}
                                         : responses.takeFirst(),
                     this);
  }
};
QString suffix() {
#if defined(Q_PROCESSOR_ARM_64)
  return "-win-arm64-setup.exe";
#else
  return "-win64-setup.exe";
#endif
}
QByteArray release() {
  const QString name = "ClickFlow-0.6.0" + suffix();
  const QString prefix = "https://github.com/zzyzzye/click/releases/download/v0.6.0/";
  return QJsonDocument(
             QJsonObject{
                 {"tag_name", "v0.6.0"},
                 {"assets",
                  QJsonArray{QJsonObject{{"name", name}, {"browser_download_url", prefix + name}},
                             QJsonObject{{"name", name + ".sha256"},
                                         {"browser_download_url", prefix + name + ".sha256"}}}}})
      .toJson();
}
void queueUpdate(Network& network, const QByteArray& data = QByteArray(4096, 'x'),
                 const QByteArray& checksum = {}) {
  network.responses.append({release()});
  network.responses.append({checksum.isEmpty()
                                ? QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex()
                                : checksum});
  network.responses.append({data});
}
QString lastAction(const QSignalSpy& spy) {
  return spy.isEmpty() ? QString() : spy.last()[0].toString();
}
UpdateDependencies dependencies(Network& network, const QString& directory) {
  UpdateDependencies d;
  d.supported = true;
  d.network = &network;
  d.installedDirectory = [directory] { return directory; };
  return d;
}
} // namespace

class UpdateControllerTests : public QObject {
  Q_OBJECT
private slots:
  void init() {
    QCoreApplication::setApplicationVersion("0.5.1");
  }
  void detectsNewerRelease();
  void rejectsInvalidOrOlderRelease();
  void downloadProgressAndExplicitRestart();
  void cancellationIgnoresLateRepliesAndRetries();
  void hashFailureNeverLaunches();
  void busyAndUnsavedCancellationRetainReadyPackage();
  void launchFailureAndPackageTampering();
  void portableBuildCannotInstall();
  void networkFailureAndInvalidMetadataRecover();
  void closingDuringVerificationCleansDownloadedPackage();
  void rejectsPreviewAndMismatchedAssets();
  void removedUpdateReturnsToVersionCheck();
};
void UpdateControllerTests::rejectsPreviewAndMismatchedAssets() {
  Network network;
  QTemporaryDir installed;
  UpdateController c([] { return true; }, dependencies(network, installed.path()));
  QSignalSpy actions(&c, &UpdateController::actionChanged);
  for (const QString& flag : {QString("draft"), QString("prerelease")}) {
    auto metadata = QJsonDocument::fromJson(release()).object();
    metadata.insert(flag, true);
    network.responses.append({QJsonDocument(metadata).toJson()});
    c.handleAction();
    c.handleAction();
    QTRY_COMPARE(lastAction(actions), QString("重新检查"));
  }
  QCOMPARE(network.requests.size(), 2);
  for (bool wrongName : {true, false}) {
    auto metadata = QJsonDocument::fromJson(release()).object();
    auto assets = metadata["assets"].toArray();
    auto asset = assets[0].toObject();
    asset.insert(wrongName ? "name" : "browser_download_url",
                 wrongName ? "OtherProduct" + suffix()
                           : asset["browser_download_url"].toString() + "?download=1");
    assets[0] = asset;
    metadata.insert("assets", assets);
    network.responses.append({QJsonDocument(metadata).toJson()});
    c.handleAction();
    QTRY_COMPARE(lastAction(actions), QString("重新检查"));
  }
  queueUpdate(network);
  c.handleAction();
  QTRY_COMPARE(lastAction(actions), QString("下载更新"));
}

void UpdateControllerTests::removedUpdateReturnsToVersionCheck() {
  for (bool missingChecksum : {true, false}) {
    Network network;
    QTemporaryDir installed;
    network.responses.append({release()});
    if (!missingChecksum)
      network.responses.append({QByteArray(64, '0')});
    network.responses.append({{}, QNetworkReply::ContentNotFoundError, 32, 404});
    UpdateController c([] { return true; }, dependencies(network, installed.path()));
    QSignalSpy actions(&c, &UpdateController::actionChanged),
        quit(&c, &UpdateController::quitRequested);
    c.handleAction();
    QTRY_COMPARE(lastAction(actions), QString("下载更新"));
    c.handleAction();
    QTRY_COMPARE(lastAction(actions), QString("重新检查"));
    QCOMPARE(quit.count(), 0);
    queueUpdate(network);
    c.handleAction();
    QTRY_COMPARE(lastAction(actions), QString("下载更新"));
    c.handleAction();
    QTRY_COMPARE(lastAction(actions), QString("重启并更新"));
  }
}
void UpdateControllerTests::detectsNewerRelease() {
  QVERIFY(isReleaseNewer("v0.5.2", "0.5.1"));
  QVERIFY(isReleaseNewer("0.6.0", "0.5.9"));
  QVERIFY(isReleaseNewer("1.0.0", "0.9.9"));
}
void UpdateControllerTests::rejectsInvalidOrOlderRelease() {
  for (const auto& v : {"v0.5.1", "0.5.0", "invalid", "v0.6.0-beta", "1.0", "9999999999.0.0"})
    QVERIFY(!isReleaseNewer(v, "0.5.1"));
}
void UpdateControllerTests::downloadProgressAndExplicitRestart() {
  Network network;
  QTemporaryDir installed;
  queueUpdate(network);
  auto d = dependencies(network, installed.path());
  int launches = 0;
  QString staging;
  d.launchInstaller = [&](const QString& path, const QStringList& args, QString*) {
    ++launches;
    staging = QFileInfo(path).dir().absolutePath();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.readAll() != QByteArray(4096, 'x'))
      return false;
    return args.contains("/ClickFlowUpdate=1") && args.contains("/SILENT") &&
           args.contains("/NORESTART") && args.contains("/NOCLOSEAPPLICATIONS") &&
           args.contains("/DIR=\"" + QDir::toNativeSeparators(installed.path()) + "\"");
  };
  UpdateController c([] { return true; }, d);
  QSignalSpy actions(&c, &UpdateController::actionChanged),
      progress(&c, &UpdateController::progressChanged);
  QSignalSpy quit(&c, &UpdateController::quitRequested);
  c.handleAction();
  c.handleAction();
  QCOMPARE(network.requests.size(), 1);
  QTRY_COMPARE(lastAction(actions), QString("下载更新"));
  c.handleAction();
  c.handleAction();
  QTRY_COMPARE(lastAction(actions), QString("重启并更新"));
  QCOMPARE(network.requests.size(), 3);
  QCOMPARE(launches, 0);
  QCOMPARE(quit.count(), 0);
  bool intermediate = false;
  for (const auto& args : progress)
    intermediate |= args[0].toInt() > 0 && args[0].toInt() < 100;
  QVERIFY(intermediate);
  QCOMPARE(progress.last()[0].toInt(), 100);
  c.handleAction();
  c.handleAction();
  QTRY_COMPARE(launches, 1);
  QCOMPARE(quit.count(), 1);
  // 真实助手接管清理；假安装器测试自行清理自己的临时数据。
  QVERIFY(QDir(staging).removeRecursively());
}
void UpdateControllerTests::cancellationIgnoresLateRepliesAndRetries() {
  Network network;
  QTemporaryDir installed;
  queueUpdate(network);
  UpdateController c([] { return true; }, dependencies(network, installed.path()));
  QSignalSpy actions(&c, &UpdateController::actionChanged),
      quit(&c, &UpdateController::quitRequested);
  c.handleAction();
  c.cancel();
  QTest::qWait(10);
  QCOMPARE(lastAction(actions), QString("检查更新"));
  network.responses.clear();
  queueUpdate(network);
  c.handleAction();
  QTRY_COMPARE(lastAction(actions), QString("下载更新"));
  bool cancelledDownload = false;
  connect(&c, &UpdateController::progressChanged, &c,
          [&](int, const QString& detail, bool cancellable) {
            if (!cancelledDownload && cancellable && detail.contains("MB")) {
              cancelledDownload = true;
              c.cancel();
            }
          });
  c.handleAction();
  QTRY_VERIFY(cancelledDownload);
  QCOMPARE(network.requests.size(), 4);
  QTest::qWait(10);
  QCOMPARE(lastAction(actions), QString("重新下载"));
  const QByteArray data(16, 'y');
  network.responses.append({QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex()});
  network.responses.append({data});
  c.handleAction();
  QTRY_COMPARE(lastAction(actions), QString("重启并更新"));
  QCOMPARE(quit.count(), 0);
}
void UpdateControllerTests::hashFailureNeverLaunches() {
  Network network;
  QTemporaryDir installed;
  queueUpdate(network, "payload", QByteArray(64, '0'));
  UpdateController c([] { return true; }, dependencies(network, installed.path()));
  QSignalSpy actions(&c, &UpdateController::actionChanged),
      statuses(&c, &UpdateController::statusChanged);
  QSignalSpy quit(&c, &UpdateController::quitRequested);
  c.handleAction();
  QTRY_COMPARE(lastAction(actions), QString("下载更新"));
  c.handleAction();
  QTRY_COMPARE(lastAction(actions), QString("重新下载"));
  QVERIFY(statuses.last()[0].toString().contains("校验失败"));
  QCOMPARE(quit.count(), 0);
}
void UpdateControllerTests::busyAndUnsavedCancellationRetainReadyPackage() {
  Network network;
  QTemporaryDir installed;
  queueUpdate(network);
  bool idle = false;
  int preparations = 0;
  UpdateController c([&] { return idle; }, dependencies(network, installed.path()));
  c.setRestartPreparation([&] {
    ++preparations;
    return false;
  });
  QSignalSpy actions(&c, &UpdateController::actionChanged),
      quit(&c, &UpdateController::quitRequested);
  c.handleAction();
  QTRY_COMPARE(lastAction(actions), QString("下载更新"));
  c.handleAction();
  QTRY_COMPARE(lastAction(actions), QString("重启并更新"));
  c.handleAction();
  QCOMPARE(preparations, 0);
  idle = true;
  c.handleAction();
  QTRY_COMPARE(preparations, 1);
  QCOMPARE(lastAction(actions), QString("重启并更新"));
  QCOMPARE(quit.count(), 0);
}
void UpdateControllerTests::launchFailureAndPackageTampering() {
  Network network;
  QTemporaryDir installed;
  queueUpdate(network);
  auto d = dependencies(network, installed.path());
  int launches = 0;
  QString path;
  d.launchInstaller = [&](const QString& p, const QStringList&, QString* error) {
    path = p;
    ++launches;
    *error = "模拟助手启动失败";
    return false;
  };
  UpdateController c([] { return true; }, d);
  QSignalSpy actions(&c, &UpdateController::actionChanged),
      quit(&c, &UpdateController::quitRequested);
  c.handleAction();
  QTRY_COMPARE(lastAction(actions), QString("下载更新"));
  c.handleAction();
  QTRY_COMPARE(lastAction(actions), QString("重启并更新"));
  c.handleAction();
  QTRY_COMPARE(launches, 1);
  QCOMPARE(quit.count(), 0);
  QCOMPARE(lastAction(actions), QString("重启并更新"));
  QFile package(path);
  QVERIFY(package.open(QIODevice::WriteOnly));
  package.write("changed");
  package.close();
  c.handleAction();
  QCOMPARE(launches, 1);
  QTRY_COMPARE(lastAction(actions), QString("重新下载"));
  QVERIFY(!QFile::exists(path));
}
void UpdateControllerTests::portableBuildCannotInstall() {
  Network network;
  network.responses.append({release()});
  UpdateController c([] { return true; }, dependencies(network, {}));
  QSignalSpy actions(&c, &UpdateController::actionChanged),
      statuses(&c, &UpdateController::statusChanged);
  c.handleAction();
  QTRY_COMPARE(lastAction(actions), QString("下载更新"));
  c.handleAction();
  QCOMPARE(network.requests.size(), 1);
  QVERIFY(statuses.last()[0].toString().contains("不是已安装版本"));
}
void UpdateControllerTests::networkFailureAndInvalidMetadataRecover() {
  Network network;
  QTemporaryDir installed;
  UpdateController c([] { return true; }, dependencies(network, installed.path()));
  QSignalSpy actions(&c, &UpdateController::actionChanged);
  network.responses.append({{}, QNetworkReply::TimeoutError});
  c.handleAction();
  QTRY_COMPARE(lastAction(actions), QString("重新检查"));
  network.responses.append({"invalid-json"});
  c.handleAction();
  QTRY_COMPARE(lastAction(actions), QString("重新检查"));
  network.responses.append({"{\"tag_name\":\"v0.6.0\",\"assets\":[]}"});
  c.handleAction();
  QTRY_COMPARE(lastAction(actions), QString("重新检查"));
  queueUpdate(network);
  c.handleAction();
  QTRY_COMPARE(lastAction(actions), QString("下载更新"));
}

void UpdateControllerTests::closingDuringVerificationCleansDownloadedPackage() {
  Network network;
  QTemporaryDir installed;
  queueUpdate(network);
  auto d = dependencies(network, installed.path());
  QString path;
  d.launchInstaller = [&](const QString& p, const QStringList&, QString*) {
    path = p;
    return false;
  };
  auto c = std::make_unique<UpdateController>([] { return true; }, d);
  QSignalSpy actions(c.get(), &UpdateController::actionChanged);
  c->handleAction();
  QTRY_COMPARE(lastAction(actions), QString("下载更新"));
  c->handleAction();
  QTRY_COMPARE(lastAction(actions), QString("重启并更新"));
  c->handleAction();
  QTRY_VERIFY(!path.isEmpty());
  QVERIFY(QFile::exists(path));
  c->handleAction();
  c.reset();
  QVERIFY(!QFile::exists(path));
}

QTEST_GUILESS_MAIN(UpdateControllerTests)
#include "UpdateControllerTests.moc"
