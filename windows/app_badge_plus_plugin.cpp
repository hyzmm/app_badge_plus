#include "app_badge_plus_plugin.h"

// This must be included before many other Windows headers.
#include <windows.h>

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>

#include <memory>
#include <variant>

#include "badge_controller.h"

namespace app_badge_plus {

namespace {

// Reads the badged count from the arguments of the `updateBadge` call.
int CountFromArguments(const flutter::EncodableValue* arguments) {
  if (arguments == nullptr) {
    return 0;
  }
  const auto* map = std::get_if<flutter::EncodableMap>(arguments);
  if (map == nullptr) {
    return 0;
  }
  const auto count = map->find(flutter::EncodableValue("count"));
  if (count == map->end()) {
    return 0;
  }
  if (const auto* value = std::get_if<int32_t>(&count->second)) {
    return *value;
  }
  if (const auto* value = std::get_if<int64_t>(&count->second)) {
    return static_cast<int>(*value);
  }
  return 0;
}

HWND WindowForRegistrar(flutter::PluginRegistrarWindows* registrar) {
  flutter::FlutterView* view = registrar->GetView();
  return view != nullptr ? view->GetNativeWindow() : nullptr;
}

}  // namespace

// static
void AppBadgePlusPlugin::RegisterWithRegistrar(
    flutter::PluginRegistrarWindows* registrar) {
  auto channel =
      std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
          registrar->messenger(), "app_badge_plus",
          &flutter::StandardMethodCodec::GetInstance());

  auto plugin = std::make_unique<AppBadgePlusPlugin>(registrar);

  channel->SetMethodCallHandler(
      [plugin_pointer = plugin.get()](const auto& call, auto result) {
        plugin_pointer->HandleMethodCall(call, std::move(result));
      });

  registrar->AddPlugin(std::move(plugin));
}

AppBadgePlusPlugin::AppBadgePlusPlugin(
    flutter::PluginRegistrarWindows* registrar)
    : badge_controller_(
          std::make_unique<BadgeController>(WindowForRegistrar(registrar))) {}

AppBadgePlusPlugin::~AppBadgePlusPlugin() {}

void AppBadgePlusPlugin::HandleMethodCall(
    const flutter::MethodCall<flutter::EncodableValue>& method_call,
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
  if (method_call.method_name() == "updateBadge") {
    const int count = CountFromArguments(method_call.arguments());
    if (badge_controller_->UpdateCount(count)) {
      result->Success();
    } else {
      result->Error("update_badge_failed",
                    "Failed to update the app badge on Windows.");
    }
  } else if (method_call.method_name() == "isSupported") {
    result->Success(flutter::EncodableValue(badge_controller_->IsSupported()));
  } else {
    result->NotImplemented();
  }
}

}  // namespace app_badge_plus
