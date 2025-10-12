#define WLR_USE_UNSTABLE

#include "ViewGesture.hpp"
#include "globals.hpp"
#include "hyprview.hpp"
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/debug/Log.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/desktop/Window.hpp>
#include <hyprland/src/managers/input/trackpad/GestureTypes.hpp>
#include <hyprland/src/managers/input/trackpad/TrackpadGestures.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprutils/string/ConstVarList.hpp>
#include <unistd.h>

using namespace Hyprutils::String;

inline CFunctionHook *g_pRenderWorkspaceHook = nullptr;
inline CFunctionHook *g_pAddDamageHookA = nullptr;
inline CFunctionHook *g_pAddDamageHookB = nullptr;
typedef void (*origRenderWorkspace)(void *, PHLMONITOR, PHLWORKSPACE, timespec *, const CBox &);
typedef void (*origAddDamageA)(void *, const CBox &);
typedef void (*origAddDamageB)(void *, const pixman_region32_t *);

static bool g_unloading = false;

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() { return HYPRLAND_API_VERSION; }

static bool renderingOverview = false;

static void hkRenderWorkspace(void *thisptr, PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace, timespec *now, const CBox &geometry) {
  // Check if this monitor has an active overview
  auto it = g_pHypreEyeInstances.find(pMonitor);
  bool hasOverview = (it != g_pHypreEyeInstances.end() && it->second != nullptr);

  if (!hasOverview || renderingOverview || (hasOverview && it->second->blockOverviewRendering)) {
    // Normal rendering when overview is not active on this monitor
    ((origRenderWorkspace)(g_pRenderWorkspaceHook->m_original))(thisptr, pMonitor, pWorkspace, now, geometry);
  } else {
    // When overview is active on this monitor, skip the normal workspace
    // rendering This prevents real windows from showing underneath the overview
    // The overview will render its own pass element which handles the display
    it->second->render();
  }
}

static void hkAddDamageA(void *thisptr, const CBox &box) {
  const auto PMONITOR = (CMonitor *)thisptr;
  const auto PMONITORSP = PMONITOR->m_self.lock();

  if (!PMONITORSP) {
    ((origAddDamageA)g_pAddDamageHookA->m_original)(thisptr, box);
    return;
  }

  auto it = g_pHypreEyeInstances.find(PMONITORSP);
  if (it == g_pHypreEyeInstances.end() || !it->second || it->second->blockDamageReporting) {
    ((origAddDamageA)g_pAddDamageHookA->m_original)(thisptr, box);
    return;
  }

  it->second->onDamageReported();
}

static void hkAddDamageB(void *thisptr, const pixman_region32_t *rg) {
  const auto PMONITOR = (CMonitor *)thisptr;
  const auto PMONITORSP = PMONITOR->m_self.lock();

  if (!PMONITORSP) {
    ((origAddDamageB)g_pAddDamageHookB->m_original)(thisptr, rg);
    return;
  }

  auto it = g_pHypreEyeInstances.find(PMONITORSP);
  if (it == g_pHypreEyeInstances.end() || !it->second || it->second->blockDamageReporting) {
    ((origAddDamageB)g_pAddDamageHookB->m_original)(thisptr, rg);
    return;
  }

  it->second->onDamageReported();
}

