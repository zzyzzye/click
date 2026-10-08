#include "app/controllers/UpdateController.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>

namespace {
constexpr auto kLatestReleaseApi =
    "https://api.github.com/repos/zzyzzye/click/releases/latest";
constexpr auto kLatestReleasePage =
    "https://github.com/zzyzzye/click/releases/latest";

QString userAgent() {
  return "ClickFlow/" + QCoreApplication::applicationVersion();
}

QNetworkRequest createRequest(const QUrl& url) {
  QNetworkRequest request(url);
  request.setHeader(QNetworkRequest::UserAgentHeader, userAgent());
  return request;
}
}  // namespace

bool isReleaseNewer(const QString& tag, const QString& currentVersion) {
  const QString latest = tag.startsWith('v') ? tag.mid(1) : tag;
  const QStringList latestParts = latest.split('.');
  const QStringList currentParts = currentVersion.split('.');
  for (int i = 0; i < 3; ++i) {
    const int candidate =
        i < latestParts.size() ? latestParts[i].toInt() : 0;
    const int current =
        i < currentParts.size() ? currentParts[i].toInt() : 0;
    if (candidate != current) return candidate > current;
  }
  return false;
}

UpdateController::UpdateController(ApplicationIdleProbe applicationIdleProbe,
                                   QObject* parent)
    : QObject(parent),
      network_(new QNetworkAccessManager(this)),
      applicationIdleProbe_(std::move(applicationIdleProbe)) {}

void UpdateController::handleAction() {
  if (updateReady_) {
    downloadUpdate();
  } else {
    checkForUpdates();
  }
}

void UpdateController::checkForUpdates() {
  emit statusChanged("更新：正在检查…");
  emit actionChanged("检查中…", false);

  auto* reply = network_->get(createRequest(QUrl(kLatestReleaseApi)));
  connect(reply, &QNetworkReply::finished, this, [this, reply] {
    const QByteArray body = reply->readAll();
    const auto error = reply->error();
    const int statusCode =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString errorText = reply->errorString();
    reply->deleteLater();

    if (error != QNetworkReply::NoError) {
      if (statusCode == 403 || error == QNetworkReply::ContentAccessDenied) {
        checkForUpdatesFromReleasePage();
        return;
      }
      resetAction(
          QString("更新：检查失败（%1），请检查网络连接。").arg(errorText));
      return;
    }

    const QJsonObject release = QJsonDocument::fromJson(body).object();
    const QString tag = release.value("tag_name").toString();
    if (tag.isEmpty() ||
        !isReleaseNewer(tag, QCoreApplication::applicationVersion())) {
      resetAction(QString("更新：当前已是最新版本（%1）。")
                      .arg(QCoreApplication::applicationVersion()),
                  "检查更新");
      return;
    }

    QUrl installerUrl;
    QUrl checksumUrl;
    for (const auto& value : release.value("assets").toArray()) {
      const auto asset = value.toObject();
      const QString name = asset.value("name").toString();
      const QUrl url(asset.value("browser_download_url").toString());
      if (name.endsWith("-win64-setup.exe")) installerUrl = url;
      if (name.endsWith("-win64-setup.exe.sha256")) checksumUrl = url;
    }
    if (!installerUrl.isValid() || !checksumUrl.isValid()) {
      resetAction("更新：找到新版本，但没有找到 Windows 安装包。",
                  "检查更新");
      return;
    }
    markUpdateAvailable(tag, installerUrl, checksumUrl);
  });
}

