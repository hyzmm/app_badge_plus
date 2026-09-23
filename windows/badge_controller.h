#ifndef FLUTTER_PLUGIN_APP_BADGE_PLUS_BADGE_CONTROLLER_H_
#define FLUTTER_PLUGIN_APP_BADGE_PLUS_BADGE_CONTROLLER_H_

#ifndef NOMINMAX
#define NOMINMAX
#endif

// windows.h has to come before the headers that build on it.
#include <windows.h>
#include <shobjidl.h>

namespace app_badge_plus {

// Draws the app badge on Windows.
//
// Apps running with package identity use the badge notification APIs described
// in https://learn.microsoft.com/windows/apps/develop/notifications/badges.
// Unpackaged apps, which is how Flutter apps run by default, use the taskbar
// overlay icon instead: the Win32 counterpart of a taskbar badge.
class BadgeController {
 public:
  BadgeController();
  ~BadgeController();

  BadgeController(const BadgeController&) = delete;
  BadgeController& operator=(const BadgeController&) = delete;

  void SetWindow(HWND window);

  bool IsSupported() const;

  // Shows `count` on the app badge. Counts above 99 are rendered as the
  // system "more than 99" badge. Zero or negative counts clear the badge.
  bool UpdateCount(int count);

 private:
  bool UpdateNotificationBadge(int count);
  bool ClearNotificationBadge();
  bool UpdateOverlayBadge(int count);
  bool ClearOverlayBadge();

  HWND window_ = nullptr;
  ITaskbarList3* taskbar_ = nullptr;
  bool com_initialized_ = false;
  bool apartment_initialized_ = false;
  bool use_notification_badges_ = false;
};

}  // namespace app_badge_plus

#endif  // FLUTTER_PLUGIN_APP_BADGE_PLUS_BADGE_CONTROLLER_H_
