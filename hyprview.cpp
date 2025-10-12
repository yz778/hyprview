#include "hyprview.hpp"
#include <algorithm>
#include <any>
#include <unordered_set>
#define private public
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprland/src/debug/Log.hpp>
#include <hyprland/src/helpers/time/Time.hpp>
#include <hyprland/src/managers/KeybindManager.hpp>
#include <hyprland/src/managers/animation/AnimationManager.hpp>
#include <hyprland/src/managers/animation/DesktopAnimationManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/render/Renderer.hpp>
#undef private
#include "HyprViewPassElement.hpp"

// Helper to find the CHyprView instance for a given animation variable
CHyprView *findInstanceForAnimation(WP<Hyprutils::Animation::CBaseAnimatedVariable> thisptr) {
  for (auto &[monitor, instance] : g_pHypreViewInstances) {
    if (instance && (instance->size.get() == thisptr.lock().get() || instance->pos.get() == thisptr.lock().get())) {
      return instance.get();
    }
  }
  return nullptr;
}

static void damageMonitor(WP<Hyprutils::Animation::CBaseAnimatedVariable> thisptr) {
  auto *instance = findInstanceForAnimation(thisptr);
  if (instance)
    instance->damage();
}

void removeOverview(WP<Hyprutils::Animation::CBaseAnimatedVariable> thisptr) {
  LOG_FILE("=== removeOverview() CALLED ===");
  LOG_FILE(std::string("removeOverview(): g_pHypreViewInstances.size()=") + std::to_string(g_pHypreViewInstances.size()));

  // Find and remove the instance for this animation
  for (auto it = g_pHypreViewInstances.begin(); it != g_pHypreViewInstances.end(); ++it) {
    if (it->second && (it->second->size.get() == thisptr.lock().get() || it->second->pos.get() == thisptr.lock().get())) {
      LOG_FILE("removeOverview(): Found matching instance, erasing");
      g_pHypreViewInstances.erase(it);
      LOG_FILE("removeOverview(): Instance erased");
      break;
    }
  }
  LOG_FILE(std::string("removeOverview(): After erase, g_pHypreViewInstances.size()=") + std::to_string(g_pHypreViewInstances.size()));
  LOG_FILE("=== removeOverview() FINISHED ===");
}

CHyprView::~CHyprView() {
  LOG_FILE("=== ~CHyprView() DESTRUCTOR CALLED ===");
  LOG_FILE(std::string("~CHyprView(): images.size()=") + std::to_string(images.size()));

  // Restore all windows to their original workspaces
  LOG_FILE("~CHyprView(): Restoring windows to original workspaces");
  for (const auto &image : images) {
    auto window = image.pWindow.lock();
    if (window && image.originalWorkspace && window->m_workspace != image.originalWorkspace) {
      Debug::log(LOG, "[hyprview] Restoring window '{}' from workspace {} to {}",
                 window->m_title, window->m_workspace->m_id, image.originalWorkspace->m_id);
      window->moveToWorkspace(image.originalWorkspace);
    }
  }
  LOG_FILE("~CHyprView(): Workspace restoration complete");

  g_pHyprRenderer->makeEGLCurrent();
  LOG_FILE("~CHyprView(): makeEGLCurrent() done");

  images.clear();
  LOG_FILE("~CHyprView(): images.clear() done");

  g_pInputManager->unsetCursorImage();
  LOG_FILE("~CHyprView(): unsetCursorImage() done");

  g_pHyprOpenGL->markBlurDirtyForMonitor(pMonitor.lock());
  LOG_FILE("~CHyprView(): markBlurDirtyForMonitor() done");
  LOG_FILE("=== ~CHyprView() DESTRUCTOR FINISHED ===");
}

