#include "hyprview.hpp"
#include <algorithm>
#include <any>
#include <cmath>
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
  for (auto &[monitor, instance] : g_pHyprViewInstances) {
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

void CHyprView::calculateAdaptiveRowHeights(size_t windowCount, double screenHeight) {
  rowHeights.clear();
  rowYPositions.clear();
  windowsPerRow.clear();

  if (windowCount == 0 || GRID_ROWS == 0) {
    return;
  }

  windowsPerRow.resize(GRID_ROWS, 0);
  for (size_t i = 0; i < windowCount; ++i) {
    int row = i / SIDE_LENGTH;
    if (row < GRID_ROWS) {
      windowsPerRow[row]++;
    }
  }

  double uniformHeight = screenHeight / GRID_ROWS;
  double currentY = 0.0;
  for (int i = 0; i < GRID_ROWS; ++i) {
    rowYPositions.push_back(currentY);
    rowHeights.push_back(uniformHeight);
    currentY += uniformHeight;
  }
  Debug::log(LOG, "[hyprview] Grid: {}x{} for {} windows, rowHeight={}", SIDE_LENGTH, GRID_ROWS, windowCount, uniformHeight);
}

CHyprView::~CHyprView() {

  // Restore all windows to their original workspaces

  for (const auto &image : images) {
    auto window = image.pWindow.lock();
    if (window && image.originalWorkspace && window->m_workspace != image.originalWorkspace) {
      Debug::log(LOG, "[hyprview] Restoring window '{}' from workspace {} to {}", window->m_title, window->m_workspace->m_id, image.originalWorkspace->m_id);
      window->moveToWorkspace(image.originalWorkspace);
    }
  }

  g_pHyprRenderer->makeEGLCurrent();

  images.clear();

  // Also clear the background framebuffer
  bgFramebuffer.release();

  g_pInputManager->unsetCursorImage();

  g_pHyprOpenGL->markBlurDirtyForMonitor(pMonitor.lock());
}

void CHyprView::captureBackground() {
  auto monitor = pMonitor.lock();
  if (!monitor)
    return;

  // Capture background at full monitor size to avoid recursive layers
  const auto MONITOR_SIZE = monitor->m_pixelSize;
  const auto FORMAT = monitor->m_output->state->state().drmFormat;

  // Allocate the background framebuffer at full size
  bgFramebuffer.alloc(MONITOR_SIZE.x, MONITOR_SIZE.y, FORMAT);

  // Get current workspace
  auto activeWorkspace = monitor->m_activeWorkspace;
  if (!activeWorkspace)
    return;

  // Temporarily hide all windows on this monitor
  std::vector<PHLWINDOW> hiddenWindows;
  std::vector<bool> originalHiddenStates;

  for (auto &w : g_pCompositor->m_windows) {
    if (!w->m_isMapped || w->isHidden() || w->m_monitor.lock() != monitor)
      continue;

    // Hide windows from current and special workspaces temporarily
    if (w->m_workspace == activeWorkspace || w->m_workspace->m_isSpecialWorkspace) {
      hiddenWindows.push_back(w);
      originalHiddenStates.push_back(w->m_hidden);
      w->m_hidden = true;
    }
  }

  // Capture the background with hidden windows
  CRegion fullRegion{0, 0, (int)MONITOR_SIZE.x, (int)MONITOR_SIZE.y};
  g_pHyprRenderer->beginRender(monitor, fullRegion, RENDER_MODE_FULL_FAKE, nullptr, &bgFramebuffer);
  // Render the workspace with hidden windows (showing just the wallpaper/background)
  g_pHyprRenderer->renderWorkspace(monitor, activeWorkspace, std::chrono::steady_clock::now(), CBox{0, 0, (int)MONITOR_SIZE.x, (int)MONITOR_SIZE.y});
  g_pHyprRenderer->endRender();

  // Restore all windows to original hidden state
  for (size_t i = 0; i < hiddenWindows.size(); ++i) {
    hiddenWindows[i]->m_hidden = originalHiddenStates[i];
  }

  bgCaptured = true;
}

CHyprView::CHyprView(PHLMONITOR pMonitor_, PHLWORKSPACE startedOn_, bool swipe_, EWindowCollectionMode mode) : pMonitor(pMonitor_), startedOn(startedOn_), swipe(swipe_), m_collectionMode(mode) {

  // Capture the background BEFORE moving windows for the overview
  captureBackground();

  // Block rendering until we finish moving windows to active workspace
  // This ensures the overview layer is created AFTER workspace migration
  blockOverviewRendering = true;

  originalFocusedWindow = g_pCompositor->m_lastWindow;
  userExplicitlySelected = false;

  auto origWindow = originalFocusedWindow.lock();
  Debug::log(LOG, "[hyprview] CHyprView(): Saved original focused window: {}", (void *)origWindow.get());

  static auto *const *PMARGIN = (Hyprlang::INT *const *)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprview:margin")->getDataStaticPtr();

  MARGIN = **PMARGIN;

  static auto *const *PACTIVEBORDERCOL = (Hyprlang::INT *const *)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprview:active_border_color")->getDataStaticPtr();
  static auto *const *PINACTIVEBORDERCOL = (Hyprlang::INT *const *)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprview:inactive_border_color")->getDataStaticPtr();
  static auto *const *PBORDERWIDTH = (Hyprlang::INT *const *)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprview:border_width")->getDataStaticPtr();
  static auto *const *PBORDERRADIUS = (Hyprlang::INT *const *)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprview:border_radius")->getDataStaticPtr();
  static auto *const *PBGDIM = (Hyprlang::FLOAT *const *)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprview:bg_dim")->getDataStaticPtr();
  static auto *const *PWORKSPACEINDICATORENABLED = (Hyprlang::INT *const *)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprview:workspace_indicator_enabled")->getDataStaticPtr();
  static auto *const *PWORKSPACEINDICATORFONTSIZE = (Hyprlang::INT *const *)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprview:workspace_indicator_font_size")->getDataStaticPtr();
  static auto PWORKSPACEINDICATORPOSITION_VAL = HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprview:workspace_indicator_position");
  static auto *const *PWORKSPACEINDICATORBGOPACITY = (Hyprlang::FLOAT *const *)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprview:workspace_indicator_bg_opacity")->getDataStaticPtr();

  ACTIVE_BORDER_COLOR = **PACTIVEBORDERCOL;
  INACTIVE_BORDER_COLOR = **PINACTIVEBORDERCOL;
  BORDER_WIDTH = **PBORDERWIDTH;
  BORDER_RADIUS = **PBORDERRADIUS;
  BG_DIM = **PBGDIM;
  WORKSPACE_INDICATOR_ENABLED = **PWORKSPACEINDICATORENABLED != 0;
  WORKSPACE_INDICATOR_FONT_SIZE = **PWORKSPACEINDICATORFONTSIZE;
  WORKSPACE_INDICATOR_BG_OPACITY = **PWORKSPACEINDICATORBGOPACITY;
  WORKSPACE_INDICATOR_POSITION = "";

  try {
    if (PWORKSPACEINDICATORPOSITION_VAL) {
      if (auto strPtr = (Hyprlang::STRING const *)PWORKSPACEINDICATORPOSITION_VAL->getDataStaticPtr()) {
        if (*strPtr) {
          WORKSPACE_INDICATOR_POSITION = *strPtr;
        }
      }
    }
  } catch (...) {
    // Keep default on any exception
  }

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

  const size_t windowCount = windowsToRender.size();

  if (windowCount == 1) {
    SIDE_LENGTH = 1;
    GRID_ROWS = 1;
  } else if (windowCount == 2) {
    SIDE_LENGTH = 2;
    GRID_ROWS = 1;
  } else if (windowCount <= 4) {
    SIDE_LENGTH = 2;
    GRID_ROWS = 2;
  } else if (windowCount <= 6) {
    SIDE_LENGTH = 3;
    GRID_ROWS = 2;
  } else if (windowCount <= 9) {
    SIDE_LENGTH = 3;
    GRID_ROWS = 3;
  } else {
    double sqrtCount = std::sqrt((double)windowCount);
    SIDE_LENGTH = (int)std::ceil(sqrtCount);
    GRID_ROWS = (int)std::ceil((double)windowCount / (double)SIDE_LENGTH);
    if (SIDE_LENGTH > kMaxGridColumns) SIDE_LENGTH = kMaxGridColumns;
    GRID_ROWS = (windowCount + SIDE_LENGTH - 1) / SIDE_LENGTH;
  }

  Debug::log(LOG, "[hyprview] Using {}x{} grid for {} windows", SIDE_LENGTH, GRID_ROWS, windowCount);

  const size_t maxWindows = SIDE_LENGTH * GRID_ROWS;
  const size_t numWindows = std::min(windowsToRender.size(), maxWindows);
  images.resize(numWindows);

  g_pHyprRenderer->makeEGLCurrent();

  Vector2D fullMonitorSize = pMonitor->m_pixelSize;
  calculateAdaptiveRowHeights(numWindows, fullMonitorSize.y);
  Vector2D tileSize = {fullMonitorSize.x / SIDE_LENGTH, fullMonitorSize.y / GRID_ROWS};
  Vector2D tileRenderSize = tileSize - Vector2D{kMarginMultiplier * MARGIN, kMarginMultiplier * MARGIN};

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

    image.box = {(i % SIDE_LENGTH) * tileSize.x + MARGIN, (i / SIDE_LENGTH) * tileSize.y + MARGIN, tileRenderSize.x, tileRenderSize.y};

    const auto RENDERSIZE = (window->m_realSize->value() * pMonitor->m_scale).floor();
    image.fb.alloc(std::max(1.0, RENDERSIZE.x), std::max(1.0, RENDERSIZE.y), pMonitor->m_output->state->state().drmFormat);

    CRegion fakeDamage{0, 0, INT16_MAX, INT16_MAX};

    const auto REALPOS = window->m_realPosition->value();

    // Temporarily move window to monitor position for rendering
    window->m_realPosition->setValue(pMonitor->m_position);

    g_pHyprRenderer->beginRender(pMonitor.lock(), fakeDamage, RENDER_MODE_FULL_FAKE, nullptr, &image.fb);

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

  int gridX = currentid % SIDE_LENGTH;
  int gridY = currentid / SIDE_LENGTH;

  g_pAnimationManager->createAnimation(fullMonitorSize * fullMonitorSize / tileSize, size, g_pConfigManager->getAnimationPropertyConfig("windowsMove"), AVARDAMAGE_NONE);
  g_pAnimationManager->createAnimation((-(tileSize * Vector2D{(double)gridX, (double)gridY}) * pMonitor->m_scale) * (fullMonitorSize / tileSize), pos, g_pConfigManager->getAnimationPropertyConfig("windowsMove"), AVARDAMAGE_NONE);

  size->setUpdateCallback(damageMonitor);
  pos->setUpdateCallback(damageMonitor);

  if (!swipe) {
    *size = fullMonitorSize;
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
    Vector2D fullMonitorSize = pMonitor->m_pixelSize;

    // Only handle if mouse is on this monitor
    if (globalMousePos.x < monitorPos.x || globalMousePos.x >= monitorPos.x + fullMonitorSize.x ||
        globalMousePos.y < monitorPos.y || globalMousePos.y >= monitorPos.y + fullMonitorSize.y) {
      return; // Mouse is on a different monitor
    }

    lastMousePosLocal = globalMousePos - monitorPos;

    if (!images.empty()) {
      int row = 0;
      if (!rowYPositions.empty() && !rowHeights.empty()) {
        for (size_t r = 0; r < rowYPositions.size(); ++r) {
          double rowStart = rowYPositions[r];
          double rowEnd = rowStart + rowHeights[r];
          if (lastMousePosLocal.y >= rowStart && lastMousePosLocal.y < rowEnd) {
            row = r;
            break;
          }
          if (lastMousePosLocal.y >= rowEnd) {
            row = r + 1;
          }
        }
        if (row >= GRID_ROWS) row = GRID_ROWS - 1;
        if (row < 0) row = 0;
      } else {
        row = lastMousePosLocal.y / fullMonitorSize.y * GRID_ROWS;
      }
      int windowsInThisRow = (row < (int)windowsPerRow.size()) ? windowsPerRow[row] : SIDE_LENGTH;
      double tileWidth = fullMonitorSize.x / SIDE_LENGTH;
      double rowCenteringOffset = 0.0;
      if (windowsInThisRow < SIDE_LENGTH && windowsInThisRow > 0) {
        double gridWidth = SIDE_LENGTH * tileWidth;
        double usedWidth = windowsInThisRow * tileWidth;
        rowCenteringOffset = (gridWidth - usedWidth) / 2.0;
      }
      double adjustedMouseX = lastMousePosLocal.x - rowCenteringOffset;
      int col = (int)(adjustedMouseX / tileWidth);
      if (col < 0) col = 0;
      if (col >= windowsInThisRow) col = windowsInThisRow - 1;
      int tileIndex = col + row * SIDE_LENGTH;
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
    close();
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

  Vector2D fullMonitorSize = pMonitor->m_pixelSize;
  int row = 0;
  if (!rowYPositions.empty() && !rowHeights.empty()) {
    for (size_t r = 0; r < rowYPositions.size(); ++r) {
      double rowStart = rowYPositions[r];
      double rowEnd = rowStart + rowHeights[r];
      if (lastMousePosLocal.y >= rowStart && lastMousePosLocal.y < rowEnd) {
        row = r;
        break;
      }
      if (lastMousePosLocal.y >= rowEnd) {
        row = r + 1;
      }
    }
    if (row >= GRID_ROWS) row = GRID_ROWS - 1;
    if (row < 0) row = 0;
  } else {
    row = lastMousePosLocal.y / fullMonitorSize.y * GRID_ROWS;
  }
  int windowsInThisRow = (row < (int)windowsPerRow.size()) ? windowsPerRow[row] : SIDE_LENGTH;
  double tileWidth = fullMonitorSize.x / SIDE_LENGTH;
  double rowCenteringOffset = 0.0;
  if (windowsInThisRow < SIDE_LENGTH && windowsInThisRow > 0) {
    double gridWidth = SIDE_LENGTH * tileWidth;
    double usedWidth = windowsInThisRow * tileWidth;
    rowCenteringOffset = (gridWidth - usedWidth) / 2.0;
  }
  double adjustedMouseX = lastMousePosLocal.x - rowCenteringOffset;
  int col = (int)(adjustedMouseX / tileWidth);
  if (col < 0) col = 0;
  if (col >= windowsInThisRow) col = windowsInThisRow - 1;
  closeOnID = col + row * SIDE_LENGTH;

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

  double tileWidth = SIZE.x / SIDE_LENGTH;
  int col = openedID % SIDE_LENGTH;
  int row = openedID / SIDE_LENGTH;
  double rowHeight = (row < (int)rowHeights.size()) ? rowHeights[row] : (SIZE.y / GRID_ROWS);
  double rowYPos = (row < (int)rowYPositions.size()) ? rowYPositions[row] : (row * SIZE.y / GRID_ROWS);
  double scaleFactor = SIZE.y / pMonitor->m_pixelSize.y;
  rowHeight *= scaleFactor;
  rowYPos *= scaleFactor;
  Vector2D tileRenderSize = {tileWidth - kMarginMultiplier * MARGIN, rowHeight - kMarginMultiplier * MARGIN};
  CBox texbox = CBox{col * tileWidth + MARGIN, rowYPos + MARGIN, tileRenderSize.x, tileRenderSize.y}.translate(pMonitor->m_position);

  damage();

  blockDamageReporting = true;
  g_pHyprRenderer->damageBox(texbox);
  blockDamageReporting = false;
  g_pCompositor->scheduleFrameForMonitor(pMonitor.lock());
}

void CHyprView::close() {

  if (closing) {

    Debug::log(LOG, "[hyprview] close(): already closing, returning");
    return;
  }

  closing = true;

  // STEP 1: Restore ALL windows to their original workspaces FIRST
  // This ensures that when we focus a window, the workspace switch happens naturally

  Debug::log(LOG, "[hyprview] close(): Restoring all windows to original workspaces");
  for (const auto &image : images) {
    auto window = image.pWindow.lock();
    if (window && image.originalWorkspace && window->m_workspace != image.originalWorkspace) {
      Debug::log(LOG, "[hyprview] close(): Restoring window '{}' from workspace {} to {}",
                 window->m_title, window->m_workspace->m_id, image.originalWorkspace->m_id);
      window->moveToWorkspace(image.originalWorkspace);
    }
  }

  // STEP 2: Now focus the selected window (workspace will follow automatically)
  if (userExplicitlySelected && closeOnID >= 0 && closeOnID < (int)images.size()) {

    const auto &TILE = images[closeOnID];
    auto window = TILE.pWindow.lock();
    if (window && window->m_isMapped) {

      Debug::log(LOG, "[hyprview] close(): User selected window, focusing and bringing to top");
      g_pCompositor->focusWindow(window);
      g_pKeybindManager->alterZOrder("top");
    }
  } else {

    auto origWindow = originalFocusedWindow.lock();
    if (origWindow && origWindow->m_isMapped) {

      Debug::log(LOG, "[hyprview] close(): Restoring original window focus");
      g_pCompositor->focusWindow(origWindow);
    }
  }
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

  Vector2D SIZE = size->value();

  double tileWidth = SIZE.x / SIDE_LENGTH;
  bool isSingleWindow = (images.size() == 1);
  Vector2D tileRenderSize;
  if (isSingleWindow) {
    tileRenderSize = SIZE * kSingleWindowScale;
  }

  // Render the captured background instead of a solid color
  if (bgCaptured && bgFramebuffer.m_size.x > 0 && bgFramebuffer.m_size.y > 0) {
    Vector2D fullMonitorSize = pMonitor->m_pixelSize;  // Use actual monitor size, not animated size
    CBox monitorBox = {0, 0, fullMonitorSize.x, fullMonitorSize.y};
    CRegion damage{0, 0, INT16_MAX, INT16_MAX};
    g_pHyprOpenGL->renderTextureInternal(bgFramebuffer.getTexture(), monitorBox,
                                         {.damage = &damage, .a = 1.0, .round = 0});

    // Add a dim overlay when overview is active (even with no windows)
    // This makes it clear we're in overview mode vs just viewing desktop
    g_pHyprOpenGL->renderRect(monitorBox, CHyprColor(0.0, 0.0, 0.0, BG_DIM), {});
  }

  const auto PLASTWINDOW = g_pCompositor->m_lastWindow.lock();

  for (size_t i = 0; i < images.size(); ++i) {
    size_t col = i % SIDE_LENGTH;
    size_t row = i / SIDE_LENGTH;

    const Vector2D &textureSize = images[i].fb.m_size;

    if (textureSize.x < 1 || textureSize.y < 1)
      continue;

    // Center the grid on screen
    double cellX, cellY;
    double cellWidth, cellHeight;
    if (isSingleWindow) {
      cellX = (SIZE.x - tileRenderSize.x) / 2.0;
      cellY = (SIZE.y - tileRenderSize.y) / 2.0;
      cellWidth = tileRenderSize.x;
      cellHeight = tileRenderSize.y;
    } else {
      double rowHeight = (row < rowHeights.size()) ? rowHeights[row] : (SIZE.y / GRID_ROWS);
      double rowYPos = (row < rowYPositions.size()) ? rowYPositions[row] : (row * SIZE.y / GRID_ROWS);
      double scaleFactor = SIZE.y / pMonitor->m_pixelSize.y;
      rowHeight *= scaleFactor;
      rowYPos *= scaleFactor;
      cellWidth = tileWidth - kMarginMultiplier * MARGINSIZE;
      cellHeight = rowHeight - kMarginMultiplier * MARGINSIZE;
      size_t actualCols = std::min((size_t)SIDE_LENGTH, images.size());
      size_t actualRows = (images.size() + SIDE_LENGTH - 1) / SIDE_LENGTH;
      double gridWidth = actualCols * tileWidth;
      double totalGridHeight = 0.0;
      for (size_t r = 0; r < actualRows && r < rowHeights.size(); ++r) {
        totalGridHeight += rowHeights[r] * scaleFactor;
      }
      double gridOffsetX = (SIZE.x - gridWidth) / 2.0;
      double gridOffsetY = (SIZE.y - totalGridHeight) / 2.0;
      int windowsInThisRow = (row < windowsPerRow.size()) ? windowsPerRow[row] : SIDE_LENGTH;
      double rowCenteringOffset = 0.0;
      if (windowsInThisRow < SIDE_LENGTH && windowsInThisRow > 0) {
        double usedWidth = windowsInThisRow * tileWidth;
        rowCenteringOffset = (gridWidth - usedWidth) / 2.0;
      }
      cellX = gridOffsetX + rowCenteringOffset + col * tileWidth + MARGINSIZE;
      cellY = gridOffsetY + rowYPos + MARGINSIZE;
    }
    const double textureAspect = textureSize.x / textureSize.y;
    const double cellAspect = cellWidth / cellHeight;
    Vector2D newSize;
    if (textureAspect > cellAspect) {
      newSize.x = cellWidth;
      newSize.y = newSize.x / textureAspect;
    } else {
      newSize.y = cellHeight;
      newSize.x = newSize.y * textureAspect;
    }
    const double offsetX = (cellWidth - newSize.x) / 2.0;
    const double offsetY = (cellHeight - newSize.y) / 2.0;

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

    // Render workspace number indicator (if enabled and window exists)
    if (WORKSPACE_INDICATOR_ENABLED) {
      auto window = images[i].pWindow.lock();
      if (window && images[i].originalWorkspace) {
        renderWorkspaceIndicator(i, borderBox, damage, ISACTIVE);
      }
    }
  }
}

void CHyprView::renderWorkspaceIndicator(size_t i, const CBox &borderBox, const CRegion &damage, const bool ISACTIVE) {
  int workspaceID = images[i].originalWorkspace->m_id;
  std::string workspaceText = "wsid:" + std::to_string(workspaceID);
  // Use border color based on whether window is active
  const auto &INDICATOR_COLOR = ISACTIVE ? ACTIVE_BORDER_COLOR : INACTIVE_BORDER_COLOR;
  auto textTexture = g_pHyprOpenGL->renderText(workspaceText, INDICATOR_COLOR, WORKSPACE_INDICATOR_FONT_SIZE, false, "sans-serif");

  if (textTexture) {
    const double textPadding = kIndicatorTextPadding;
    double textX, textY;

    // Calculate position based on configured position
    if (WORKSPACE_INDICATOR_POSITION == "top-left") {
      textX = borderBox.x + textPadding;
      textY = borderBox.y + textPadding;
    } else if (WORKSPACE_INDICATOR_POSITION == "bottom-left") {
      textX = borderBox.x + textPadding;
      textY = borderBox.y + borderBox.height - (textTexture->m_size.y * kIndicatorTextScale) - textPadding;
    } else if (WORKSPACE_INDICATOR_POSITION == "bottom-right") {
      textX = borderBox.x + borderBox.width - (textTexture->m_size.x * kIndicatorTextScale) - textPadding;
      textY = borderBox.y + borderBox.height - (textTexture->m_size.y * kIndicatorTextScale) - textPadding;
    } else {
      textX = borderBox.x + borderBox.width - (textTexture->m_size.x * kIndicatorTextScale) - textPadding;
      textY = borderBox.y + textPadding;
    }

    // Scale text size appropriately
    double textWidth = textTexture->m_size.x * kIndicatorTextScale;
    double textHeight = textTexture->m_size.y * kIndicatorTextScale;

    CBox textBox = {textX, textY, textWidth, textHeight};

    // Render background for text with configured opacity
    CBox textBgBox = {textX - kIndicatorBgPadding, textY - kIndicatorBgPadding, 
                      textWidth + kMarginMultiplier * kIndicatorBgPadding, 
                      textHeight + kMarginMultiplier * kIndicatorBgPadding};
    CHyprOpenGLImpl::SRectRenderData bgData;
    bgData.round = kIndicatorBgRound;
    g_pHyprOpenGL->renderRect(textBgBox, CHyprColor(0.0, 0.0, 0.0, WORKSPACE_INDICATOR_BG_OPACITY), bgData);

    // Render the text on top
    g_pHyprOpenGL->renderTextureInternal(textTexture, textBox, {.damage = &damage, .a = 1.0, .round = 0});
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

  Vector2D fullMonitorSize = pMonitor->m_pixelSize;
  Vector2D tileSize = {fullMonitorSize.x / SIDE_LENGTH, fullMonitorSize.y / GRID_ROWS};

  int gridX = WORKSPACE_FOCUS_ID % SIDE_LENGTH;
  int gridY = WORKSPACE_FOCUS_ID / SIDE_LENGTH;

  const auto SIZEMAX = fullMonitorSize * fullMonitorSize / tileSize;
  const auto POSMAX = (-(tileSize * Vector2D{(double)gridX, (double)gridY}) * pMonitor->m_scale) * (fullMonitorSize / tileSize);

  const auto SIZEMIN = fullMonitorSize;
  const auto POSMIN = Vector2D{0, 0};

  size->setValueAndWarp(lerp(SIZEMIN, SIZEMAX, PERC));
  pos->setValueAndWarp(lerp(POSMIN, POSMAX, PERC));
}

void CHyprView::onSwipeEnd() {
  Vector2D fullMonitorSize = pMonitor->m_pixelSize;
  const auto SIZEMIN = fullMonitorSize;
  Vector2D tileSize = {fullMonitorSize.x / SIDE_LENGTH, fullMonitorSize.y / GRID_ROWS};
  const auto SIZEMAX = fullMonitorSize * fullMonitorSize / tileSize;
  const auto PERC = (size->value() - SIZEMIN).x / (SIZEMAX - SIZEMIN).x;
  if (PERC > 0.5) {
    close();
    return;
  }
  *size = fullMonitorSize;
  *pos = {0, 0};

  swipeWasCommenced = true;
  m_isSwiping = false;
}
