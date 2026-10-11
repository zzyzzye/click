#include "platform/macos/MacOSClickBackend.h"

#include <ApplicationServices/ApplicationServices.h>

#include <QRandomGenerator>
#include <QKeySequence>
#include <optional>

namespace {

std::optional<CGKeyCode> keyCode(int key) {
  // 字母与数字使用 macOS 标准物理键位；文字步骤使用 Unicode，不依赖键盘布局。
  static const CGKeyCode letters[] = {0,  11, 8,  2,  14, 3, 5,  4,  34, 38, 40, 37, 46,
                                      45, 31, 35, 12, 15, 1, 17, 32, 9,  13, 7,  16, 6};
  static const CGKeyCode numbers[] = {29, 18, 19, 20, 21, 23, 22, 26, 28, 25};
  if (key >= Qt::Key_A && key <= Qt::Key_Z)
    return letters[key - Qt::Key_A];
  if (key >= Qt::Key_0 && key <= Qt::Key_9)
    return numbers[key - Qt::Key_0];
  switch (key) {
  case Qt::Key_Space:
    return 49;
  case Qt::Key_Return:
  case Qt::Key_Enter:
    return 36;
  case Qt::Key_Tab:
    return 48;
  case Qt::Key_Escape:
    return 53;
  case Qt::Key_Backspace:
    return 51;
  case Qt::Key_Left:
    return 123;
  case Qt::Key_Right:
    return 124;
  case Qt::Key_Down:
    return 125;
  case Qt::Key_Up:
    return 126;
  case Qt::Key_F6:
    return 97;
  case Qt::Key_F8:
    return 100;
  default:
    return {};
  }
}

CGPoint resolvePoint(const ClickProfile& profile) {
  CGPoint point = CGPointZero;
  if (profile.targetMode == TargetMode::FollowCursor) {
    CGEventRef current = CGEventCreate(nullptr);
    if (!current) {
      return point;
    }
    point = CGEventGetLocation(current);
    CFRelease(current);
  } else {
    point = CGPointMake(profile.fixedPoint.x(), profile.fixedPoint.y());
  }

  if (profile.jitterRadius > 0) {
    const int jitterX = QRandomGenerator::global()->bounded(-profile.jitterRadius,
                                                            profile.jitterRadius + 1);
    const int jitterY = QRandomGenerator::global()->bounded(-profile.jitterRadius,
                                                            profile.jitterRadius + 1);
    point.x += jitterX;
    point.y += jitterY;
  }

  return point;
}

}  // namespace

bool MacOSClickBackend::click(const ClickProfile& profile) {
  const CGPoint point = resolvePoint(profile);
  const CGEventType downType =
      profile.button == ClickButton::Left ? kCGEventLeftMouseDown : kCGEventRightMouseDown;
  const CGEventType upType =
      profile.button == ClickButton::Left ? kCGEventLeftMouseUp : kCGEventRightMouseUp;
  const CGMouseButton button =
      profile.button == ClickButton::Left ? kCGMouseButtonLeft : kCGMouseButtonRight;

  CGEventRef moveEvent = CGEventCreateMouseEvent(nullptr, kCGEventMouseMoved, point, button);
  CGEventRef downEvent = CGEventCreateMouseEvent(nullptr, downType, point, button);
  CGEventRef upEvent = CGEventCreateMouseEvent(nullptr, upType, point, button);

  if (!moveEvent || !downEvent || !upEvent) {
    if (moveEvent) {
      CFRelease(moveEvent);
    }
    if (downEvent) {
      CFRelease(downEvent);
    }
    if (upEvent) {
      CFRelease(upEvent);
    }
    return false;
  }

  CGEventPost(kCGHIDEventTap, moveEvent);
  CGEventPost(kCGHIDEventTap, downEvent);
  CGEventPost(kCGHIDEventTap, upEvent);

  CFRelease(moveEvent);
  CFRelease(downEvent);
  CFRelease(upEvent);
  return true;
}

