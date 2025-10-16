#include "hyprview.hpp"
#include "PlacementAlgorithms.hpp"
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

void gridPlacement(CHyprView* hyprview, std::vector<PHLWINDOW> &windowsToRender) {
    const size_t windowCount = windowsToRender.size();

    // Grid size calculation:
    // 1 window: 1x1 (80% of screen size, centered)
    // 2 windows: 2x1
    // 3-4 windows: 2x2
    // 5-9 windows: 3x3
    // 10+ windows: 4xX
    if (windowCount == 1) {
        hyprview->SIDE_LENGTH = 1;
        hyprview->GRID_ROWS = 1;
    } else if (windowCount <= 2) {
        hyprview->SIDE_LENGTH = 2;
        hyprview->GRID_ROWS = 1;
    } else if (windowCount <= 4) {
        hyprview->SIDE_LENGTH = 2;
        hyprview->GRID_ROWS = 2;
    } else if (windowCount <= 9) {
        hyprview->SIDE_LENGTH = 3;
        hyprview->GRID_ROWS = 3;
    } else {
        hyprview->SIDE_LENGTH = 4;
        hyprview->GRID_ROWS = (windowCount + hyprview->SIDE_LENGTH - 1) / hyprview->SIDE_LENGTH;
    }

    Debug::log(LOG, "[hyprview] Using {}x{} grid for {} windows", hyprview->SIDE_LENGTH, hyprview->GRID_ROWS, windowCount);

    const size_t maxWindows = hyprview->SIDE_LENGTH * hyprview->GRID_ROWS;
    const size_t numWindows = std::min(windowsToRender.size(), maxWindows);
    hyprview->images.resize(numWindows);

    g_pHyprRenderer->makeEGLCurrent();

    // Use m_pixelSize for full monitor dimensions at native resolution
    Vector2D fullMonitorSize = hyprview->pMonitor->m_pixelSize;
    Vector2D tileSize = {fullMonitorSize.x / hyprview->SIDE_LENGTH, fullMonitorSize.y / hyprview->GRID_ROWS};
    Vector2D tileRenderSize = tileSize - Vector2D{2.0 * hyprview->MARGIN, 2.0 * hyprview->MARGIN};

    Debug::log(LOG,
               "[hyprview] Monitor size: {}x{}, Grid: {}x{}, Tile size: {}x{}, "
               "Render size: {}x{}",
               hyprview->pMonitor->m_size.x, hyprview->pMonitor->m_size.y, hyprview->SIDE_LENGTH, hyprview->GRID_ROWS, tileSize.x, tileSize.y, tileRenderSize.x, tileRenderSize.y);

    g_pHyprRenderer->m_bBlockSurfaceFeedback = true;

    // Save original workspaces BEFORE moving
    std::unordered_map<PHLWINDOW, PHLWORKSPACE> originalWorkspaces;
    for (auto &window : windowsToRender) {
        originalWorkspaces[window] = window->m_workspace;
    }

    // Move windows to active workspace so they have valid surfaces for rendering
    for (auto &window : windowsToRender) {
        if (window->m_workspace != hyprview->pMonitor->m_activeWorkspace) {
            Debug::log(LOG, "[hyprview] Moving window '{}' from workspace {} to {}",
                       window->m_title, window->m_workspace->m_id, hyprview->pMonitor->m_activeWorkspace->m_id);
            window->moveToWorkspace(hyprview->pMonitor->m_activeWorkspace);
        }
    }

    // Render all windows to framebuffers
    for (size_t i = 0; i < numWindows; ++i) {
        CHyprView::SWindowImage &image = hyprview->images[i];
        auto &window = windowsToRender[i];

        image.pWindow = window;
        image.originalPos = window->m_realPosition->value();
        image.originalSize = window->m_realSize->value();
        image.originalWorkspace = originalWorkspaces[window]; // Save the original workspace

        image.box = {(i % hyprview->SIDE_LENGTH) * tileSize.x + hyprview->MARGIN, (i / hyprview->SIDE_LENGTH) * tileSize.y + hyprview->MARGIN, tileRenderSize.x, tileRenderSize.y};

        const auto RENDERSIZE = (window->m_realSize->value() * hyprview->pMonitor->m_scale).floor();
        image.fb.alloc(std::max(1.0, RENDERSIZE.x), std::max(1.0, RENDERSIZE.y), hyprview->pMonitor->m_output->state->state().drmFormat);

        CRegion fakeDamage{0, 0, INT16_MAX, INT16_MAX};

        const auto REALPOS = window->m_realPosition->value();

        // Temporarily move window to monitor position for rendering
        window->m_realPosition->setValue(hyprview->pMonitor->m_position);

        g_pHyprRenderer->beginRender(hyprview->pMonitor.lock(), fakeDamage, RENDER_MODE_FULL_FAKE, nullptr, &image.fb);

        // Now all windows should have valid surfaces since they're on the active workspace
        if (window && window->m_isMapped) {
            g_pHyprRenderer->renderWindow(window, hyprview->pMonitor.lock(), Time::steadyNow(), false, RENDER_PASS_MAIN, false, false);
        }

        g_pHyprOpenGL->m_renderData.blockScreenShader = true;
        g_pHyprRenderer->endRender();

        // Restore original position
        window->m_realPosition->setValue(REALPOS);
    }

    // Initialize currentid after the grid is set up
    int currentid = -1;
    if (!hyprview->images.empty()) {
        currentid = 0;
    }

    int gridX = currentid % hyprview->SIDE_LENGTH;
    int gridY = currentid / hyprview->SIDE_LENGTH;

    // Use m_pixelSize for full monitor dimensions at native resolution  
    Vector2D fullMonitorSizeFunc = hyprview->pMonitor->m_pixelSize;
    Vector2D tileSizeFunc = {fullMonitorSizeFunc.x / hyprview->SIDE_LENGTH, fullMonitorSizeFunc.y / hyprview->GRID_ROWS};

    g_pAnimationManager->createAnimation(fullMonitorSizeFunc * fullMonitorSizeFunc / tileSizeFunc, hyprview->size, g_pConfigManager->getAnimationPropertyConfig("windowsMove"), AVARDAMAGE_NONE);
    g_pAnimationManager->createAnimation((-(tileSizeFunc * Vector2D{(double)gridX, (double)gridY}) * hyprview->pMonitor->m_scale) * (fullMonitorSizeFunc / tileSizeFunc), hyprview->pos, g_pConfigManager->getAnimationPropertyConfig("windowsMove"), AVARDAMAGE_NONE);

    hyprview->size->setUpdateCallback(damageMonitor);
    hyprview->pos->setUpdateCallback(damageMonitor);

    if (!hyprview->swipe) {
        *hyprview->size = fullMonitorSizeFunc;
        *hyprview->pos = {0, 0};
    }

    hyprview->openedID = currentid;
}