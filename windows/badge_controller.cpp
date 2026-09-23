#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "badge_controller.h"

// These must be included before the headers that build on them.
#include <windows.h>

#include <appmodel.h>
#include <flutter_windows.h>

// GDI+ and the Windows Runtime projections predate /W4-clean builds, so
// silence their declarations only.
#pragma warning(push, 0)
#include <gdiplus.h>
#include <winrt/Windows.Data.Xml.Dom.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Notifications.h>
#pragma warning(pop)

#include <algorithm>
#include <cstring>
#include <string>

namespace app_badge_plus {

namespace {

using winrt::Windows::Data::Xml::Dom::XmlElement;
using winrt::Windows::UI::Notifications::BadgeNotification;
using winrt::Windows::UI::Notifications::BadgeTemplateType;
using winrt::Windows::UI::Notifications::BadgeUpdateManager;

// Taskbar badge icons are drawn at 32 logical pixels for better visibility.
constexpr int kIconSize = 32;
constexpr int kMaxIconSize = 64;

// Counts above this are rendered with the "more than 99" badge that the system
// badge templates use.
constexpr int kMaxNumericCount = 99;

// Opaque red, matching the system-provided numeric badge images.
constexpr Gdiplus::ARGB kBackgroundColor = 0xFFE81123;

// Whether the process has the package identity that the badge notification
// APIs require.
bool HasPackageIdentity() {
  UINT32 length = 0;
  return ::GetCurrentPackageFullName(&length, nullptr) ==
         ERROR_INSUFFICIENT_BUFFER;
}

std::wstring BadgeLabel(int count) {
  if (count > kMaxNumericCount) {
    return L"99+";
  }
  return std::to_wstring(count);
}

// GDI+ must be started before any of its types can draw. It is started once
// and shut down when the process exits.
class GdiplusSession {
 public:
  GdiplusSession() {
    started_ =
        Gdiplus::GdiplusStartup(&token_, &input_, nullptr) == Gdiplus::Ok;
  }

  ~GdiplusSession() {
    if (started_) {
      Gdiplus::GdiplusShutdown(token_);
    }
  }

  bool started() const { return started_; }