bool MacOSClickBackend::keyTap(const ClickProfile& profile) {
  const QKeySequence sequence = QKeySequence::fromString(profile.keyboardKey, QKeySequence::PortableText);
  if (sequence.count() != 1) return false;
  const auto code = keyCode(sequence[0].key());
  if (!code)
    return false;
  CGEventRef down = CGEventCreateKeyboardEvent(nullptr, *code, true);
  CGEventRef up = CGEventCreateKeyboardEvent(nullptr, *code, false);
  if (!down || !up) {
    if (down)
      CFRelease(down);
    if (up)
      CFRelease(up);
    return false;
  }
  CGEventFlags flags = 0;
  const auto mods = sequence[0].keyboardModifiers();
  if (mods.testFlag(Qt::ControlModifier))
    flags |= kCGEventFlagMaskCommand;
  if (mods.testFlag(Qt::MetaModifier))
    flags |= kCGEventFlagMaskControl;
  if (mods.testFlag(Qt::AltModifier))
    flags |= kCGEventFlagMaskAlternate;
  if (mods.testFlag(Qt::ShiftModifier))
    flags |= kCGEventFlagMaskShift;
  CGEventSetFlags(down, flags);
  CGEventSetFlags(up, flags);
  CGEventPost(kCGHIDEventTap, down);
  CGEventPost(kCGHIDEventTap, up);
  CFRelease(down);
  CFRelease(up);
  return true;
}

QVector<QRect> MacOSClickBackend::screenRects() const {
  CGDirectDisplayID displays[32];
  uint32_t count = 0;
  QVector<QRect> result;
  if (CGGetActiveDisplayList(32, displays, &count) != kCGErrorSuccess)
    return result;
  for (uint32_t i = 0; i < count; ++i) {
    const auto r = CGDisplayBounds(displays[i]);
    result.append(QRect(int(r.origin.x), int(r.origin.y), int(r.size.width), int(r.size.height)));
  }
  return result;
}
bool MacOSClickBackend::supportsKey(const QString& text) const {
  auto s = QKeySequence::fromString(text, QKeySequence::PortableText);
  return s.count() == 1 && keyCode(s[0].key()).has_value();
}
bool MacOSClickBackend::textInput(const QString& text) {
  CGEventRef down = CGEventCreateKeyboardEvent(nullptr, 0, true);
  CGEventRef up = CGEventCreateKeyboardEvent(nullptr, 0, false);
  if (!down || !up) { if (down) CFRelease(down); if (up) CFRelease(up); return false; }
  CGEventKeyboardSetUnicodeString(down, text.size(),
                                  reinterpret_cast<const UniChar*>(text.utf16()));
  CGEventKeyboardSetUnicodeString(up, text.size(), reinterpret_cast<const UniChar*>(text.utf16()));
  CGEventPost(kCGHIDEventTap, down); CGEventPost(kCGHIDEventTap, up);
  CFRelease(down); CFRelease(up); return true;
}

QPoint MacOSClickBackend::currentCursorPosition() const {
  CGEventRef current = CGEventCreate(nullptr);
  if (!current) {
    return QPoint(0, 0);
  }
  const CGPoint point = CGEventGetLocation(current);
  CFRelease(current);
  return QPoint(static_cast<int>(point.x), static_cast<int>(point.y));
}

bool MacOSClickBackend::hasAccessibilityPermission() const {
  return AXIsProcessTrusted();
}

void MacOSClickBackend::requestAccessibilityPermission() {
  const void* keys[] = {kAXTrustedCheckOptionPrompt};
  const void* values[] = {kCFBooleanTrue};
  CFDictionaryRef options = CFDictionaryCreate(kCFAllocatorDefault, keys, values, 1,
                                               &kCFCopyStringDictionaryKeyCallBacks,
                                               &kCFTypeDictionaryValueCallBacks);
  AXIsProcessTrustedWithOptions(options);
  CFRelease(options);
}
