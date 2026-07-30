#pragma once

#include <QPoint>
#include <QRect>
#include <QString>
#include <QVector>

#include <algorithm>
#include <cmath>
#include <optional>

#include "core/MacroTypes.h"

class WindowService {
 public:
  virtual ~WindowService() = default;

  virtual QVector<WindowTarget> availableWindows() const = 0;
  virtual std::optional<WindowTarget> windowAt(const QPoint& screenPoint) const = 0;
  virtual std::optional<WindowTarget> resolve(const WindowTarget& target,
                                              QString* error = nullptr) const = 0;
  virtual bool isAlive(const WindowTarget& target) const = 0;
  virtual bool isForeground(const WindowTarget& target) const = 0;
  virtual bool activate(const WindowTarget& target) = 0;
  virtual QSize clientSize(const WindowTarget& target) const = 0;
  virtual std::optional<QPoint> screenToClient(const WindowTarget& target,
                                               const QPoint& point) const = 0;
  virtual std::optional<QPoint> clientToScreen(const WindowTarget& target,
                                               const QPoint& point) const = 0;
  virtual QRect virtualDesktopRect() const = 0;
  virtual QString displayName(const WindowTarget& target) const = 0;

  static bool sizeMatches(const QSize& recorded, const QSize& current) {
    if (!recorded.isValid() || recorded.isEmpty() || !current.isValid() ||
        current.isEmpty()) {
      return false;
    }
    const int widthTolerance =
        std::max(8, static_cast<int>(std::ceil(recorded.width() * 0.02)));
    const int heightTolerance =
        std::max(8, static_cast<int>(std::ceil(recorded.height() * 0.02)));
    return std::abs(recorded.width() - current.width()) <= widthTolerance &&
           std::abs(recorded.height() - current.height()) <= heightTolerance;
  }
};
