#define WLR_USE_UNSTABLE

#include "ViewGesture.hpp"
#include "globals.hpp"
#include "hyprview.hpp"
#include "PlacementAlgorithms.hpp"
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
#include <iostream>
#include <fstream>

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
  auto it = g_pHyprViewInstances.find(pMonitor);
  bool hasOverview = (it != g_pHyprViewInstances.end() && it->second != nullptr);

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

  auto it = g_pHyprViewInstances.find(PMONITORSP);
  if (it == g_pHyprViewInstances.end() || !it->second || it->second->blockDamageReporting) {
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

  auto it = g_pHyprViewInstances.find(PMONITORSP);
  if (it == g_pHyprViewInstances.end() || !it->second || it->second->blockDamageReporting) {
    ((origAddDamageB)g_pAddDamageHookB->m_original)(thisptr, rg);
    return;
  }

  it->second->onDamageReported();
}

// Helper function to parse dispatcher arguments
struct DispatcherArgs {
  enum class Action { TOGGLE,
                      ON,
                      OFF,
                      SELECT,
                      DEBUG } action;
  EWindowCollectionMode collectionMode;
  std::string placement;
  std::string targetMonitor;  // Empty = current monitor, otherwise specific monitor name
  std::string error;
};

static DispatcherArgs parseDispatcherArgs(const std::string &arg) {
  DispatcherArgs result;
  result.action = DispatcherArgs::Action::TOGGLE;
  result.collectionMode = EWindowCollectionMode::CURRENT_ONLY;
  result.placement = "grid"; // Default to the original algorithm

  // Convert to lowercase for case-insensitive comparison
  std::string lowerArg = arg;
  std::transform(lowerArg.begin(), lowerArg.end(), lowerArg.begin(), ::tolower);

  // Determine action - check debug first since it can be combined with other keywords
  if (lowerArg.find("debug") != std::string::npos) {
    result.action = DispatcherArgs::Action::DEBUG;
  } else if (lowerArg == "select") {
    result.action = DispatcherArgs::Action::SELECT;
  } else if (lowerArg.find("off") != std::string::npos) {
    result.action = DispatcherArgs::Action::OFF;
  } else if (lowerArg.find("on") != std::string::npos) {
    result.action = DispatcherArgs::Action::ON;
  } else if (lowerArg.empty() || lowerArg.find("toggle") != std::string::npos) {
    result.action = DispatcherArgs::Action::TOGGLE;
  }

  // Check for "all" and "special" keywords (order-independent)
  bool hasAll = (lowerArg.find("all") != std::string::npos);
  bool hasSpecial = (lowerArg.find("special") != std::string::npos);

  // Determine collection mode
  if (hasAll && hasSpecial) {
    result.collectionMode = EWindowCollectionMode::ALL_WITH_SPECIAL;
  } else if (hasAll) {
    result.collectionMode = EWindowCollectionMode::ALL_WORKSPACES;
  } else if (hasSpecial) {
    result.collectionMode = EWindowCollectionMode::WITH_SPECIAL;
  } else {
    result.collectionMode = EWindowCollectionMode::CURRENT_ONLY;
  }

  // Parse the placement
  size_t placementPos = lowerArg.find("placement:");
  if (placementPos != std::string::npos) {
    size_t start = placementPos + 10; // length of "placement:"
    size_t end = lowerArg.find(' ', start);
    if (end == std::string::npos) {
      end = lowerArg.length();
    }
    result.placement = lowerArg.substr(start, end - start);

    // Validate placement algorithm
    if (!result.placement.empty() &&
        result.placement != "grid" &&
        result.placement != "spiral" &&
        result.placement != "flow" &&
        result.placement != "adaptive" &&
        result.placement != "wide") {
      result.error = "Invalid placement algorithm: " + result.placement + ". Valid options: grid, spiral, flow, adaptive, wide";
    }
  }

  // Parse the monitor target
  size_t monitorPos = lowerArg.find("monitor:");
  if (monitorPos != std::string::npos) {
    size_t start = monitorPos + 8; // length of "monitor:"
    size_t end = lowerArg.find(' ', start);
    if (end == std::string::npos) {
      end = lowerArg.length();
    }
    result.targetMonitor = arg.substr(start, end - start); // Use original arg for case-sensitive monitor name
  }

  return result;
}