void UpdateController::checkForUpdatesFromReleasePage() {
  emit statusChanged("更新：正在通过 Release 页面重试…");

  auto* reply = network_->get(createRequest(QUrl(kLatestReleasePage)));
  connect(reply, &QNetworkReply::finished, this, [this, reply] {
    const auto error = reply->error();
    const QString finalUrl = reply->url().toString();
    reply->deleteLater();

    const QRegularExpression releasePattern(
        "/releases/tag/(v[0-9]+\\.[0-9]+\\.[0-9]+)$");
    const QRegularExpressionMatch match = releasePattern.match(finalUrl);
    if (error != QNetworkReply::NoError || !match.hasMatch()) {
      resetAction("更新：GitHub 当前不可达或暂时限流。");
      return;
    }

    const QString tag = match.captured(1);
    if (!isReleaseNewer(tag, QCoreApplication::applicationVersion())) {
      resetAction(QString("更新：当前已是最新版本（%1）。")
                      .arg(QCoreApplication::applicationVersion()),
                  "检查更新");
      return;
    }

    const QString version = tag.mid(1);
    const QString base =
        QString("https://github.com/zzyzzye/click/releases/download/%1/")
            .arg(tag);
    const QUrl installerUrl(base + "ClickFlow-" + version +
                            "-win64-setup.exe");
    markUpdateAvailable(tag, installerUrl,
                        QUrl(installerUrl.toString() + ".sha256"));
  });
}

void UpdateController::downloadUpdate() {
#if !defined(Q_OS_WIN)
  emit statusChanged("更新：当前平台暂不支持自动覆盖更新。");
  return;
#else
  if (applicationIdleProbe_ && !applicationIdleProbe_()) {
    emit statusChanged("更新：请先停止当前运行任务。");
    return;
  }

  emit statusChanged(QString("更新：正在下载 %1…").arg(updateVersion_));
  emit actionChanged("下载中…", false);
  auto* reply = network_->get(createRequest(installerUrl_));
  connect(reply, &QNetworkReply::finished, this, [this, reply] {
    const QByteArray installerData = reply->readAll();
    const auto error = reply->error();
    reply->deleteLater();
    if (error != QNetworkReply::NoError || installerData.isEmpty()) {
      resetAction("更新：安装包下载失败。");
      return;
    }

    const QString installerPath = QDir::temp().filePath(
        QString("ClickFlow-%1-setup.exe").arg(updateVersion_));
    QFile installerFile(installerPath);
    if (!installerFile.open(QIODevice::WriteOnly) ||
        installerFile.write(installerData) != installerData.size()) {
      resetAction("更新：无法保存安装包。");
      return;
    }
    installerFile.close();

    auto* checksumReply = network_->get(createRequest(checksumUrl_));
    connect(checksumReply, &QNetworkReply::finished, this,
            [this, checksumReply, installerPath, installerData] {
              const QByteArray checksumData = checksumReply->readAll();
              const auto error = checksumReply->error();
              checksumReply->deleteLater();
              const QString expected =
                  QString::fromUtf8(checksumData)
                      .simplified()
                      .section(' ', 0, 0)
                      .toLower();
              const QString actual = QString::fromLatin1(
                  QCryptographicHash::hash(installerData,
                                           QCryptographicHash::Sha256)
                      .toHex());
              if (error != QNetworkReply::NoError || expected.size() != 64 ||
                  expected != actual) {
                QFile::remove(installerPath);
                resetAction("更新：安装包校验失败，已取消安装。");
                return;
              }

              emit statusChanged("更新：校验通过，正在启动安装程序…");
              updateReady_ = false;
              if (!QProcess::startDetached(installerPath, {})) {
                resetAction("更新：无法启动安装程序。");
                return;
              }
              emit quitRequested();
            });
  });
#endif
}

void UpdateController::resetAction(const QString& status,
                                   const QString& actionText) {
  updateReady_ = false;
  updateVersion_.clear();
  installerUrl_ = QUrl{};
  checksumUrl_ = QUrl{};
  emit statusChanged(status);
  emit actionChanged(actionText, true);
}

void UpdateController::markUpdateAvailable(const QString& tag,
                                           const QUrl& installerUrl,
                                           const QUrl& checksumUrl) {
  updateVersion_ = tag;
  installerUrl_ = installerUrl;
  checksumUrl_ = checksumUrl;
  updateReady_ = true;
  emit statusChanged(QString("更新：发现新版本 %1。").arg(tag));
  emit actionChanged("下载并安装", true);
}