CHyprView::CHyprView(PHLMONITOR pMonitor_, PHLWORKSPACE startedOn_, bool swipe_, EWindowCollectionMode mode) : pMonitor(pMonitor_), startedOn(startedOn_), swipe(swipe_), m_collectionMode(mode) {

  // Block rendering until we finish moving windows to active workspace
  // This ensures the overview layer is created AFTER workspace migration
  blockOverviewRendering = true;

  originalFocusedWindow = g_pCompositor->m_lastWindow;
  userExplicitlySelected = false;

  auto origWindow = originalFocusedWindow.lock();
  Debug::log(LOG, "[hyprview] CHyprView(): Saved original focused window: {}", (void *)origWindow.get());

  static auto *const *PMARGIN = (Hyprlang::INT *const *)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprview:margin")->getDataStaticPtr();
  static auto *const *PPADDING = (Hyprlang::INT *const *)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprview:padding")->getDataStaticPtr();
  static auto *const *PCOL = (Hyprlang::INT *const *)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprview:bg_color")->getDataStaticPtr();
  static auto *const *PGRIDCOL = (Hyprlang::INT *const *)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprview:grid_color")->getDataStaticPtr();

  MARGIN = **PMARGIN;
  PADDING = **PPADDING;
  BG_COLOR = **PCOL;
  GRID_COLOR = **PGRIDCOL;

  static auto *const *PACTIVEBORDERCOL = (Hyprlang::INT *const *)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprview:active_border_color")->getDataStaticPtr();
  static auto *const *PINACTIVEBORDERCOL = (Hyprlang::INT *const *)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprview:inactive_border_color")->getDataStaticPtr();
  static auto *const *PBORDERWIDTH = (Hyprlang::INT *const *)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprview:border_width")->getDataStaticPtr();
  static auto *const *PBORDERRADIUS = (Hyprlang::INT *const *)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprview:border_radius")->getDataStaticPtr();

  ACTIVE_BORDER_COLOR = **PACTIVEBORDERCOL;
  INACTIVE_BORDER_COLOR = **PINACTIVEBORDERCOL;
  BORDER_WIDTH = **PBORDERWIDTH;
  BORDER_RADIUS = **PBORDERRADIUS;

  std::vector<PHLWINDOW> windowsToRender;

  PHLWORKSPACE activeWorkspace = pMonitor->m_activeWorkspace;

  if (!activeWorkspace)
    return;

  Debug::log(LOG,
             "[hyprview] CHyprView(): Collecting windows for monitor '{}', "
             "workspace ID {}, mode={}",
             pMonitor->m_description, activeWorkspace->m_id, (int)m_collectionMode);

  // Lambda to check if window should be included based on collection mode
  auto shouldIncludeWindow = [&](PHLWINDOW w) -> bool {
    auto windowWorkspace = w->m_workspace;
    if (!windowWorkspace)
      return false;

    auto windowMonitor = w->m_monitor.lock();
    if (!windowMonitor || windowMonitor != pMonitor.lock())
      return false;

    switch (m_collectionMode) {
    case EWindowCollectionMode::CURRENT_ONLY:
      // Only current workspace
      return windowWorkspace == activeWorkspace;

    case EWindowCollectionMode::ALL_WORKSPACES:
      // All workspaces on monitor, excluding special
      return !windowWorkspace->m_isSpecialWorkspace;

    case EWindowCollectionMode::WITH_SPECIAL:
      // Current workspace + special workspace
      return windowWorkspace == activeWorkspace || windowWorkspace->m_isSpecialWorkspace;

    case EWindowCollectionMode::ALL_WITH_SPECIAL:
      // All workspaces on monitor including special
      return true;
    }
    return false;
  };

  for (auto &w : g_pCompositor->m_windows) {
    if (!w->m_isMapped || w->isHidden())
      continue;

    if (!shouldIncludeWindow(w))
      continue;

    Debug::log(LOG, "[hyprview] Adding window '{}' from monitor '{}', workspace {}", w->m_title, pMonitor->m_description, w->m_workspace->m_id);
    windowsToRender.push_back(w);
  }

  // Sort windows: current workspace first, then by X then Y
  std::stable_sort(windowsToRender.begin(), windowsToRender.end(), [&activeWorkspace](const PHLWINDOW &a, const PHLWINDOW &b) {
    auto wsA = a->m_workspace;
    auto wsB = b->m_workspace;

    // Priority 1: Current workspace first
    bool aIsCurrent = (wsA == activeWorkspace);
    bool bIsCurrent = (wsB == activeWorkspace);
    if (aIsCurrent != bIsCurrent)
      return aIsCurrent; // Current workspace windows come first

    // Priority 2: Within same workspace group, sort by workspace ID
    if (wsA != wsB)
      return wsA->m_id < wsB->m_id;

    // Priority 3: Within same workspace, sort by X then Y (changed from Y then X)
    if (a->m_realPosition->value().x != b->m_realPosition->value().x)
      return a->m_realPosition->value().x < b->m_realPosition->value().x;
    return a->m_realPosition->value().y < b->m_realPosition->value().y;
  });

  Debug::log(LOG, "[hyprview] CHyprView(): Collected {} windows for monitor '{}'", windowsToRender.size(), pMonitor->m_description);

  const size_t windowCount = windowsToRender.size();

  if (windowCount < 3) {
    SIDE_LENGTH = 2;
    GRID_ROWS = 1;
  } else if (windowCount < 5) {
    SIDE_LENGTH = 2;
    GRID_ROWS = 2;
  } else {
    SIDE_LENGTH = 3;
    GRID_ROWS = (windowCount + SIDE_LENGTH - 1) / SIDE_LENGTH;
  }

  Debug::log(LOG, "[hyprview] Using {}x{} grid for {} windows", SIDE_LENGTH, GRID_ROWS, windowCount);

  const size_t maxWindows = SIDE_LENGTH * GRID_ROWS;
  const size_t numWindows = std::min(windowsToRender.size(), maxWindows);
  images.resize(numWindows);

  g_pHyprRenderer->makeEGLCurrent();

  Vector2D tileSize = {pMonitor->m_size.x / SIDE_LENGTH, pMonitor->m_size.y / GRID_ROWS};
  Vector2D tileRenderSize = tileSize - Vector2D{2.0 * (MARGIN + PADDING), 2.0 * (MARGIN + PADDING)};

  Debug::log(LOG,
             "[hyprview] Monitor size: {}x{}, Grid: {}x{}, Tile size: {}x{}, "
             "Render size: {}x{}",
             pMonitor->m_size.x, pMonitor->m_size.y, SIDE_LENGTH, GRID_ROWS, tileSize.x, tileSize.y, tileRenderSize.x, tileRenderSize.y);

  g_pHyprRenderer->m_bBlockSurfaceFeedback = true;

  int currentid = -1;

  // Save original workspaces BEFORE moving
  std::unordered_map<PHLWINDOW, PHLWORKSPACE> originalWorkspaces;
  for (auto &window : windowsToRender) {
    originalWorkspaces[window] = window->m_workspace;
  }

  // Move windows to active workspace so they have valid surfaces for rendering
  for (auto &window : windowsToRender) {
    if (window->m_workspace != activeWorkspace) {
      Debug::log(LOG, "[hyprview] Moving window '{}' from workspace {} to {}",
                 window->m_title, window->m_workspace->m_id, activeWorkspace->m_id);
      window->moveToWorkspace(activeWorkspace);
    }
  }

  // Render all windows to framebuffers
  for (size_t i = 0; i < numWindows; ++i) {
    CHyprView::SWindowImage &image = images[i];
    auto &window = windowsToRender[i];

    image.pWindow = window;
    image.originalPos = window->m_realPosition->value();
    image.originalSize = window->m_realSize->value();
    image.originalWorkspace = originalWorkspaces[window]; // Save the original workspace

    image.box = {(i % SIDE_LENGTH) * tileSize.x + (MARGIN + PADDING), (i / SIDE_LENGTH) * tileSize.y + (MARGIN + PADDING), tileRenderSize.x, tileRenderSize.y};

    const auto RENDERSIZE = (window->m_realSize->value() * pMonitor->m_scale).floor();
    image.fb.alloc(std::max(1.0, RENDERSIZE.x), std::max(1.0, RENDERSIZE.y), pMonitor->m_output->state->state().drmFormat);

    CRegion fakeDamage{0, 0, INT16_MAX, INT16_MAX};

    const auto REALPOS = window->m_realPosition->value();

    Debug::log(LOG, "[hyprview] Rendering window '{}' - workspace: {}, surfaceCount: {}",
               window->m_title, window->m_workspace->m_id, window->surfacesCount());

    // Temporarily move window to monitor position for rendering
    window->m_realPosition->setValue(pMonitor->m_position);

    g_pHyprRenderer->beginRender(pMonitor.lock(), fakeDamage, RENDER_MODE_FULL_FAKE, nullptr, &image.fb);

    g_pHyprOpenGL->clear(GRID_COLOR.stripA());

    // Now all windows should have valid surfaces since they're on the active workspace
    if (window && window->m_isMapped) {
      g_pHyprRenderer->renderWindow(window, pMonitor.lock(), Time::steadyNow(), false, RENDER_PASS_MAIN, false, false);
    }

    g_pHyprOpenGL->m_renderData.blockScreenShader = true;
    g_pHyprRenderer->endRender();

    // Restore original position
    window->m_realPosition->setValue(REALPOS);
  }

  g_pHyprRenderer->m_bBlockSurfaceFeedback = false;

  if (numWindows > 0)
    currentid = 0;

  g_pAnimationManager->createAnimation(pMonitor->m_size * pMonitor->m_size / tileSize, size, g_pConfigManager->getAnimationPropertyConfig("windowsMove"), AVARDAMAGE_NONE);
  g_pAnimationManager->createAnimation((-((pMonitor->m_size / (double)SIDE_LENGTH) * Vector2D{currentid % SIDE_LENGTH, currentid / SIDE_LENGTH}) * pMonitor->m_scale) * (pMonitor->m_size / tileSize), pos, g_pConfigManager->getAnimationPropertyConfig("windowsMove"), AVARDAMAGE_NONE);

  size->setUpdateCallback(damageMonitor);
  pos->setUpdateCallback(damageMonitor);

  if (!swipe) {
    *size = pMonitor->m_size;
    *pos = {0, 0};
  }

  openedID = currentid;

  g_pInputManager->setCursorImageUntilUnset("left_ptr");

  lastMousePosLocal = g_pInputManager->getMouseCoordsInternal() - pMonitor->m_position;

  auto onCursorMove = [this](void *self, SCallbackInfo &info, std::any param) {
    if (closing)
      return;

    info.cancelled = true;

    // Check if mouse is actually on this monitor
    Vector2D globalMousePos = g_pInputManager->getMouseCoordsInternal();
    Vector2D monitorPos = pMonitor->m_position;
    Vector2D monitorSize = pMonitor->m_size;

    // Only handle if mouse is on this monitor
    if (globalMousePos.x < monitorPos.x || globalMousePos.x >= monitorPos.x + monitorSize.x ||
        globalMousePos.y < monitorPos.y || globalMousePos.y >= monitorPos.y + monitorSize.y) {
      return; // Mouse is on a different monitor
    }

    lastMousePosLocal = globalMousePos - monitorPos;

    if (!images.empty()) {
      int x = lastMousePosLocal.x / monitorSize.x * SIDE_LENGTH;
      int y = lastMousePosLocal.y / monitorSize.y * SIDE_LENGTH;
      int tileIndex = x + y * SIDE_LENGTH;

      if (tileIndex >= 0 && tileIndex < (int)images.size()) {
        auto window = images[tileIndex].pWindow.lock();

        if (window && window->m_isMapped) {
          auto lastHovered = lastHoveredWindow.lock();
          auto currentFocus = g_pCompositor->m_lastWindow.lock();

          // Always ensure focus is on the hovered window
          // Check both if it's a different window OR if compositor focus has drifted
          bool needsFocusUpdate = (window != lastHovered) || (window != currentFocus);

          if (needsFocusUpdate) {
            Debug::log(LOG, "[hyprview] Updating focus: {} (prev: {}, compositor: {})",
                       window->m_title,
                       lastHovered ? lastHovered->m_title : "none",
                       currentFocus ? currentFocus->m_title : "none");
            g_pCompositor->focusWindow(window);
            lastHoveredWindow = window;
          }
        }
      }
    }
  };

  auto onCursorSelect = [this](void *self, SCallbackInfo &info, std::any param) {
    if (closing)
      return;

    info.cancelled = true;

    selectHoveredWindow();

    // Use immediate close and let the dispatcher handle cleanup
    closeImmediate();
  };

  auto onMouseAxis = [this](void *self, SCallbackInfo &info, std::any param) {
    if (closing)
      return;

    // The focused window should already be correct from onCursorMove
    // Just let the scroll event pass through to the compositor's focused window
    // No need to do anything - scroll will naturally go to g_pCompositor->m_lastWindow
  };

  mouseMoveHook = g_pHookSystem->hookDynamic("mouseMove", onCursorMove);
  touchMoveHook = g_pHookSystem->hookDynamic("touchMove", onCursorMove);

  mouseButtonHook = g_pHookSystem->hookDynamic("mouseButton", onCursorSelect);
  mouseAxisHook = g_pHookSystem->hookDynamic("mouseAxis", onMouseAxis);
  touchDownHook = g_pHookSystem->hookDynamic("touchDown", onCursorSelect);

  // NOW unblock rendering - workspace migration is complete
  // The overview layer will be created on the next render pass
  blockOverviewRendering = false;
  Debug::log(LOG, "[hyprview] CHyprView(): Constructor complete, unblocked rendering");
}