 private:
  Gdiplus::GdiplusStartupInput input_;
  ULONG_PTR token_ = 0;
  bool started_ = false;
};

bool EnsureGdiplusStarted() {
  static GdiplusSession session;
  return session.started();
}

// Paints `label` onto `graphics` as white text on a red disc.
bool PaintBadge(Gdiplus::Graphics& graphics,
                int size,
                const std::wstring& label) {
  graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
  graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
  graphics.Clear(Gdiplus::Color(0, 0, 0, 0));

  Gdiplus::SolidBrush background{Gdiplus::Color(kBackgroundColor)};
  const Gdiplus::REAL edge = 0.5f;
  const Gdiplus::REAL diameter = static_cast<Gdiplus::REAL>(size) - 2 * edge;
  if (graphics.FillEllipse(&background, edge, edge, diameter, diameter) !=
      Gdiplus::Ok) {
    return false;
  }

  Gdiplus::FontFamily segoe_ui(L"Segoe UI");
  const Gdiplus::FontFamily* family = segoe_ui.GetLastStatus() == Gdiplus::Ok
                                          ? &segoe_ui
                                          : Gdiplus::FontFamily::GenericSansSerif();

  const int length = static_cast<int>(label.size());
  const Gdiplus::StringFormat* measurement =
      Gdiplus::StringFormat::GenericTypographic();
  const Gdiplus::REAL available_width = static_cast<Gdiplus::REAL>(size) * 0.82f;
  float em_size = static_cast<float>(size) *
                  (length == 1 ? 0.72f : (length == 2 ? 0.58f : 0.42f));
  for (int attempt = 0; attempt < 8; ++attempt) {
    const Gdiplus::Font candidate(family, em_size, Gdiplus::FontStyleBold,
                                  Gdiplus::UnitPixel);
    Gdiplus::RectF measured;
    const Gdiplus::Status status = graphics.MeasureString(
        label.c_str(), length, &candidate, Gdiplus::PointF(0.0f, 0.0f),
        measurement, &measured);
    if (status == Gdiplus::Ok && measured.Width <= available_width) {
      break;
    }
    em_size *= 0.9f;
  }

  Gdiplus::StringFormat layout;
  layout.SetAlignment(Gdiplus::StringAlignmentCenter);
  layout.SetLineAlignment(Gdiplus::StringAlignmentCenter);

  // Digits sit above the line box centre because of their descender space.
  const Gdiplus::REAL nudge = static_cast<Gdiplus::REAL>(size) * 0.04f;
  const Gdiplus::RectF layout_rect(0.0f, nudge, static_cast<Gdiplus::REAL>(size),
                                   static_cast<Gdiplus::REAL>(size));

  const Gdiplus::Font font(family, em_size, Gdiplus::FontStyleBold,
                           Gdiplus::UnitPixel);
  Gdiplus::SolidBrush foreground(Gdiplus::Color(255, 255, 255));
  return graphics.DrawString(label.c_str(), length, &font, layout_rect, &layout,
                             &foreground) == Gdiplus::Ok;
}

// Renders `label` as a badge icon sized for the window's DPI.
HICON CreateBadgeIcon(HWND window, const std::wstring& label) {
  if (!EnsureGdiplusStarted()) {
    return nullptr;
  }

  const int dpi = static_cast<int>(FlutterDesktopGetDpiForHWND(window));
  const int size =
      std::clamp(::MulDiv(kIconSize, dpi == 0 ? 96 : dpi, 96), kIconSize,
                 kMaxIconSize);

  BITMAPINFO color_info = {};
  color_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  color_info.bmiHeader.biWidth = size;
  // A negative height keeps the bitmap top-down, as GDI+ expects.
  color_info.bmiHeader.biHeight = -size;
  color_info.bmiHeader.biPlanes = 1;
  color_info.bmiHeader.biBitCount = 32;
  color_info.bmiHeader.biCompression = BI_RGB;

  void* color_pixels = nullptr;
  HBITMAP color_bitmap = ::CreateDIBSection(nullptr, &color_info, DIB_RGB_COLORS,
                                            &color_pixels, nullptr, 0);
  if (color_bitmap == nullptr || color_pixels == nullptr) {
    if (color_bitmap != nullptr) {
      ::DeleteObject(color_bitmap);
    }
    return nullptr;
  }

  bool painted = false;
  {
    // Drawing straight into the DIB keeps the premultiplied alpha that
    // CreateIconIndirect blends with.
    Gdiplus::Bitmap bitmap(size, size, size * 4, PixelFormat32bppPARGB,
                           static_cast<BYTE*>(color_pixels));
    if (bitmap.GetLastStatus() == Gdiplus::Ok) {
      Gdiplus::Graphics graphics(&bitmap);
      if (graphics.GetLastStatus() == Gdiplus::Ok) {
        painted = PaintBadge(graphics, size, label);
      }
    }
  }
  if (!painted) {
    ::DeleteObject(color_bitmap);
    return nullptr;
  }

  const int mask_stride = ((size + 31) / 32) * 4;
  BITMAPINFO mask_info = {};
  mask_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  mask_info.bmiHeader.biWidth = size;
  mask_info.bmiHeader.biHeight = size;
  mask_info.bmiHeader.biPlanes = 1;
  mask_info.bmiHeader.biBitCount = 1;
  mask_info.bmiHeader.biCompression = BI_RGB;

  void* mask_bits = nullptr;
  HBITMAP mask_bitmap = ::CreateDIBSection(nullptr, &mask_info, DIB_RGB_COLORS,
                                           &mask_bits, nullptr, 0);
  if (mask_bitmap == nullptr || mask_bits == nullptr) {
    if (mask_bitmap != nullptr) {
      ::DeleteObject(mask_bitmap);
    }
    ::DeleteObject(color_bitmap);
    return nullptr;
  }
  // A zeroed mask marks every pixel as opaque; the colour bitmap's alpha
  // channel does the actual blending.
  std::memset(mask_bits, 0,
              static_cast<size_t>(mask_stride) * static_cast<size_t>(size));

  ICONINFO icon_info = {};
  icon_info.fIcon = TRUE;
  icon_info.hbmColor = color_bitmap;
  icon_info.hbmMask = mask_bitmap;

  HICON icon = ::CreateIconIndirect(&icon_info);
  ::DeleteObject(mask_bitmap);
  ::DeleteObject(color_bitmap);
  return icon;
}

}  // namespace

BadgeController::BadgeController() {
  const HRESULT com_result = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  com_initialized_ = SUCCEEDED(com_result);

  ITaskbarList3* taskbar = nullptr;
  if (SUCCEEDED(::CoCreateInstance(CLSID_TaskbarList, nullptr,
                                   CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&taskbar)))) {
    if (SUCCEEDED(taskbar->HrInit())) {
      taskbar_ = taskbar;
    } else {
      taskbar->Release();
    }
  }

