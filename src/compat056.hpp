#pragma once

// Hyprland 0.56: compositor collections, framebuffer, and animations moved.

#include "compat054.hpp"

#include <hyprland/src/output/Monitor.hpp>
#include <hyprland/src/state/MonitorState.hpp>
#include <hyprland/src/desktop/state/WindowState.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/config/shared/animation/AnimationTree.hpp>
#include <hyprland/src/render/Framebuffer.hpp>
#include <hyprland/src/render/gl/GLFramebuffer.hpp>

#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/pass/PassElement.hpp>
#include <hyprland/src/animation/AnimationManager.hpp>
#include <hyprland/src/pointer/PointerManager.hpp>
#include <hyprland/src/pointer/cursor/CursorManager.hpp>
#include <hyprland/src/managers/fullscreen/FullscreenController.hpp>

// 0.51 had CMonitor at global scope; 0.56 namespaces it.
using CMonitor = Monitor::CMonitor;

// 0.56 moved the render globals, enums and texture interface under Render::.
using Render::GL::CHyprOpenGLImpl;
using Render::GL::g_pHyprOpenGL;
using Render::RENDER_MODE_FULL_FAKE;
using Render::RENDER_MODE_NORMAL;
using Render::RENDER_PASS_MAIN;
using CTexture = Render::ITexture;

// The manager singletons lost their g_p globals in favour of namespaced
// accessors. Keep the old spellings pointing at the new homes.
#define g_pAnimationManager Animation::mgr()
#define g_pPointerManager   Pointer::mgr()
#define g_pCursorManager    Pointer::Cursor::mgr()

#include <hyprland/src/config/shared/actions/ConfigActions.hpp>

// 0.56 made renderWindow()/renderWorkspace() protected on IHyprRenderer.
// A using-declaration in a derived type re-exports them publicly, which lets us
// take a pointer-to-member and call it on the real renderer. The member still
// belongs to IHyprRenderer, so this is a plain access-check workaround rather
// than a cast -- no layout or ABI assumptions.
namespace hyprview_compat {
    struct CRendererAccess : Render::IHyprRenderer {
        using Render::IHyprRenderer::renderWindow;
        using Render::IHyprRenderer::renderWorkspace;
    };

    inline void renderWindow(PHLWINDOW window, PHLMONITOR monitor, const Time::steady_tp& now, bool decorate, Render::eRenderPassMode mode, bool ignorePosition = false,
                             bool standalone = false) {
        (g_pHyprRenderer.get()->*(&CRendererAccess::renderWindow))(window, monitor, now, decorate, mode, ignorePosition, standalone);
    }

    inline void renderWorkspace(PHLMONITOR monitor, PHLWORKSPACE workspace, const Time::steady_tp& now, const CBox& geometry) {
        (g_pHyprRenderer.get()->*(&CRendererAccess::renderWorkspace))(monitor, workspace, now, geometry);
    }
}