void CHyprView::selectHoveredWindow() {
  if (closing)
    return;

  int x = lastMousePosLocal.x / pMonitor->m_size.x * SIDE_LENGTH;
  int y = lastMousePosLocal.y / pMonitor->m_size.y * SIDE_LENGTH;
  closeOnID = x + y * SIDE_LENGTH;

  if (closeOnID >= (int)images.size())
    closeOnID = images.size() - 1;
  if (closeOnID < 0)
    closeOnID = 0;

  userExplicitlySelected = true;

  Debug::log(LOG,
             "[hyprview] selectHoveredWindow(): User explicitly selected "
             "window at index {}",
             closeOnID);
}

void CHyprView::redrawID(int id, bool forcelowres) {
  blockOverviewRendering = true;

  g_pHyprRenderer->makeEGLCurrent();

  if (id >= (int)images.size())
    id = images.size() - 1;
  if (id < 0)
    id = 0;

  auto &image = images[id];
  auto window = image.pWindow.lock();
  if (!window) {
    blockOverviewRendering = false;
    return;
  }

  const auto RENDERSIZE = (window->m_realSize->value() * pMonitor->m_scale).floor();
  if (RENDERSIZE.x < 1 || RENDERSIZE.y < 1) {
    blockOverviewRendering = false;
    return;
  }

  if (image.fb.m_size.x != RENDERSIZE.x || image.fb.m_size.y != RENDERSIZE.y) {
    image.fb.release();
    image.fb.alloc(RENDERSIZE.x, RENDERSIZE.y, pMonitor->m_output->state->state().drmFormat);
  }

  CRegion fakeDamage{0, 0, INT16_MAX, INT16_MAX};

  const auto REALPOS = window->m_realPosition->value();
  window->m_realPosition->setValue(pMonitor->m_position);

  g_pHyprRenderer->beginRender(pMonitor.lock(), fakeDamage, RENDER_MODE_FULL_FAKE, nullptr, &image.fb);

  g_pHyprOpenGL->clear(GRID_COLOR.stripA());

  if (window->m_isMapped) {
    g_pHyprRenderer->renderWindow(window, pMonitor.lock(), Time::steadyNow(), false, RENDER_PASS_MAIN, false, false);
  }

  g_pHyprOpenGL->m_renderData.blockScreenShader = true;
  g_pHyprRenderer->endRender();

  window->m_realPosition->setValue(REALPOS);

  blockOverviewRendering = false;
}

