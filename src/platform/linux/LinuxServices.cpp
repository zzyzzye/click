#include "platform/PlatformServices.h"
#include "platform/WindowStyleService.h"
#include "core/ClickBackend.h"
#include "core/HotkeyService.h"

#include <QGuiApplication>
#include <QKeySequence>
#include <QRandomGenerator>
#include <QSocketNotifier>
#include <array>
#include <vector>

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>

namespace {
// Wayland 下不使用 XWayland 冒充全局输入支持。
Display* openDisplay() {
  return QGuiApplication::platformName() == "xcb" ? XOpenDisplay(nullptr) : nullptr;
}

KeySym keySymbol(Qt::Key key) {
  if (key >= Qt::Key_A && key <= Qt::Key_Z) return XK_a + key - Qt::Key_A;
  if (key >= Qt::Key_0 && key <= Qt::Key_9) return XK_0 + key - Qt::Key_0;
  if (key >= Qt::Key_F1 && key <= Qt::Key_F35) return XK_F1 + key - Qt::Key_F1;
  switch (key) {
    case Qt::Key_Space: return XK_space;
    case Qt::Key_Return: case Qt::Key_Enter: return XK_Return;
    case Qt::Key_Escape: return XK_Escape;
    case Qt::Key_Tab: return XK_Tab;
    case Qt::Key_Backspace: return XK_BackSpace;
    case Qt::Key_Delete: return XK_Delete;
    case Qt::Key_Insert: return XK_Insert;
    case Qt::Key_Home: return XK_Home;
    case Qt::Key_End: return XK_End;
    case Qt::Key_PageUp: return XK_Page_Up;
    case Qt::Key_PageDown: return XK_Page_Down;
    case Qt::Key_Left: return XK_Left;
    case Qt::Key_Right: return XK_Right;
    case Qt::Key_Up: return XK_Up;
    case Qt::Key_Down: return XK_Down;
    default: return NoSymbol;
  }
}

unsigned int modifierMask(Qt::KeyboardModifiers mods) {
  unsigned int result = 0;
  if (mods.testFlag(Qt::ShiftModifier)) result |= ShiftMask;
  if (mods.testFlag(Qt::ControlModifier)) result |= ControlMask;
  if (mods.testFlag(Qt::AltModifier)) result |= Mod1Mask;
  if (mods.testFlag(Qt::MetaModifier)) result |= Mod4Mask;
  return result;
}

class LinuxClickBackend final : public ClickBackend {
 public:
  LinuxClickBackend() : display_(openDisplay()) {
    int event, error, major, minor;
    available_ = display_ && XTestQueryExtension(display_, &event, &error, &major, &minor);
  }
  ~LinuxClickBackend() override { if (display_) XCloseDisplay(display_); }
  bool click(const ClickProfile& profile) override {
    if (!available_) return false;
    QPoint point = profile.targetMode == TargetMode::FollowCursor
        ? currentCursorPosition() : profile.fixedPoint;
    if (profile.jitterRadius > 0) {
      auto* random = QRandomGenerator::global();
      point += QPoint(random->bounded(-profile.jitterRadius, profile.jitterRadius + 1),
                      random->bounded(-profile.jitterRadius, profile.jitterRadius + 1));
    }
    point.setX(qBound(0, point.x(), DisplayWidth(display_, DefaultScreen(display_)) - 1));
    point.setY(qBound(0, point.y(), DisplayHeight(display_, DefaultScreen(display_)) - 1));
    const unsigned int button = profile.button == ClickButton::Left ? 1 : 3;
    const bool moved = XTestFakeMotionEvent(display_, DefaultScreen(display_), point.x(), point.y(), 0);
    const bool down = XTestFakeButtonEvent(display_, button, True, 0);
    const bool up = XTestFakeButtonEvent(display_, button, False, 0);
    XFlush(display_);
    return moved && down && up;
  }
  bool keyTap(const ClickProfile& profile) override {
    if (!available_) return false;
    const auto sequence = QKeySequence::fromString(profile.keyboardKey, QKeySequence::PortableText);
    if (sequence.count() != 1 || sequence[0].keyboardModifiers() != Qt::NoModifier) return false;
    const KeyCode code = XKeysymToKeycode(display_, keySymbol(sequence[0].key()));
    if (!code) return false;
    const bool down = XTestFakeKeyEvent(display_, code, True, 0);
    const bool up = XTestFakeKeyEvent(display_, code, False, 0);
    XFlush(display_);
    return down && up;
  }
  QPoint currentCursorPosition() const override {
    if (!available_) return {};
    Window root, child;
    int x, y, childX, childY;
    unsigned int mask;
    if (!XQueryPointer(display_, DefaultRootWindow(display_), &root, &child,
                       &x, &y, &childX, &childY, &mask)) return {};
    return {x, y};
  }
  bool hasAccessibilityPermission() const override { return available_; }
  void requestAccessibilityPermission() override {}
 private:
  Display* display_ = nullptr;
  bool available_ = false;
};

// XGrabKey 的错误异步返回，注册期间同步检查，避免误报成功。
bool grabFailed = false;
int grabError(Display*, XErrorEvent*) { grabFailed = true; return 0; }

class LinuxHotkeyService final : public HotkeyService {
 public:
  LinuxHotkeyService() : display_(openDisplay()) {
    if (!display_) return;
    notifier_ = std::make_unique<QSocketNotifier>(ConnectionNumber(display_), QSocketNotifier::Read);
    connect(notifier_.get(), &QSocketNotifier::activated, this, [this] { dispatch(); });
    auto* mapping = XGetModifierMapping(display_);
    if (mapping) {
      const auto numLock = XKeysymToKeycode(display_, XK_Num_Lock);
      for (int mod = 0; mod < 8; ++mod)
        for (int index = 0; index < mapping->max_keypermod; ++index)
          if (numLock && mapping->modifiermap[mod * mapping->max_keypermod + index] == numLock)
            numLockMask_ |= 1U << mod;
      XFreeModifiermap(mapping);
    }
  }
  ~LinuxHotkeyService() override {
    notifier_.reset();
    unregisterAll();
    if (display_) XCloseDisplay(display_);
  }
  bool registerHotkeys(const ClickProfile& profile) override {
    unregisterAll();
    if (!display_) {
      emit registrationFailed("Linux 全局输入需要 X11 会话，请切换到 Xorg 桌面。");
      return false;
    }
    const std::array<QString, 5> sequences = {profile.hotkeys.startStop, profile.hotkeys.capturePoint,
        profile.hotkeys.emergencyStop, profile.hotkeys.macroRecord, profile.hotkeys.macroPlayback};
    for (int action = 0; action < 5; ++action) {
      auto sequence = QKeySequence::fromString(sequences[action], QKeySequence::PortableText);
      if (sequence.count() != 1) return fail();
      const auto code = XKeysymToKeycode(display_, keySymbol(sequence[0].key()));
      const auto mods = modifierMask(sequence[0].keyboardModifiers());
      if (!code || (mods & (LockMask | numLockMask_))) return fail();
      for (const auto& binding : bindings_)
        if (binding.code == code && binding.mods == mods) return fail();
      XSync(display_, False);
      grabFailed = false;
      auto previous = XSetErrorHandler(grabError);
      for (auto locks : lockMasks())
        XGrabKey(display_, code, mods | locks, DefaultRootWindow(display_), False,
                 GrabModeAsync, GrabModeAsync);
      XSync(display_, False);
      XSetErrorHandler(previous);
      bindings_.push_back({code, mods, action});
      if (grabFailed) return fail();
    }
    return true;
  }
  void unregisterAll() override {
    if (!display_) return;
    for (const auto& binding : bindings_)
      for (auto locks : lockMasks())
        XUngrabKey(display_, binding.code, binding.mods | locks, DefaultRootWindow(display_));
    XFlush(display_);
    bindings_.clear();
  }
  QString backendName() const override { return "X11 XGrabKey"; }
 private:
  bool fail() {
    unregisterAll();
    emit registrationFailed("全局热键无效、重复或已被其他程序占用。");
    return false;
  }
  std::array<unsigned int, 4> lockMasks() const {
    return {0, LockMask, numLockMask_, LockMask | numLockMask_};
  }
  void dispatch() {
    while (XPending(display_)) {
      XEvent event;
      XNextEvent(display_, &event);
      if (event.type != KeyPress) continue;
      const auto mods = event.xkey.state & ~(LockMask | numLockMask_);
      // 信号处理可能重新注册热键，遍历期间只保存动作编号。
      int action = -1;
      for (const auto& binding : bindings_)
        if (binding.code == event.xkey.keycode && binding.mods == mods) action = binding.action;
      switch (action) {
        case 0: emit startStopPressed(); break;
        case 1: emit capturePointPressed(); break;
        case 2: emit emergencyStopPressed(); break;
        case 3: emit macroRecordPressed(); break;
        case 4: emit macroPlaybackPressed(); break;
      }
    }
  }
  struct Binding { KeyCode code; unsigned int mods; int action; };
  Display* display_ = nullptr;
  unsigned int numLockMask_ = 0;
  std::vector<Binding> bindings_;
  std::unique_ptr<QSocketNotifier> notifier_;
};

class LinuxWindowStyle final : public WindowStyleService {
 public:
  void prepare(QWidget*) override {}
  void apply(QWidget*) override {}
  bool usesBackdrop() const override { return false; }
};
}  // namespace

std::unique_ptr<ClickBackend> createClickBackend() { return std::make_unique<LinuxClickBackend>(); }
std::unique_ptr<HotkeyService> createHotkeyService() { return std::make_unique<LinuxHotkeyService>(); }
MacroPlatformServices createMacroPlatformServices() { return {}; }
std::unique_ptr<WindowStyleService> createWindowStyleService() { return std::make_unique<LinuxWindowStyle>(); }