static SDispatchResult onHyprviewDispatcher(std::string arg) {
  Debug::log(LOG, "[hyprview] Dispatcher called with arg='{}'", arg);

  // Check if any instance is swiping
  for (auto &[monitor, instance] : g_pHypreEyeInstances) {
    if (instance && instance->m_isSwiping)
      return {.success = false, .error = "already swiping"};
  }

  if (arg == "select") {
    // Select hovered window on the monitor where the cursor is
    auto PMONITOR = g_pCompositor->m_lastMonitor.lock();
    if (PMONITOR) {
      auto it = g_pHypreEyeInstances.find(PMONITOR);
      if (it != g_pHypreEyeInstances.end() && it->second) {
        it->second->selectHoveredWindow();
        it->second->close();
      }
    }
    return {};
  }

  // Treat empty arg as "toggle" for convenience
  if (arg == "toggle" || arg.empty()) {
    Debug::log(LOG, "[hyprview] Toggle called");

    bool hasAnyOverview = !g_pHypreEyeInstances.empty();

    if (hasAnyOverview) {
      // Close all overviews on all monitors
      LOG_FILE("=== TOGGLE: CLOSING ALL OVERVIEWS ===");
      LOG_FILE(std::string("Toggle: closing ") + std::to_string(g_pHypreEyeInstances.size()) + " instances");
      Debug::log(LOG, "[hyprview] Toggle: closing all overviews ({} instances)", g_pHypreEyeInstances.size());

      for (auto &[monitor, instance] : g_pHypreEyeInstances) {
        if (instance && !instance->closing) {
          LOG_FILE("Toggle: Calling close() on instance");
          instance->close();
          LOG_FILE("Toggle: close() returned");
        }
      }

      LOG_FILE("Toggle: Calling removeAllOfType()");
      g_pHyprRenderer->m_renderPass.removeAllOfType("CHyprViewPassElement");
      LOG_FILE("Toggle: removeAllOfType() done");

      LOG_FILE("Toggle: Cleaning up empty monitor instances");
      for (auto it = g_pHypreEyeInstances.begin(); it != g_pHypreEyeInstances.end();) {
        if (it->second && it->second->closing) {
          LOG_FILE("Toggle: Erasing closing instance");
          it = g_pHypreEyeInstances.erase(it);
        } else {
          ++it;
        }
      }
      LOG_FILE(std::string("Toggle: After cleanup, instances remaining=") + std::to_string(g_pHypreEyeInstances.size()));
    } else {
      // Open overview on all enabled monitors
      Debug::log(LOG, "[hyprview] Toggle: opening overviews on all monitors");

      renderingOverview = true;
      for (auto &monitor : g_pCompositor->m_monitors) {
        if (monitor->m_enabled && monitor->m_activeWorkspace) {
          Debug::log(LOG, "[hyprview] Creating overview for monitor {}", monitor->m_description);
          g_pHypreEyeInstances[monitor] = std::make_unique<CHyprView>(monitor, monitor->m_activeWorkspace);
        }
      }
      renderingOverview = false;
    }

    return {};
  }

  if (arg == "off" || arg == "close" || arg == "disable") {
    LOG_FILE("=== OFF/CLOSE/DISABLE CALLED ===");
    for (auto &[monitor, instance] : g_pHypreEyeInstances) {
      if (instance) {
        LOG_FILE("off: Calling close() on instance");
        instance->close();
      }
    }

    LOG_FILE("off: Calling removeAllOfType()");
    g_pHyprRenderer->m_renderPass.removeAllOfType("CHyprViewPassElement");

    LOG_FILE("off: Cleaning up empty monitor instances");
    for (auto it = g_pHypreEyeInstances.begin(); it != g_pHypreEyeInstances.end();) {
      if (it->second && it->second->closing) {
        LOG_FILE("off: Erasing closing instance");
        it = g_pHypreEyeInstances.erase(it);
      } else {
        ++it;
      }
    }
    LOG_FILE(std::string("off: After cleanup, instances remaining=") + std::to_string(g_pHypreEyeInstances.size()));
    return {};
  }

  if (!g_pHypreEyeInstances.empty())
    return {};

  renderingOverview = true;
  for (auto &monitor : g_pCompositor->m_monitors) {
    if (monitor->m_enabled && monitor->m_activeWorkspace) {
      g_pHypreEyeInstances[monitor] = std::make_unique<CHyprView>(monitor, monitor->m_activeWorkspace);
    }
  }
  renderingOverview = false;
  return {};
}

static void failNotif(const std::string &reason) { HyprlandAPI::addNotification(PHANDLE, "[hyprview] Failure in initialization: " + reason, CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000); }