void CHyprView::redrawAll(bool forcelowres) {
  for (size_t i = 0; i < images.size(); ++i) {
    redrawID(i, forcelowres);
  }
}

void CHyprView::damage() {
  blockDamageReporting = true;
  g_pHyprRenderer->damageMonitor(pMonitor.lock());
  blockDamageReporting = false;
}

void CHyprView::onDamageReported() {
  damageDirty = true;

  Vector2D SIZE = size->value();

  Vector2D tileSize = {SIZE.x / SIDE_LENGTH, SIZE.y / GRID_ROWS};
  Vector2D tileRenderSize = tileSize - Vector2D{2.0 * (MARGIN + PADDING), 2.0 * (MARGIN + PADDING)};
  CBox texbox = CBox{(openedID % SIDE_LENGTH) * tileSize.x + (MARGIN + PADDING), (openedID / SIDE_LENGTH) * tileSize.y + (MARGIN + PADDING), tileRenderSize.x, tileRenderSize.y}.translate(pMonitor->m_position);

  damage();

  blockDamageReporting = true;
  g_pHyprRenderer->damageBox(texbox);
  blockDamageReporting = false;
  g_pCompositor->scheduleFrameForMonitor(pMonitor.lock());
}

void CHyprView::close() {
  LOG_FILE("=== CLOSE() CALLED ===");

  if (closing) {
    LOG_FILE("close(): already closing, returning");
    Debug::log(LOG, "[hyprview] close(): already closing, returning");
    return;
  }

  const int ID = closeOnID == -1 ? openedID : closeOnID;
  LOG_FILE(std::string("close(): starting - ID=") + std::to_string(ID) + ", closeOnID=" + std::to_string(closeOnID) + ", openedID=" + std::to_string(openedID) + ", images.size()=" + std::to_string(images.size()));
  Debug::log(LOG,
             "[hyprview] close(): starting close, ID={}, closeOnID={}, "
             "openedID={}, images.size()={}",
             ID, closeOnID, openedID, images.size());

  // Handle case when there are no windows (images is empty)
  if (images.empty()) {
    LOG_FILE("close(): No windows on this monitor, closing without animation");
    Debug::log(LOG, "[hyprview] close(): No windows on this monitor, closing "
                    "without animation");
    closing = true;
    LOG_FILE("close(): Empty images case - set closing=true (no callback)");
    return;
  }

  const int clampedID = std::clamp(ID, 0, (int)images.size() - 1);
  LOG_FILE(std::string("close(): clampedID=") + std::to_string(clampedID));

  if (clampedID >= 0 && clampedID < (int)images.size()) {
    LOG_FILE("close(): Accessing images[clampedID]");
    const auto &TILE = images[clampedID];
    LOG_FILE("close(): Got TILE reference");

    Vector2D tileSize = (pMonitor->m_size / SIDE_LENGTH);
    LOG_FILE("close(): Calculated tileSize");

    *size = pMonitor->m_size * pMonitor->m_size / tileSize;
    *pos = (-((pMonitor->m_size / (double)SIDE_LENGTH) * Vector2D{clampedID % SIDE_LENGTH, clampedID / SIDE_LENGTH}) * pMonitor->m_scale) * (pMonitor->m_size / tileSize);
    LOG_FILE("close(): Set size and pos animations");

    size->setCallbackOnEnd(removeOverview);
    LOG_FILE("close(): Set animation callback");

    closing = true;
    LOG_FILE("close(): Set closing=true");

    // STEP 1: Restore ALL windows to their original workspaces FIRST
    // This ensures that when we focus a window, the workspace switch happens naturally
    LOG_FILE("close(): Restoring all windows to original workspaces");
    Debug::log(LOG, "[hyprview] close(): Restoring all windows to original workspaces");
    for (const auto &image : images) {
      auto window = image.pWindow.lock();
      if (window && image.originalWorkspace && window->m_workspace != image.originalWorkspace) {
        Debug::log(LOG, "[hyprview] close(): Restoring window '{}' from workspace {} to {}",
                   window->m_title, window->m_workspace->m_id, image.originalWorkspace->m_id);
        window->moveToWorkspace(image.originalWorkspace);
      }
    }
    LOG_FILE("close(): Workspace restoration complete");

    // STEP 2: Now focus the selected window (workspace will follow automatically)
    if (userExplicitlySelected) {
      LOG_FILE("close(): User explicitly selected window");
      auto window = TILE.pWindow.lock();
      if (window && window->m_isMapped) {
        LOG_FILE(std::string("close(): Focusing window: ") + window->m_title);
        Debug::log(LOG, "[hyprview] close(): User selected window, focusing and bringing to top");
        g_pCompositor->focusWindow(window);
        LOG_FILE("close(): Called focusWindow()");

        g_pKeybindManager->alterZOrder("top");
        LOG_FILE("close(): Called alterZOrder(top)");
      } else {
        LOG_FILE("close(): Selected window is null or not mapped");
        Debug::log(LOG, "[hyprview] close(): Selected window is null or not mapped");
      }
    } else {
      LOG_FILE("close(): No explicit selection, restoring original focus");
      auto origWindow = originalFocusedWindow.lock();
      if (origWindow && origWindow->m_isMapped) {
        LOG_FILE(std::string("close(): Restoring focus to: ") + origWindow->m_title);
        Debug::log(LOG, "[hyprview] close(): No explicit selection, restoring "
                        "original window focus");
        g_pCompositor->focusWindow(origWindow);
        LOG_FILE("close(): Restored focus");
      } else {
        LOG_FILE("close(): Original window no longer valid");
        Debug::log(LOG, "[hyprview] close(): Original window no longer valid, "
                        "not changing focus");
      }
    }
    LOG_FILE("close(): Finished focus handling");
  } else {
    LOG_FILE("close(): clampedID out of range - no windows to select");
    size->setCallbackOnEnd(removeOverview);
    closing = true;
  }
  LOG_FILE("=== CLOSE() FINISHED ===");
}

