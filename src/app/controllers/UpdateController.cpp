#include "app/controllers/UpdateController.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <QVersionNumber>

namespace {
constexpr auto api = "https://api.github.com/repos/zzyzzye/click/releases/latest";
constexpr auto releasePage = "https://github.com/zzyzzye/click/releases/latest";
constexpr auto assetPrefix = "https://github.com/zzyzzye/click/releases/download/";
constexpr qint64 maxPackageSize = 512LL * 1024 * 1024;

QString suffix() {
#if defined(Q_PROCESSOR_ARM_64)
  return "-win-arm64-setup.exe";
#else
  return "-win64-setup.exe";
#endif
}
bool validVersion(const QString& v) {
  static const QRegularExpression pattern("^v?[0-9]{1,8}\\.[0-9]{1,8}\\.[0-9]{1,8}$");
  return pattern.match(v).hasMatch();
}
QString installedDirectory() {
#if defined(Q_OS_WIN)
  QSettings registration(
      "HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\"
      "{C2A5B39A-329F-4EF3-8E1F-9A2C2C93281C}_is1",
      QSettings::Registry64Format);
  const QString registered = registration.value("InstallLocation").toString();
  const QString actual = QFileInfo(QCoreApplication::applicationDirPath()).canonicalFilePath();
  const QString known = QFileInfo(registered).canonicalFilePath();
  if (!known.isEmpty() && known.compare(actual, Qt::CaseInsensitive) == 0)
    return actual;
#endif
  return {};
}
bool launchInstaller(const QString& path, const QStringList& args, QString* error) {
#if defined(Q_OS_WIN)
  // 独立、无 Qt 依赖的助手复制到临时目录运行，不占用待覆盖的程序文件。
  const QString helper = QFileInfo(path).dir().filePath("ClickFlowUpdateHelper.exe");
  const QString source =
      QDir(QCoreApplication::applicationDirPath()).filePath("ClickFlowUpdateHelper.exe");
  if (QFile::exists(helper))
    QFile::remove(helper);
  if (!QFile::copy(source, helper) ||
      !QProcess::startDetached(helper,
                               {path, QCoreApplication::applicationFilePath(), args.join(' '),
                                QString::number(QCoreApplication::applicationPid())})) {
    *error = "无法启动更新助手，当前版本未改变；请重试或手动更新。";
    return false;
  }
  return true;
#else
  Q_UNUSED(path)
  Q_UNUSED(args)
  *error = "当前平台暂不支持覆盖更新。";
  return false;
#endif
}
} // namespace

bool isReleaseNewer(const QString& tag, const QString& current) {
  if (!validVersion(tag) || !validVersion(current))
    return false;
  auto parse = [](QString v) {
    if (v.startsWith('v'))
      v.remove(0, 1);
    return QVersionNumber::fromString(v);
  };
  return QVersionNumber::compare(parse(tag), parse(current)) > 0;
}

UpdateController::UpdateController(ApplicationIdleProbe idle, QObject* parent)
    : UpdateController(std::move(idle), UpdateDependencies{}, parent) {}