static Hyprlang::CParseResult hyprviewGestureKeyword(const char *LHS, const char *RHS) {
  Hyprlang::CParseResult result;

  if (g_unloading)
    return result;

  CConstVarList data(RHS);

  size_t fingerCount = 0;
  eTrackpadGestureDirection direction = TRACKPAD_GESTURE_DIR_NONE;

  try {
    fingerCount = std::stoul(std::string{data[0]});
  } catch (...) {
    result.setError(std::format("Invalid value {} for finger count", data[0]).c_str());
    return result;
  }

  if (fingerCount <= 1 || fingerCount >= 10) {
    result.setError(std::format("Invalid value {} for finger count", data[0]).c_str());
    return result;
  }

  direction = g_pTrackpadGestures->dirForString(data[1]);

  if (direction == TRACKPAD_GESTURE_DIR_NONE) {
    result.setError(std::format("Invalid direction: {}", data[1]).c_str());
    return result;
  }

  int startDataIdx = 2;
  uint32_t modMask = 0;
  float deltaScale = 1.F;

  while (true) {

    if (data[startDataIdx].starts_with("mod:")) {
      modMask = g_pKeybindManager->stringToModMask(std::string{data[startDataIdx].substr(4)});
      startDataIdx++;
      continue;
    } else if (data[startDataIdx].starts_with("scale:")) {
      try {
        deltaScale = std::clamp(std::stof(std::string{data[startDataIdx].substr(6)}), 0.1F, 10.F);
        startDataIdx++;
        continue;
      } catch (...) {
        result.setError(std::format("Invalid delta scale: {}", std::string{data[startDataIdx].substr(6)}).c_str());
        return result;
      }
    }

    break;
  }

  std::expected<void, std::string> resultFromGesture;

  if (data[startDataIdx] == "toggle")
    resultFromGesture = g_pTrackpadGestures->addGesture(makeUnique<CViewGesture>(), fingerCount, direction, modMask, deltaScale);
  else if (data[startDataIdx] == "unset")
    resultFromGesture = g_pTrackpadGestures->removeGesture(fingerCount, direction, modMask, deltaScale);
  else {
    result.setError(std::format("Invalid gesture: {}", data[startDataIdx]).c_str());
    return result;
  }

  if (!resultFromGesture) {
    result.setError(resultFromGesture.error().c_str());
    return result;
  }

  return result;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
  PHANDLE = handle;

  const std::string HASH = __hyprland_api_get_hash();

  if (HASH != GIT_COMMIT_HASH) {
    failNotif("Version mismatch (headers ver is not equal to running hyprland ver)");
    throw std::runtime_error("[hyprview] Version mismatch");
  }

  auto FNS = HyprlandAPI::findFunctionsByName(PHANDLE, "renderWorkspace");
  if (FNS.empty()) {
    failNotif("no fns for hook renderWorkspace");
    throw std::runtime_error("[hyprview] No fns for hook renderWorkspace");
  }

  g_pRenderWorkspaceHook = HyprlandAPI::createFunctionHook(PHANDLE, FNS[0].address, (void *)hkRenderWorkspace);

  FNS = HyprlandAPI::findFunctionsByName(PHANDLE, "addDamageEPK15pixman_region32");
  if (FNS.empty()) {
    failNotif("no fns for hook addDamageEPK15pixman_region32");
    throw std::runtime_error("[hyprview] No fns for hook addDamageEPK15pixman_region32");
  }

  g_pAddDamageHookB = HyprlandAPI::createFunctionHook(PHANDLE, FNS[0].address, (void *)hkAddDamageB);

  FNS = HyprlandAPI::findFunctionsByName(PHANDLE, "_ZN8CMonitor9addDamageERKN9Hyprutils4Math4CBoxE");
  if (FNS.empty()) {
    failNotif("no fns for hook _ZN8CMonitor9addDamageERKN9Hyprutils4Math4CBoxE");
    throw std::runtime_error("[hyprview] No fns for hook "
                             "_ZN8CMonitor9addDamageERKN9Hyprutils4Math4CBoxE");
  }

  g_pAddDamageHookA = HyprlandAPI::createFunctionHook(PHANDLE, FNS[0].address, (void *)hkAddDamageA);

  bool success = g_pRenderWorkspaceHook->hook();
  success = success && g_pAddDamageHookA->hook();
  success = success && g_pAddDamageHookB->hook();

  if (!success) {
    failNotif("Failed initializing hooks");
    throw std::runtime_error("[hyprview] Failed initializing hooks");
  }

  static auto P = HyprlandAPI::registerCallbackDynamic(PHANDLE, "preRender", [](void *self, SCallbackInfo &info, std::any param) {
    for (auto &[monitor, instance] : g_pHypreEyeInstances) {
      if (instance)
        instance->onPreRender();
    }

    // Clean up closing instances
    for (auto it = g_pHypreEyeInstances.begin(); it != g_pHypreEyeInstances.end();) {
      if (it->second && it->second->closing) {
        LOG_FILE("preRender: Removing closing instance");
        g_pHyprRenderer->m_renderPass.removeAllOfType("CHyprViewPassElement");
        it = g_pHypreEyeInstances.erase(it);
      } else {
        ++it;
      }
    }
  });

  HyprlandAPI::addDispatcherV2(PHANDLE, "hyprview:toggle", ::onHyprviewDispatcher);

  Debug::log(LOG, "[hyprview] Plugin initialized, dispatcher 'hyprview:toggle' registered");

  HyprlandAPI::addConfigKeyword(PHANDLE, "hyprview-gesture", ::hyprviewGestureKeyword, {});

  HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprview:margin", Hyprlang::INT{10});
  HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprview:padding", Hyprlang::INT{10});
  HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprview:bg_color", Hyprlang::INT{0xFF111111});
  HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprview:grid_color", Hyprlang::INT{0xFF000000});
  HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprview:gesture_distance", Hyprlang::INT{200});
  HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprview:active_border_color", Hyprlang::INT{0xFFCA7815});
  HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprview:inactive_border_color", Hyprlang::INT{0x88c0c0c0});
  HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprview:border_width", Hyprlang::INT{5});
  HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprview:border_radius", Hyprlang::INT{5});
  HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprview:debug_log", Hyprlang::INT{0});

  HyprlandAPI::reloadConfig();

  return {"hyprview", "A plugin for an GNOME style overview", "yz778", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
  g_pHyprRenderer->m_renderPass.removeAllOfType("CHyprViewPassElement");

  g_unloading = true;

  g_pHypreEyeInstances.clear();

  g_pConfigManager->reload();
}
