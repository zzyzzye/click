#include "platform/windows/WindowsWindowService.h"

#include <QFileInfo>

namespace {

void setError(QString* error, const QString& message) {
  if (error) *error = message;
}

bool exactIdentity(const WindowsNativeWindow& candidate,
                   const WindowTarget& target) {
  return candidate.executablePath.compare(target.executablePath,
                                          Qt::CaseInsensitive) == 0 &&
         candidate.className == target.className && candidate.title == target.title;
}

bool stableIdentity(const WindowsNativeWindow& candidate,
                    const WindowTarget& target) {
  return candidate.executablePath.compare(target.executablePath,
                                          Qt::CaseInsensitive) == 0 &&
         candidate.className == target.className;
}

}  // namespace

WindowsWindowService::WindowsWindowService(std::unique_ptr<WindowsWindowApi> api,
                                           quintptr excludedWindow)
    : api_(std::move(api)), excludedWindow_(excludedWindow) {}

QVector<WindowTarget> WindowsWindowService::availableWindows() const {
  QVector<WindowTarget> result;
  if (!api_) return result;
  for (const auto& window : api_->enumerateWindows()) {
    if (eligible(window)) result.append(toTarget(window));
  }
  return result;
}

std::optional<WindowTarget> WindowsWindowService::windowAt(
    const QPoint& screenPoint) const {
  if (!api_) return std::nullopt;
  const quintptr id = api_->rootWindowAt(screenPoint);
  const auto info = api_->windowInfo(id);
  if (!info || !eligible(*info)) return std::nullopt;
  return toTarget(*info);
}

std::optional<WindowTarget> WindowsWindowService::resolve(const WindowTarget& target,
                                                          QString* error) const {
  if (!api_) {
    setError(error, "窗口服务不可用。");
    return std::nullopt;
  }

  if (target.nativeId && api_->isWindow(target.nativeId)) {
    const auto info = api_->windowInfo(target.nativeId);
    if (info && eligible(*info) && exactIdentity(*info, target)) return toTarget(*info);
  }

  QVector<WindowsNativeWindow> exact;
  QVector<WindowsNativeWindow> stable;
  for (const auto& candidate : api_->enumerateWindows()) {
    if (!eligible(candidate)) continue;
    if (exactIdentity(candidate, target)) exact.append(candidate);
    if (stableIdentity(candidate, target)) stable.append(candidate);
  }
  if (exact.size() == 1) return toTarget(exact.first());
  if (exact.size() > 1 || stable.size() > 1) {
    setError(error, "找到多个匹配窗口，请重新选择目标窗口。");
    return std::nullopt;
  }
  if (stable.size() == 1) return toTarget(stable.first());
  setError(error, "找不到已绑定的目标窗口，请重新选择。");
  return std::nullopt;
}

bool WindowsWindowService::isAlive(const WindowTarget& target) const {
  return api_ && api_->isWindow(target.nativeId);
}

bool WindowsWindowService::isForeground(const WindowTarget& target) const {
  return api_ && target.nativeId && api_->foregroundWindow() == target.nativeId;
}

bool WindowsWindowService::activate(const WindowTarget& target) {
  return api_ && target.nativeId && api_->activateWindow(target.nativeId);
}

QSize WindowsWindowService::clientSize(const WindowTarget& target) const {
  if (!api_) return {};
  const auto info = api_->windowInfo(target.nativeId);
  return info ? info->clientSize : QSize();
}

std::optional<QPoint> WindowsWindowService::screenToClient(
    const WindowTarget& target, const QPoint& point) const {
  return api_ ? api_->screenToClient(target.nativeId, point) : std::nullopt;
}

std::optional<QPoint> WindowsWindowService::clientToScreen(
    const WindowTarget& target, const QPoint& point) const {
  return api_ ? api_->clientToScreen(target.nativeId, point) : std::nullopt;
}

QRect WindowsWindowService::virtualDesktopRect() const {
  return api_ ? api_->virtualDesktopRect() : QRect();
}

QString WindowsWindowService::displayName(const WindowTarget& target) const {
  QString executableName = QFileInfo(target.executablePath).fileName();
  if (executableName.isEmpty()) executableName = target.executablePath;
  return QString("%1 — %2").arg(target.title, executableName);
}

bool WindowsWindowService::eligible(const WindowsNativeWindow& window) const {
  return window.id && window.id != excludedWindow_ && window.visible &&
         !window.cloaked && window.topLevel && !window.title.trimmed().isEmpty() &&
         !window.executablePath.trimmed().isEmpty() &&
         !window.className.trimmed().isEmpty() && window.clientSize.isValid() &&
         !window.clientSize.isEmpty();
}

WindowTarget WindowsWindowService::toTarget(
    const WindowsNativeWindow& window) const {
  WindowTarget target;
  target.nativeId = window.id;
  target.executablePath = window.executablePath;
  target.className = window.className;
  target.title = window.title;
  target.clientSize = window.clientSize;
  return target;
}