// Helper to get target monitor(s)
static std::vector<PHLMONITOR> getTargetMonitors(const std::string& targetMonitorName) {
  std::vector<PHLMONITOR> targets;

  if (targetMonitorName.empty()) {
    // No specific monitor - use ALL enabled monitors
    for (auto &monitor : g_pCompositor->m_monitors) {
      if (monitor->m_enabled && monitor->m_activeWorkspace) {
        targets.push_back(monitor);
      }
    }
  } else {
    // Find monitor by name/description
    for (auto &monitor : g_pCompositor->m_monitors) {
      if (!monitor->m_enabled)
        continue;

      // Match by name or description
      if (monitor->m_name == targetMonitorName ||
          monitor->m_description.find(targetMonitorName) != std::string::npos) {
        targets.push_back(monitor);
      }
    }
  }

  return targets;
}

static SDispatchResult onHyprviewDispatcher(std::string arg) {
  Debug::log(LOG, "[hyprview] Dispatcher called with arg='{}'", arg);

  // Check if any instance is swiping
  for (auto &[monitor, instance] : g_pHyprViewInstances) {
    if (instance && instance->m_isSwiping)
      return {.success = false, .error = "already swiping"};
  }

  // Parse arguments
  DispatcherArgs parsedArgs = parseDispatcherArgs(arg);

  // Check for parsing errors
  if (!parsedArgs.error.empty()) {
    return {.success = false, .error = parsedArgs.error};
  }

  // Handle SELECT action
  if (parsedArgs.action == DispatcherArgs::Action::SELECT) {
    auto PMONITOR = g_pCompositor->m_lastMonitor.lock();
    if (PMONITOR) {
      auto it = g_pHyprViewInstances.find(PMONITOR);
      if (it != g_pHyprViewInstances.end() && it->second) {
        it->second->selectHoveredWindow();
        it->second->close();
      }
    }
    return {};
  }

  // Handle DEBUG action
  if (parsedArgs.action == DispatcherArgs::Action::DEBUG) {
    auto PMONITOR = g_pCompositor->m_lastMonitor.lock();
    if (!PMONITOR) {
      return {.success = false, .error = "No active monitor"};
    }

    // Write to a file instead of stdout since Hyprland doesn't have terminal attached
    std::string debugFile = "/tmp/hyprview_debug.txt";
    std::ofstream out(debugFile);

    if (!out.is_open()) {
      return {.success = false, .error = "Failed to open debug file"};
    }

    out << "\n========== HYPRVIEW DEBUG MODE ==========\n";
    out << "Monitor: " << PMONITOR->m_description << "\n";
    out << "Collection mode: " << (int)parsedArgs.collectionMode << "\n";
    out << "Placement algorithm: " << parsedArgs.placement << "\n";

    // Get monitor dimensions
    Vector2D reservedTopLeft = PMONITOR->m_reservedTopLeft;
    Vector2D reservedBottomRight = PMONITOR->m_reservedBottomRight;
    Vector2D fullMonitorSize = PMONITOR->m_pixelSize;
    Vector2D availableSize = {
        fullMonitorSize.x - reservedTopLeft.x - reservedBottomRight.x,
        fullMonitorSize.y - reservedTopLeft.y - reservedBottomRight.y
    };

    out << "\nMonitor Information:\n";
    out << "  Full size: " << fullMonitorSize.x << "x" << fullMonitorSize.y << "\n";
    out << "  Reserved top-left: " << reservedTopLeft.x << "x" << reservedTopLeft.y << "\n";
    out << "  Reserved bottom-right: " << reservedBottomRight.x << "x" << reservedBottomRight.y << "\n";
    out << "  Available size: " << availableSize.x << "x" << availableSize.y << "\n";

    // Collect windows
    auto activeWorkspace = PMONITOR->m_activeWorkspace;
    if (!activeWorkspace) {
      out.close();
      return {.success = false, .error = "No active workspace"};
    }

    auto shouldIncludeWindow = [&](PHLWINDOW w) -> bool {
      auto windowWorkspace = w->m_workspace;
      if (!windowWorkspace)
        return false;

      auto windowMonitor = w->m_monitor.lock();
      if (!windowMonitor || windowMonitor != PMONITOR)
        return false;

      switch (parsedArgs.collectionMode) {
      case EWindowCollectionMode::CURRENT_ONLY:
        return windowWorkspace == activeWorkspace;
      case EWindowCollectionMode::ALL_WORKSPACES:
        return !windowWorkspace->m_isSpecialWorkspace;
      case EWindowCollectionMode::WITH_SPECIAL:
        return windowWorkspace == activeWorkspace || windowWorkspace->m_isSpecialWorkspace;
      case EWindowCollectionMode::ALL_WITH_SPECIAL:
        return true;
      }
      return false;
    };

    std::vector<PHLWINDOW> windowsToRender;
    for (auto &w : g_pCompositor->m_windows) {
      if (!w->m_isMapped || w->isHidden())
        continue;
      if (!shouldIncludeWindow(w))
        continue;
      windowsToRender.push_back(w);
    }

    out << "\nWindows (" << windowsToRender.size() << " total):\n";

    // Convert to WindowInfo
    std::vector<WindowInfo> windowInfos;
    windowInfos.reserve(windowsToRender.size());
    for (size_t i = 0; i < windowsToRender.size(); ++i) {
      auto& w = windowsToRender[i];
      windowInfos.push_back({
        i,
        w->m_realSize->value().x,
        w->m_realSize->value().y
      });
      out << "  [" << i << "] \"" << w->m_title << "\" - "
          << w->m_realSize->value().x << "x" << w->m_realSize->value().y
          << " (WS: " << w->m_workspace->m_id << ")\n";
    }

    // Get margin from config
    static auto *const *PMARGIN = (Hyprlang::INT *const *)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprview:margin")->getDataStaticPtr();
    int margin = **PMARGIN;

    // Prepare screen info
    ScreenInfo screenInfo = {
      availableSize.x,
      availableSize.y,
      reservedTopLeft.x,
      reservedTopLeft.y,
      (double)margin
    };

    // Call placement algorithm
    PlacementResult placementResult;
    if (parsedArgs.placement == "spiral") {
      placementResult = spiralPlacement(windowInfos, screenInfo);
    } else if (parsedArgs.placement == "flow") {
      placementResult = flowPlacement(windowInfos, screenInfo);
    } else if (parsedArgs.placement == "adaptive") {
      placementResult = adaptivePlacement(windowInfos, screenInfo);
    } else if (parsedArgs.placement == "wide") {
      placementResult = widePlacement(windowInfos, screenInfo);
    } else {
      placementResult = gridPlacement(windowInfos, screenInfo);
    }

    out << "\nPlacement Result:\n";
    out << "  Grid: " << placementResult.gridCols << "x" << placementResult.gridRows << "\n";
    out << "  Margin: " << margin << "px\n";

    out << "\nTile Positions:\n";
    for (size_t i = 0; i < placementResult.tiles.size(); ++i) {
      const auto& tile = placementResult.tiles[i];
      auto& w = windowsToRender[i];
      out << "  [" << i << "] \"" << w->m_title << "\"\n"
          << "      Position: (" << tile.x << ", " << tile.y << ")\n"
          << "      Size: " << tile.width << "x" << tile.height << "\n";
    }

    out << "========== END DEBUG ==========\n\n";
    out.close();

    // Print to stdout as well for good measure
    std::cout << "Debug output written to: " << debugFile << std::endl;
    std::cout << "View with: cat " << debugFile << std::endl;
    std::cout.flush();

    // Also show where to find it
    std::string msg = "Debug output: cat /tmp/hyprview_debug.txt";
    system(("notify-send 'HyprView Debug' '" + msg + "'").c_str());

    return {};
  }

  // Handle OFF action
  if (parsedArgs.action == DispatcherArgs::Action::OFF) {
    auto targetMonitors = getTargetMonitors(parsedArgs.targetMonitor);

    if (targetMonitors.empty()) {
      return {.success = false, .error = "No matching monitor found"};
    }

    // Close overview on target monitors - OFF always closes regardless of explicitlyOn
    for (auto &targetMonitor : targetMonitors) {
      auto it = g_pHyprViewInstances.find(targetMonitor);
      if (it != g_pHyprViewInstances.end() && it->second) {
        it->second->close();
      }
    }

    g_pHyprRenderer->m_renderPass.removeAllOfType("CHyprViewPassElement");

    // Clean up closed instances
    for (auto it = g_pHyprViewInstances.begin(); it != g_pHyprViewInstances.end();) {
      if (it->second && it->second->closing) {
        it = g_pHyprViewInstances.erase(it);
      } else {
        ++it;
      }
    }

    return {};
  }

  // Handle ON action
  if (parsedArgs.action == DispatcherArgs::Action::ON) {
    Debug::log(LOG, "[hyprview] 'on' command called with mode={}", (int)parsedArgs.collectionMode);

    auto targetMonitors = getTargetMonitors(parsedArgs.targetMonitor);

    if (targetMonitors.empty()) {
      return {.success = false, .error = "No matching monitor found"};
    }

    // Open overview on target monitors only
    Debug::log(LOG, "[hyprview] Opening overview with mode={}", (int)parsedArgs.collectionMode);
    renderingOverview = true;
    for (auto &targetMonitor : targetMonitors) {
      if (targetMonitor->m_enabled && targetMonitor->m_activeWorkspace) {
        // Check if already active on this monitor
        auto it = g_pHyprViewInstances.find(targetMonitor);
        if (it != g_pHyprViewInstances.end() && it->second) {
          Debug::log(LOG, "[hyprview] Overview already active on monitor {}, skipping", targetMonitor->m_description);
          continue;
        }

        Debug::log(LOG, "[hyprview] Creating overview for monitor {} with mode={} and placement={}", targetMonitor->m_description, (int)parsedArgs.collectionMode, parsedArgs.placement);
        g_pHyprViewInstances[targetMonitor] = std::make_unique<CHyprView>(targetMonitor, targetMonitor->m_activeWorkspace, false, parsedArgs.collectionMode, parsedArgs.placement, true);
      }
    }
    renderingOverview = false;
    return {};
  }

  // Handle TOGGLE action
  if (parsedArgs.action == DispatcherArgs::Action::TOGGLE) {
    Debug::log(LOG, "[hyprview] Toggle called with mode={}", (int)parsedArgs.collectionMode);

    auto targetMonitors = getTargetMonitors(parsedArgs.targetMonitor);

    if (targetMonitors.empty()) {
      return {.success = false, .error = "No matching monitor found"};
    }

    // Check if any of the target monitors has an overview active (that can be toggled)
    // Monitors with explicitlyOn=true can ONLY be toggled if specifically targeted
    bool hasOverviewOnTarget = false;
    for (auto &targetMonitor : targetMonitors) {
      auto it = g_pHyprViewInstances.find(targetMonitor);
      if (it != g_pHyprViewInstances.end() && it->second) {
        // Count as toggleable if:
        // - Monitor was explicitly specified (can toggle even if explicitlyOn)
        // - OR instance is not explicitly on (general toggle affects it)
        bool isExplicitlyTargeted = !parsedArgs.targetMonitor.empty();
        bool canToggle = isExplicitlyTargeted || !it->second->explicitlyOn;

        if (canToggle) {
          hasOverviewOnTarget = true;
          break;
        }
      }
    }

    if (hasOverviewOnTarget) {
      // Close overviews on target monitors (only those that can be toggled)
      Debug::log(LOG, "[hyprview] Toggle: closing overviews on toggleable target monitors");

      for (auto &targetMonitor : targetMonitors) {
        auto it = g_pHyprViewInstances.find(targetMonitor);
        if (it != g_pHyprViewInstances.end() && it->second && !it->second->closing) {
          bool isExplicitlyTargeted = !parsedArgs.targetMonitor.empty();
          bool canToggle = isExplicitlyTargeted || !it->second->explicitlyOn;

          if (canToggle) {
            it->second->close();
          }
        }
      }

      g_pHyprRenderer->m_renderPass.removeAllOfType("CHyprViewPassElement");

      for (auto it = g_pHyprViewInstances.begin(); it != g_pHyprViewInstances.end();) {
        if (it->second && it->second->closing) {
          it = g_pHyprViewInstances.erase(it);
        } else {
          ++it;
        }
      }

    } else {
      // Open overview on target monitors
      Debug::log(LOG, "[hyprview] Toggle: opening overviews on target monitors with mode={}", (int)parsedArgs.collectionMode);

      renderingOverview = true;
      for (auto &targetMonitor : targetMonitors) {
        if (targetMonitor->m_enabled && targetMonitor->m_activeWorkspace) {
          // Do not open if an instance already exists to avoid overwriting explicitly-on overviews
          if (g_pHyprViewInstances.count(targetMonitor)) {
              Debug::log(LOG, "[hyprview] Toggle: skipping monitor {} as it already has an active overview", targetMonitor->m_description);
              continue;
          }
          Debug::log(LOG, "[hyprview] Creating overview for monitor {} with mode={} and placement={}", targetMonitor->m_description, (int)parsedArgs.collectionMode, parsedArgs.placement);
          g_pHyprViewInstances[targetMonitor] = std::make_unique<CHyprView>(targetMonitor, targetMonitor->m_activeWorkspace, false, parsedArgs.collectionMode, parsedArgs.placement);
        }
      }
      renderingOverview = false;
    }

    return {};
  }

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
    for (auto &[monitor, instance] : g_pHyprViewInstances) {
      if (instance)
        instance->onPreRender();
    }

    // Clean up closing instances
    for (auto it = g_pHyprViewInstances.begin(); it != g_pHyprViewInstances.end();) {
      if (it->second && it->second->closing) {

        g_pHyprRenderer->m_renderPass.removeAllOfType("CHyprViewPassElement");
        it = g_pHyprViewInstances.erase(it);
      } else {
        ++it;
      }
    }
  });

  // Block workspace gestures when overview is active
  static auto gestureBeginHook = HyprlandAPI::registerCallbackDynamic(PHANDLE, "swipeBegin", [](void *self, SCallbackInfo &info, std::any param) {
    // If any overview is active and it's not the hyprview gesture itself, cancel the gesture
    if (!g_pHyprViewInstances.empty()) {
      // Check if this is coming from a hyprview gesture - if so, allow it
      bool isHyprviewGesture = false;
      for (auto &[monitor, instance] : g_pHyprViewInstances) {
        if (instance && instance->swipe) {
          isHyprviewGesture = true;
          break;
        }
      }

      // If overview is active but this isn't a hyprview gesture, block it
      if (!isHyprviewGesture) {
        Debug::log(LOG, "[hyprview] Blocking workspace gesture while overview is active");
        info.cancelled = true;
      }
    }
  });

  static auto gestureUpdateHook = HyprlandAPI::registerCallbackDynamic(PHANDLE, "swipeUpdate", [](void *self, SCallbackInfo &info, std::any param) {
    // Block gesture updates when overview is active (unless it's the hyprview gesture)
    if (!g_pHyprViewInstances.empty()) {
      bool isHyprviewGesture = false;
      for (auto &[monitor, instance] : g_pHyprViewInstances) {
        if (instance && instance->swipe) {
          isHyprviewGesture = true;
          break;
        }
      }

      if (!isHyprviewGesture) {
        info.cancelled = true;
      }
    }
  });

  static auto gestureEndHook = HyprlandAPI::registerCallbackDynamic(PHANDLE, "swipeEnd", [](void *self, SCallbackInfo &info, std::any param) {
    // Block gesture end when overview is active (unless it's the hyprview gesture)
    if (!g_pHyprViewInstances.empty()) {
      bool isHyprviewGesture = false;
      for (auto &[monitor, instance] : g_pHyprViewInstances) {
        if (instance && instance->swipe) {
          isHyprviewGesture = true;
          break;
        }
      }

      if (!isHyprviewGesture) {
        info.cancelled = true;
      }
    }
  });

  HyprlandAPI::addDispatcherV2(PHANDLE, "hyprview:toggle", ::onHyprviewDispatcher);

  Debug::log(LOG, "[hyprview] Plugin initialized, dispatchers 'hyprview:toggle' registered");

  HyprlandAPI::addConfigKeyword(PHANDLE, "hyprview-gesture", ::hyprviewGestureKeyword, {});

  HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprview:margin", Hyprlang::INT{10});
  HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprview:gesture_distance", Hyprlang::INT{200});
  HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprview:active_border_color", Hyprlang::INT{0xFFCA7815});
  HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprview:inactive_border_color", Hyprlang::INT{0x88c0c0c0});
  HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprview:border_width", Hyprlang::INT{5});
  HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprview:border_radius", Hyprlang::INT{5});
  HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprview:bg_dim", Hyprlang::FLOAT{0.4});
  HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprview:workspace_indicator_enabled", Hyprlang::INT{1});
  HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprview:workspace_indicator_font_size", Hyprlang::INT{28});
  HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprview:workspace_indicator_position", Hyprlang::STRING{""});
  HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprview:workspace_indicator_bg_opacity", Hyprlang::FLOAT{0.85});
  HyprlandAPI::reloadConfig();

  return {"hyprview", "Window overview with multiple placement algorithms", "yz778", "0.1.5"};
}

APICALL EXPORT void PLUGIN_EXIT() {
  g_pHyprRenderer->m_renderPass.removeAllOfType("CHyprViewPassElement");
  g_unloading = true;
  g_pHyprViewInstances.clear();
  g_pConfigManager->reload();
}
