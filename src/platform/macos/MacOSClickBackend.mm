#include "platform/macos/MacOSClickBackend.h"

#include <ApplicationServices/ApplicationServices.h>

#include <QRandomGenerator>
#include <QKeySequence>

namespace {

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
  const int key = sequence[0].toCombined() & ~Qt::KeyboardModifierMask;
  CGKeyCode code = 0;
  if (key == Qt::Key_Space) code = 49;
  else if (key == Qt::Key_Return || key == Qt::Key_Enter) code = 36;
  else if (key == Qt::Key_F6) code = 97;
  else if (key == Qt::Key_F8) code = 100;
  else return false;
  CGEventRef down = CGEventCreateKeyboardEvent(nullptr, code, true);
  CGEventRef up = CGEventCreateKeyboardEvent(nullptr, code, false);
  if (!down || !up) { if (down) CFRelease(down); if (up) CFRelease(up); return false; }
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
