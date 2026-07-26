#include "platform/macos/MacOSClickBackend.h"

#include <ApplicationServices/ApplicationServices.h>

#include <QRandomGenerator>

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