void CHyprView::closeImmediate() {
  LOG_FILE("=== CLOSEIMMEDIATE() CALLED ===");

  if (closing) {
    LOG_FILE("closeImmediate(): already closing, returning");
    return;
  }

  closing = true;

  // STEP 1: Restore ALL windows to their original workspaces FIRST
  // This ensures that when we focus a window, the workspace switch happens naturally
  LOG_FILE("closeImmediate(): Restoring all windows to original workspaces");
  Debug::log(LOG, "[hyprview] closeImmediate(): Restoring all windows to original workspaces");
  for (const auto &image : images) {
    auto window = image.pWindow.lock();
    if (window && image.originalWorkspace && window->m_workspace != image.originalWorkspace) {
      Debug::log(LOG, "[hyprview] closeImmediate(): Restoring window '{}' from workspace {} to {}",
                 window->m_title, window->m_workspace->m_id, image.originalWorkspace->m_id);
      window->moveToWorkspace(image.originalWorkspace);
    }
  }
  LOG_FILE("closeImmediate(): Workspace restoration complete");

  // STEP 2: Now focus the selected window (workspace will follow automatically)
  if (userExplicitlySelected && closeOnID >= 0 && closeOnID < (int)images.size()) {
    LOG_FILE("closeImmediate(): User explicitly selected window");
    const auto &TILE = images[closeOnID];
    auto window = TILE.pWindow.lock();
    if (window && window->m_isMapped) {
      LOG_FILE(std::string("closeImmediate(): Focusing window: ") + window->m_title);
      Debug::log(LOG, "[hyprview] closeImmediate(): User selected window, focusing and bringing to top");
      g_pCompositor->focusWindow(window);
      g_pKeybindManager->alterZOrder("top");
    }
  } else {
    LOG_FILE("closeImmediate(): No explicit selection, restoring original focus");
    auto origWindow = originalFocusedWindow.lock();
    if (origWindow && origWindow->m_isMapped) {
      LOG_FILE(std::string("closeImmediate(): Restoring focus to: ") + origWindow->m_title);
      Debug::log(LOG, "[hyprview] closeImmediate(): Restoring original window focus");
      g_pCompositor->focusWindow(origWindow);
    }
  }

  LOG_FILE("=== CLOSEIMMEDIATE() FINISHED ===");
}

