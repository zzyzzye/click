#pragma once

#include <QCryptographicHash>
#include <QObject>
#include <QPointer>
#include <QUrl>
#include <functional>
#include <memory>

class QNetworkAccessManager;
class QNetworkReply;
class QSaveFile;
class QTemporaryDir;
class QTimer;

// 原生安装与网络依赖可替换，测试不下载真实安装包，也不启动安装程序。
struct UpdateDependencies {
  QNetworkAccessManager* network = nullptr;
#if defined(Q_OS_WIN)
  bool supported = true;
#else
  bool supported = false;
#endif
  std::function<QString()> installedDirectory;
  std::function<bool(const QString&, const QStringList&, QString*)> launchInstaller;
};

class UpdateController final : public QObject {
  Q_OBJECT
public:
  using ApplicationIdleProbe = std::function<bool()>;
  explicit UpdateController(ApplicationIdleProbe idle, QObject* parent = nullptr);
  UpdateController(ApplicationIdleProbe idle, UpdateDependencies dependencies,
                   QObject* parent = nullptr);
  ~UpdateController() override;
  void setRestartPreparation(std::function<bool()> preparation);

public slots:
  void handleAction();
  void cancel();

signals:
  void statusChanged(const QString& status);
  void actionChanged(const QString& text, bool enabled);
  // -1 表示不可量化阶段；百分比仅表示下载，不能代表安装进度。
  void progressChanged(int percent, const QString& detail, bool cancellable);
  void quitRequested();

private:
  enum class State { Idle, Checking, Available, Downloading, Ready, Installing };
  void checkForUpdates();
  void checkFromReleasePage();
  void markAvailable(const QString& tag, const QUrl& installer, const QUrl& checksum);
  void downloadUpdate();
  void downloadInstaller();
  void restartAndUpdate();
  void launchReadyUpdate(const QString& directory);
  void fail(const QString& message);
  void clearDownload();
  QNetworkReply* request(const QUrl& url, qint64 limit = 2 * 1024 * 1024);
  bool consumeInstaller();

  QNetworkAccessManager* network_;
  UpdateDependencies dependencies_;
  ApplicationIdleProbe idle_;
  std::function<bool()> prepareRestart_;
  State state_ = State::Idle;
  QPointer<QNetworkReply> reply_;
  QPointer<QTimer> verificationTimer_;
  quint64 generation_ = 0;
  QString version_;
  QUrl installerUrl_, checksumUrl_;
  QByteArray expectedHash_;
  QCryptographicHash hash_{QCryptographicHash::Sha256};
  std::unique_ptr<QTemporaryDir> temporary_;
  std::unique_ptr<QSaveFile> file_;
  QString installerPath_;
  qint64 received_ = 0;
};

bool isReleaseNewer(const QString& tag, const QString& currentVersion);
