#include "include/app_badge_plus/app_badge_plus_plugin_c_api.h"

#include <flutter/plugin_registrar_windows.h>

#include "app_badge_plus_plugin.h"

void AppBadgePlusPluginCApiRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
  app_badge_plus::AppBadgePlusPlugin::RegisterWithRegistrar(
      flutter::PluginRegistrarManager::GetInstance()
          ->GetRegistrar<flutter::PluginRegistrarWindows>(registrar));
}