void CHyprView::closeWithoutFocus() {
  if (closing)
    return;

  size->setCallbackOnEnd(removeOverview);
  closing = true;
}

void CHyprView::onPreRender() {
  if (damageDirty && !closing) {
    damageDirty = false;
    redrawAll(false);
  }
}

void CHyprView::onWorkspaceChange() {}

void CHyprView::render() { g_pHyprRenderer->m_renderPass.add(makeUnique<CHyprViewPassElement>(this)); }

void CHyprView::fullRender() {
  const auto MARGINSIZE = (closing ? (1.0 - size->getPercent()) : size->getPercent()) * MARGIN;
  const auto PADDINGSIZE = (closing ? (1.0 - size->getPercent()) : size->getPercent()) * PADDING;

  Vector2D SIZE = size->value();

  Vector2D tileSize = {SIZE.x / SIDE_LENGTH, SIZE.y / GRID_ROWS};
  Vector2D tileRenderSize = tileSize - Vector2D{2.0 * (MARGINSIZE + PADDINGSIZE), 2.0 * (MARGINSIZE + PADDINGSIZE)};

  CBox monitorBox = {0, 0, pMonitor->m_size.x, pMonitor->m_size.y};
  CHyprOpenGLImpl::SRectRenderData bgData;
  // Render fully opaque background to mask windows underneath
  g_pHyprOpenGL->renderRect(monitorBox, CHyprColor(0.0f, 0.0f, 0.0f, 1.0f), bgData);

  const auto PLASTWINDOW = g_pCompositor->m_lastWindow.lock();

  for (size_t i = 0; i < images.size(); ++i) {
    size_t x = i % SIDE_LENGTH;
    size_t y = i / SIDE_LENGTH;

    const Vector2D &textureSize = images[i].fb.m_size;

    if (textureSize.x < 1 || textureSize.y < 1)
      continue;

    const double textureAspect = textureSize.x / textureSize.y;
    const double cellAspect = tileRenderSize.x / tileRenderSize.y;

    Vector2D newSize;
    if (textureAspect > cellAspect) {
      newSize.x = tileRenderSize.x;
      newSize.y = newSize.x / textureAspect;
    } else {
      newSize.y = tileRenderSize.y;
      newSize.x = newSize.y * textureAspect;
    }

    const double cellX = x * tileSize.x + (MARGINSIZE + PADDINGSIZE);
    const double cellY = y * tileSize.y + (MARGINSIZE + PADDINGSIZE);
    const double offsetX = (tileRenderSize.x - newSize.x) / 2.0;
    const double offsetY = (tileRenderSize.y - newSize.y) / 2.0;

    CBox windowBox = {cellX + offsetX, cellY + offsetY, newSize.x, newSize.y};
    CBox borderBox = {windowBox.x - BORDER_WIDTH, windowBox.y - BORDER_WIDTH, windowBox.width + 2 * BORDER_WIDTH, windowBox.height + 2 * BORDER_WIDTH};

    const bool ISACTIVE = images[i].pWindow.lock() == PLASTWINDOW;
    const auto &BORDERCOLOR = ISACTIVE ? ACTIVE_BORDER_COLOR : INACTIVE_BORDER_COLOR;

    // Translate both boxes for the overview animation
    borderBox.translate(pos->value());
    borderBox.round();
    windowBox.translate(pos->value());
    windowBox.round();

    CHyprOpenGLImpl::SRectRenderData data;
    data.round = BORDER_RADIUS;
    g_pHyprOpenGL->renderRect(borderBox, BORDERCOLOR, data);

    CRegion damage{0, 0, INT16_MAX, INT16_MAX};
    g_pHyprOpenGL->renderTextureInternal(images[i].fb.getTexture(), windowBox, {.damage = &damage, .a = 1.0, .round = BORDER_RADIUS});
  }
}