UpdateController::UpdateController(ApplicationIdleProbe idle, UpdateDependencies dependencies,
                                   QObject* parent)
    : QObject(parent),
      network_(dependencies.network ? dependencies.network : new QNetworkAccessManager(this)),
      dependencies_(std::move(dependencies)), idle_(std::move(idle)) {
  if (!dependencies_.installedDirectory)
    dependencies_.installedDirectory = installedDirectory;
  if (!dependencies_.launchInstaller)
    dependencies_.launchInstaller = launchInstaller;
}
UpdateController::~UpdateController() {
  ++generation_;
  if (reply_) {
    reply_->disconnect(this);
    reply_->abort();
  }
  // 先关闭复查文件，再销毁临时目录；Windows 不允许删除仍打开的文件。
  delete verificationTimer_.data();
}
void UpdateController::setRestartPreparation(std::function<bool()> preparation) {
  prepareRestart_ = std::move(preparation);
}
QNetworkReply* UpdateController::request(const QUrl& url, qint64 limit) {
  QNetworkRequest req(url);
  req.setHeader(QNetworkRequest::UserAgentHeader,
                "ClickFlow/" + QCoreApplication::applicationVersion());
  req.setTransferTimeout(30000);
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                   QNetworkRequest::NoLessSafeRedirectPolicy);
  reply_ = network_->get(req);
  const auto token = generation_;
  auto* r = reply_.data();
  connect(r, &QNetworkReply::readyRead, this, [this, r, token, limit] {
    if (token == generation_ && r->bytesAvailable() > limit)
      fail("服务器返回的数据过大，已取消，请稍后重试。");
  });
  return reply_;
}
void UpdateController::handleAction() {
  switch (state_) {
  case State::Idle:
    checkForUpdates();
    break;
  case State::Available:
    downloadUpdate();
    break;
  case State::Ready:
    restartAndUpdate();
    break;
  default:
    break;
  }
}
void UpdateController::checkForUpdates() {
  if (state_ != State::Idle)
    return;
  if (!dependencies_.supported) {
    QDesktopServices::openUrl(QUrl(releasePage));
    emit statusChanged("更新：请在 Release 页面下载当前平台的新版本。");
    return;
  }
  state_ = State::Checking;
  emit statusChanged("正在检查新版本…");
  emit actionChanged("检查中…", false);
  emit progressChanged(-1, "正在连接 GitHub", true);
  const auto token = ++generation_;
  auto* r = request(QUrl(api));
  connect(r, &QNetworkReply::finished, this, [this, r, token] {
    r->deleteLater();
    if (token != generation_)
      return;
    reply_.clear();
    const auto body = r->readAll();
    if (r->error() != QNetworkReply::NoError) {
      if (r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 403) {
        checkFromReleasePage();
        return;
      }
      fail("检查失败，请检查网络后重新检查。");
      return;
    }
    if (body.size() > 2 * 1024 * 1024) {
      fail("版本信息过大，请稍后重试。");
      return;
    }
    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson(body, &parseError);
    const auto release = doc.object();
    const QString tag = release.value("tag_name").toString();
    if (parseError.error != QJsonParseError::NoError || !validVersion(tag)) {
      fail("版本信息无效，请稍后重新检查。");
      return;
    }
    if (release.value("draft").toBool() || release.value("prerelease").toBool()) {
      fail("该版本尚未正式发布，请稍后重新检查。");
      return;
    }
    if (!isReleaseNewer(tag, QCoreApplication::applicationVersion())) {
      state_ = State::Idle;
      emit statusChanged("当前已是最新版本（" + QCoreApplication::applicationVersion() + "）。");
      emit actionChanged("检查更新", true);
      emit progressChanged(0, {}, false);
      return;
    }
    QUrl installer, checksum;
    const QString version = tag.startsWith('v') ? tag.mid(1) : tag;
    const QString expectedName = "ClickFlow-" + version + suffix();
    for (const auto& value : release.value("assets").toArray()) {
      const auto asset = value.toObject();
      const QString name = asset.value("name").toString();
      const QUrl url(asset.value("browser_download_url").toString());
      if (name == expectedName)
        installer = url;
      if (name == expectedName + ".sha256")
        checksum = url;
    }
    markAvailable(tag, installer, checksum);
  });
}
void UpdateController::checkFromReleasePage() {
  const auto token = generation_;
  auto* r = request(QUrl(releasePage));
  connect(r, &QNetworkReply::finished, this, [this, r, token] {
    r->deleteLater();
    if (token != generation_)
      return;
    reply_.clear();
    const QString tag = r->url().path().section('/', -1);
    if (r->error() != QNetworkReply::NoError || !validVersion(tag)) {
      fail("GitHub 暂时限流或没有可用的新版本，请稍后重新检查。");
      return;
    }
    if (!isReleaseNewer(tag, QCoreApplication::applicationVersion())) {
      state_ = State::Idle;
      emit statusChanged("当前已是最新版本（" + QCoreApplication::applicationVersion() + "）。");
      emit actionChanged("检查更新", true);
      emit progressChanged(0, {}, false);
      return;
    }
    const QString version = tag.startsWith('v') ? tag.mid(1) : tag;
    const QUrl installer(QString(assetPrefix) + tag + "/ClickFlow-" + version + suffix());
    markAvailable(tag, installer, QUrl(installer.toString() + ".sha256"));
  });
}
void UpdateController::markAvailable(const QString& tag, const QUrl& installer,
                                     const QUrl& checksum) {
  const QString version = tag.startsWith('v') ? tag.mid(1) : tag;
  const QUrl expected(QString(assetPrefix) + tag + "/ClickFlow-" + version + suffix());
  if (installer != expected || checksum != QUrl(expected.toString() + ".sha256")) {
    fail("找到新版本，但缺少匹配架构的安装包或校验文件，请稍后重新检查。");
    return;
  }
  version_ = tag;
  installerUrl_ = installer;
  checksumUrl_ = checksum;
  state_ = State::Available;
  emit statusChanged("发现新版本 " + version_ + "，覆盖更新将保留配置、录制和流程。");
  emit actionChanged("下载更新", true);
  emit progressChanged(0, {}, false);
}
void UpdateController::downloadUpdate() {
  if (dependencies_.installedDirectory().isEmpty()) {
    emit statusChanged("当前运行的不是已安装版本，请先使用安装包安装 ClickFlow，再使用覆盖更新。");
    return;
  }
  state_ = State::Downloading;
  emit actionChanged("下载中…", false);
  emit statusChanged("正在准备下载 " + version_ + "…");
  emit progressChanged(-1, "正在获取校验信息", true);
  const auto token = ++generation_;
  auto* r = request(checksumUrl_, 65536);
  connect(r, &QNetworkReply::finished, this, [this, r, token] {
    r->deleteLater();
    if (token != generation_)
      return;
    reply_.clear();
    const QByteArray data = r->readAll();
    expectedHash_ = data.simplified().split(' ').value(0).toLower();
    static const QRegularExpression digest("^[0-9a-f]{64}$");
    const int status = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status == 404 || status == 410) {
      installerUrl_ = QUrl{};
      checksumUrl_ = QUrl{};
      fail("更新文件已移除或尚未上传完成，请重新检查版本。");
      return;
    }
    if (r->error() != QNetworkReply::NoError || data.size() > 65536 ||
        !digest.match(QString::fromLatin1(expectedHash_)).hasMatch()) {
      fail("无法取得有效校验信息，请重新下载。");
      return;
    }
    downloadInstaller();
  });
}
void UpdateController::downloadInstaller() {
  temporary_ = std::make_unique<QTemporaryDir>(QDir::tempPath() + "/ClickFlow-update-XXXXXX");
  if (!temporary_->isValid()) {
    fail("无法创建更新目录，请检查磁盘空间与权限。");
    return;
  }
  installerPath_ = temporary_->filePath("ClickFlow-update.exe");
  file_ = std::make_unique<QSaveFile>(installerPath_);
  if (!file_->open(QIODevice::WriteOnly)) {
    fail("无法写入更新文件，请检查磁盘空间与权限。");
    return;
  }
  hash_.reset();
  received_ = 0;
  emit statusChanged("正在下载 " + version_ + "，下载完成后可选择重启更新。");
  emit progressChanged(-1, "等待安装包数据…", true);
  const auto token = generation_;
  auto* r = request(installerUrl_, maxPackageSize);
  connect(r, &QNetworkReply::readyRead, this, [this, token] {
    if (token == generation_)
      consumeInstaller();
  });
  connect(r, &QNetworkReply::downloadProgress, this, [this, token](qint64 done, qint64 total) {
    if (token != generation_)
      return;
    if (total > maxPackageSize) {
      fail("更新包过大，已取消下载。");
      return;
    }
    const QString amount = QString::number(done / 1048576.0, 'f', 1) + " MB";
    const QString detail = total > 0
                               ? amount + " / " + QString::number(total / 1048576.0, 'f', 1) + " MB"
                               : "已下载 " + amount;
    emit progressChanged(total > 0 ? int(qMin<qint64>(99, done * 100 / total)) : -1, detail, true);
  });
  connect(r, &QNetworkReply::finished, this, [this, r, token] {
    r->deleteLater();
    if (token != generation_)
      return;
    if (!consumeInstaller())
      return;
    reply_.clear();
    const int status = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status == 404 || status == 410) {
      installerUrl_ = QUrl{};
      checksumUrl_ = QUrl{};
      fail("更新文件已移除或尚未上传完成，请重新检查版本。");
      return;
    }
    if (r->error() != QNetworkReply::NoError || received_ == 0) {
      fail("下载失败，当前版本未改变；请重新下载。");
      return;
    }
    emit statusChanged("下载完成，正在校验…");
    emit progressChanged(-1, "正在校验完整性", false);
    if (hash_.result().toHex() != expectedHash_) {
      fail("校验失败，已删除更新包；请重新下载。");
      return;
    }
    if (!file_->commit()) {
      fail("无法保存更新包，请检查磁盘空间后重试。");
      return;
    }
    file_.reset();
    state_ = State::Ready;
    emit statusChanged(version_ + " 已准备好。重启后覆盖原版本，配置、录制和流程将保留。");
    emit progressChanged(100, "下载与校验已完成", false);
    emit actionChanged("重启并更新", true);
  });
}
bool UpdateController::consumeInstaller() {
  if (!reply_ || !file_)
    return false;
  while (reply_->bytesAvailable() > 0) {
    const auto chunk = reply_->read(65536);
    received_ += chunk.size();
    if (received_ > maxPackageSize) {
      fail("更新包过大，已取消下载。");
      return false;
    }
    if (file_->write(chunk) != chunk.size()) {
      fail("更新文件写入失败，请检查磁盘空间后重试。");
      return false;
    }
    hash_.addData(chunk);
  }
  return true;
}
void UpdateController::restartAndUpdate() {
  if (idle_ && !idle_()) {
    emit statusChanged("请先停止当前自动化任务，再重启更新。");
    return;
  }
  const QString directory = dependencies_.installedDirectory();
  if (directory.isEmpty() || directory.contains('"')) {
    emit statusChanged("无法确认原安装位置，请从 Release 页面手动更新。");
    return;
  }
  state_ = State::Installing;
  emit actionChanged("准备重启…", false);
  emit progressChanged(-1, "正在复查更新文件", false);
  // 分批读取磁盘，避免较大安装包校验阻塞停止按钮和窗口响应。
  struct Verification {
    QFile file;
    QCryptographicHash hash{QCryptographicHash::Sha256};
  };
  auto verification = std::make_shared<Verification>();
  verification->file.setFileName(installerPath_);
  if (!verification->file.open(QIODevice::ReadOnly)) {
    fail("更新包不可读取，请重新下载。");
    return;
  }
  auto* timer = new QTimer(this);
  verificationTimer_ = timer;
  timer->setInterval(0);
  connect(timer, &QTimer::timeout, this, [this, timer, verification, directory] {
    const auto bytes = verification->file.read(256 * 1024);
    if (!bytes.isEmpty()) {
      verification->hash.addData(bytes);
      return;
    }
    timer->stop();
    timer->deleteLater();
    const bool valid = verification->file.error() == QFile::NoError &&
                       verification->hash.result().toHex() == expectedHash_;
    verification->file.close();
    if (!valid) {
      fail("更新包已变化或不可读取，请重新下载。");
      return;
    }
    launchReadyUpdate(directory);
  });
  timer->start();
}
void UpdateController::launchReadyUpdate(const QString& directory) {
  // 校验期间仍能编辑；交接前才处理草稿，取消确认不会退出当前软件。
  if ((idle_ && !idle_()) || (prepareRestart_ && !prepareRestart_()) || (idle_ && !idle_())) {
    state_ = State::Ready;
    emit actionChanged("重启并更新", true);
    emit progressChanged(100, "下载与校验已完成", false);
    emit statusChanged("已取消重启或任务正在运行，更新包仍保留，可稍后更新。");
    return;
  }
  const QStringList args{"/SP-",
                         "/SILENT",
                         "/NORESTART",
                         "/NOFORCECLOSEAPPLICATIONS",
                         "/NOCLOSEAPPLICATIONS",
                         "/ClickFlowUpdate=1",
                         "/ClickFlowParentPid=" +
                             QString::number(QCoreApplication::applicationPid()),
                         "/DIR=\"" + QDir::toNativeSeparators(directory) + "\""};
  QString error;
  if (!dependencies_.launchInstaller(installerPath_, args, &error)) {
    state_ = State::Ready;
    emit actionChanged("重启并更新", true);
    emit progressChanged(100, "下载与校验已完成", false);
    emit statusChanged(error.isEmpty() ? "无法启动更新，请重试。" : error);
    return;
  }
  // 安装器仍需读取包；交接成功后不由本进程删除临时目录。
  temporary_->setAutoRemove(false);
  emit statusChanged("正在退出并覆盖更新，完成后将重新打开 ClickFlow…");
  emit quitRequested();
}
void UpdateController::clearDownload() {
  file_.reset();
  temporary_.reset();
  installerPath_.clear();
  received_ = 0;
}
void UpdateController::cancel() {
  if (state_ != State::Checking && state_ != State::Downloading)
    return;
  ++generation_;
  if (reply_) {
    auto r = reply_;
    reply_.clear();
    r->abort();
    r->deleteLater();
  }
  clearDownload();
  state_ = installerUrl_.isEmpty() ? State::Idle : State::Available;
  emit statusChanged("已取消，当前版本未改变。");
  emit progressChanged(0, {}, false);
  emit actionChanged(state_ == State::Available ? "重新下载" : "检查更新", true);
}
void UpdateController::fail(const QString& message) {
  ++generation_;
  if (reply_) {
    auto r = reply_;
    reply_.clear();
    r->abort();
    r->deleteLater();
  }
  clearDownload();
  state_ = installerUrl_.isEmpty() ? State::Idle : State::Available;
  emit statusChanged(message);
  emit progressChanged(0, {}, false);
  emit actionChanged(state_ == State::Available ? "重新下载" : "重新检查", true);
}
