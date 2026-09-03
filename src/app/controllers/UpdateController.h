#pragma once

#include <QObject>
#include <QUrl>

#include <functional>

class QNetworkAccessManager;

// 在线更新协调器：负责版本检查、安装包下载和校验，不依赖具体页面控件。
class UpdateController final : public QObject {
  Q_OBJECT

 public:
  using ApplicationIdleProbe = std::function<bool()>;

  explicit UpdateController(ApplicationIdleProbe applicationIdleProbe,
                            QObject* parent = nullptr);

 public slots:
  void handleAction();

 signals:
  void statusChanged(const QString& status);
  void actionChanged(const QString& text, bool enabled);
  void quitRequested();

 private:
  void checkForUpdates();
  void checkForUpdatesFromReleasePage();
  void downloadUpdate();
  void resetAction(const QString& status,
                   const QString& actionText = QStringLiteral("重新检查"));
  void markUpdateAvailable(const QString& tag, const QUrl& installerUrl,
                           const QUrl& checksumUrl);

  QNetworkAccessManager* network_ = nullptr;
  ApplicationIdleProbe applicationIdleProbe_;
  bool updateReady_ = false;
  QString updateVersion_;
  QUrl installerUrl_;
  QUrl checksumUrl_;
};

// 独立版本比较函数，便于无网络单元测试。
bool isReleaseNewer(const QString& tag, const QString& currentVersion);