static float lerp(const float &from, const float &to, const float perc) { return (to - from) * perc + from; }

static Vector2D lerp(const Vector2D &from, const Vector2D &to, const float perc) { return Vector2D{lerp(from.x, to.x, perc), lerp(from.y, to.y, perc)}; }

void CHyprView::setClosing(bool closing_) { closing = closing_; }

void CHyprView::resetSwipe() { swipeWasCommenced = false; }

void CHyprView::onSwipeUpdate(double delta) {
  m_isSwiping = true;

  if (swipeWasCommenced)
    return;

  static auto *const *PDISTANCE = (Hyprlang::INT *const *)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprview:gesture_distance")->getDataStaticPtr();

  const float PERC = closing ? std::clamp(delta / (double)**PDISTANCE, 0.0, 1.0) : 1.0 - std::clamp(delta / (double)**PDISTANCE, 0.0, 1.0);
  const auto WORKSPACE_FOCUS_ID = closing && closeOnID != -1 ? closeOnID : openedID;

  Vector2D tileSize = {pMonitor->m_size.x / SIDE_LENGTH, pMonitor->m_size.y / GRID_ROWS};

  const auto SIZEMAX = pMonitor->m_size * pMonitor->m_size / tileSize;
  const auto POSMAX = (-((pMonitor->m_size / (double)SIDE_LENGTH) * Vector2D{WORKSPACE_FOCUS_ID % SIDE_LENGTH, WORKSPACE_FOCUS_ID / SIDE_LENGTH}) * pMonitor->m_scale) * (pMonitor->m_size / tileSize);

  const auto SIZEMIN = pMonitor->m_size;
  const auto POSMIN = Vector2D{0, 0};

  size->setValueAndWarp(lerp(SIZEMIN, SIZEMAX, PERC));
  pos->setValueAndWarp(lerp(POSMIN, POSMAX, PERC));
}

void CHyprView::onSwipeEnd() {
  const auto SIZEMIN = pMonitor->m_size;
  Vector2D tileSize = {pMonitor->m_size.x / SIDE_LENGTH, pMonitor->m_size.y / GRID_ROWS};
  const auto SIZEMAX = pMonitor->m_size * pMonitor->m_size / tileSize;
  const auto PERC = (size->value() - SIZEMIN).x / (SIZEMAX - SIZEMIN).x;
  if (PERC > 0.5) {
    close();
    return;
  }
  *size = pMonitor->m_size;
  *pos = {0, 0};

  swipeWasCommenced = true;
  m_isSwiping = false;
}
