#include "app/controllers/WindowChromeController.h"

#include <QAbstractButton>
#include <QEvent>
#include <QMainWindow>

#include "app/widgets/CaptionBar.h"
#include "platform/WindowStyleService.h"

#if defined(Q_OS_WIN)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <windowsx.h>
#endif

WindowChromeController::WindowChromeController(
    QMainWindow* window, CaptionBar* captionBar,
    WindowStyleService* windowStyle)
    : window_(window), captionBar_(captionBar), windowStyle_(windowStyle) {
  Q_ASSERT(window_);
  Q_ASSERT(captionBar_);
  Q_ASSERT(windowStyle_);

  QObject::connect(captionBar_, &CaptionBar::minimizeRequested, window_,
                   &QWidget::showMinimized);
  QObject::connect(captionBar_, &CaptionBar::maximizeRestoreRequested, window_,
                   [this] {
                     if (window_->isMaximized()) {
                       window_->showNormal();
                     } else {
                       window_->showMaximized();
                     }
                   });
  QObject::connect(captionBar_, &CaptionBar::closeRequested, window_,
                   &QWidget::close);
}

void WindowChromeController::handleShown() {
  windowStyle_->apply(window_);
  captionBar_->setMaximized(window_->isMaximized());
}

void WindowChromeController::handleWindowStateChange(const QEvent* event) {
  if (event && event->type() == QEvent::WindowStateChange) {
    captionBar_->setMaximized(window_->isMaximized());
  }
}

void WindowChromeController::setAlwaysOnTop(bool enabled) {
  const bool shown = window_->isVisible();
  window_->setWindowFlag(Qt::WindowStaysOnTopHint, enabled);
  if (shown) {
    window_->show();
    window_->raise();
  }

  // setWindowFlag 会重建 HWND，窗口边框和 DWM 属性都需要重新应用。
  windowStyle_->apply(window_);
}

bool WindowChromeController::handleNativeEvent(const QByteArray& eventType,
                                                void* message,
                                                qintptr* result) {
#if defined(Q_OS_WIN)
  if (eventType != "windows_generic_MSG") return false;

  auto* msg = static_cast<MSG*>(message);
  switch (msg->message) {
    case WM_NCCALCSIZE: {
      if (msg->wParam == FALSE) break;
      if (IsZoomed(msg->hwnd)) {
        auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(msg->lParam);
        const UINT dpi = GetDpiForWindow(msg->hwnd);
        const int frameX = GetSystemMetricsForDpi(SM_CXSIZEFRAME, dpi) +
                           GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
        const int frameY = GetSystemMetricsForDpi(SM_CYSIZEFRAME, dpi) +
                           GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
        params->rgrc[0].left += frameX;
        params->rgrc[0].right -= frameX;
        params->rgrc[0].top += frameY;
        params->rgrc[0].bottom -= frameY;
      }
      *result = 0;
      return true;
    }
    case WM_NCHITTEST: {
      const POINT cursor{GET_X_LPARAM(msg->lParam),
                         GET_Y_LPARAM(msg->lParam)};
      RECT windowRect{};
      GetWindowRect(msg->hwnd, &windowRect);
      const qreal dpr = window_->devicePixelRatioF();
      const auto nativeRect = [&](QAbstractButton* button) {
        const QRect inWindow = button->rect().translated(
            button->mapTo(window_, QPoint(0, 0)));
        return QRect(windowRect.left + qRound(inWindow.left() * dpr),
                     windowRect.top + qRound(inWindow.top() * dpr),
                     qRound(inWindow.width() * dpr),
                     qRound(inWindow.height() * dpr));
      };
      const QPoint point(cursor.x, cursor.y);

      if (auto* button = captionBar_->findChild<QAbstractButton*>(
              "captionMaximizeButton")) {
        if (nativeRect(button).contains(point)) {
          *result = HTCLIENT;
          return true;
        }
      }
      for (const char* objectName : {"captionMinimizeButton",
                                     "captionCloseButton"}) {
        if (auto* button =
                captionBar_->findChild<QAbstractButton*>(objectName)) {
          if (nativeRect(button).contains(point)) {
            *result = HTCLIENT;
            return true;
          }
        }
      }

      if (IsZoomed(msg->hwnd)) break;
      const UINT dpi = GetDpiForWindow(msg->hwnd);
      const int border = GetSystemMetricsForDpi(SM_CXSIZEFRAME, dpi) +
                         GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
      const int x = cursor.x - windowRect.left;
      const int y = cursor.y - windowRect.top;
      const int width = windowRect.right - windowRect.left;
      const int height = windowRect.bottom - windowRect.top;
      const bool onLeft = x < border;
      const bool onRight = x >= width - border;
      const bool onTop = y < border;
      const bool onBottom = y >= height - border;
      if (onTop && onLeft) {
        *result = HTTOPLEFT;
        return true;
      }
      if (onTop && onRight) {
        *result = HTTOPRIGHT;
        return true;
      }
      if (onBottom && onLeft) {
        *result = HTBOTTOMLEFT;
        return true;
      }
      if (onBottom && onRight) {
        *result = HTBOTTOMRIGHT;
        return true;
      }
      if (onLeft) {
        *result = HTLEFT;
        return true;
      }
      if (onRight) {
        *result = HTRIGHT;
        return true;
      }
      if (onTop) {
        *result = HTTOP;
        return true;
      }
      if (onBottom) {
        *result = HTBOTTOM;
        return true;
      }
      break;
    }
    default:
      break;
  }
#else
  Q_UNUSED(eventType);
  Q_UNUSED(message);
  Q_UNUSED(result);
#endif
  return false;
}
