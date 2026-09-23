#ifndef FLUTTER_PLUGIN_APP_BADGE_PLUS_PLUGIN_H_
#define FLUTTER_PLUGIN_APP_BADGE_PLUS_PLUGIN_H_

#include <flutter/encodable_value.h>
#include <flutter/method_call.h>
#include <flutter/method_result.h>
#include <flutter/plugin_registrar_windows.h>

#include <memory>

#include "badge_controller.h"

namespace app_badge_plus {

class AppBadgePlusPlugin : public flutter::Plugin {
 public:
  static void RegisterWithRegistrar(flutter::PluginRegistrarWindows* registrar);

  explicit AppBadgePlusPlugin(flutter::PluginRegistrarWindows* registrar);
  ~AppBadgePlusPlugin() override;

  AppBadgePlusPlugin(const AppBadgePlusPlugin&) = delete;
  AppBadgePlusPlugin& operator=(const AppBadgePlusPlugin&) = delete;

  void HandleMethodCall(
      const flutter::MethodCall<flutter::EncodableValue>& method_call,
      std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);

 private:
  flutter::PluginRegistrarWindows* registrar_;
  std::unique_ptr<BadgeController> badge_controller_;
};

}  // namespace app_badge_plus

#endif  // FLUTTER_PLUGIN_APP_BADGE_PLUS_PLUGIN_H_