  use_notification_badges_ = HasPackageIdentity();
  if (use_notification_badges_) {
    try {
      winrt::init_apartment(winrt::apartment_type::multi_threaded);
      apartment_initialized_ = true;
    } catch (const winrt::hresult_error&) {
      // The thread already lives in another apartment, and the badge APIs are
      // agile, so they can still be called from it.
    }
  }
}

BadgeController::~BadgeController() {
  if (taskbar_ != nullptr) {
    taskbar_->Release();
    taskbar_ = nullptr;
  }
  if (apartment_initialized_) {
    winrt::uninit_apartment();
  }
  if (com_initialized_) {
    ::CoUninitialize();
  }
}

void BadgeController::SetWindow(HWND window) {
  window_ = window;
}

bool BadgeController::IsSupported() const {
  return use_notification_badges_ || taskbar_ != nullptr;
}

bool BadgeController::UpdateCount(int count) {
  if (count > 0) {
    if (use_notification_badges_ && UpdateNotificationBadge(count)) {
      return true;
    }
    return UpdateOverlayBadge(count);
  }
  if (use_notification_badges_ && ClearNotificationBadge()) {
    return true;
  }
  return ClearOverlayBadge();
}

bool BadgeController::UpdateNotificationBadge(int count) {
  try {
    auto xml = BadgeUpdateManager::GetTemplateContent(
        BadgeTemplateType::BadgeNumber);
    auto badge = xml.SelectSingleNode(L"/badge").as<XmlElement>();
    // The badge templates collapse every count above 99 into one badge.
    const int value = count > kMaxNumericCount ? kMaxNumericCount : count;
    badge.SetAttribute(L"value", winrt::to_hstring(value));
    BadgeUpdateManager::CreateBadgeUpdaterForApplication().Update(
        BadgeNotification(xml));
    return true;
  } catch (const winrt::hresult_error&) {
    return false;
  }
}

bool BadgeController::ClearNotificationBadge() {
  try {
    BadgeUpdateManager::CreateBadgeUpdaterForApplication().Clear();
    return true;
  } catch (const winrt::hresult_error&) {
    return false;
  }
}

bool BadgeController::UpdateOverlayBadge(int count) {
  if (window_ == nullptr || taskbar_ == nullptr) {
    return false;
  }
  const std::wstring label = BadgeLabel(count);
  HWND root = ::GetAncestor(window_, GA_ROOT);
  HWND target_window = root ? root : window_;
  HICON icon = CreateBadgeIcon(target_window, label);
  if (icon == nullptr) {
    return false;
  }
  const HRESULT result = taskbar_->SetOverlayIcon(target_window, icon, label.c_str());
  ::DestroyIcon(icon);
  return SUCCEEDED(result);
}

bool BadgeController::ClearOverlayBadge() {
  if (window_ == nullptr || taskbar_ == nullptr) {
    return false;
  }
  HWND root = ::GetAncestor(window_, GA_ROOT);
  HWND target_window = root ? root : window_;
  return SUCCEEDED(taskbar_->SetOverlayIcon(target_window, nullptr, L""));
}

}  // namespace app_badge_plus
