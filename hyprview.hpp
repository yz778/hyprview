#pragma once
#define WLR_USE_UNSTABLE

#include "globals.hpp"
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/helpers/AnimatedVariable.hpp>
#include <hyprland/src/managers/HookSystemManager.hpp>
#include <hyprland/src/render/Framebuffer.hpp>
#include <unordered_map>
#include <vector>

// saves on resources, but is a bit broken rn with blur.
// hyprland's fault, but cba to fix.
constexpr bool ENABLE_LOWRES = false;

// Window collection modes
enum class EWindowCollectionMode {
  CURRENT_ONLY,    // Default: only current workspace windows
  ALL_WORKSPACES,  // All workspaces on monitor (excluding special)
  WITH_SPECIAL,    // Current workspace + special workspace
  ALL_WITH_SPECIAL // All workspaces + special workspace
};

class CMonitor;
class CHyprView;

// Forward declare friend functions
CHyprView *findInstanceForAnimation(WP<Hyprutils::Animation::CBaseAnimatedVariable> thisptr);
void removeOverview(WP<Hyprutils::Animation::CBaseAnimatedVariable> thisptr);

class CHyprView {
public:
  CHyprView(PHLMONITOR pMonitor_, PHLWORKSPACE startedOn_, bool swipe = false, EWindowCollectionMode mode = EWindowCollectionMode::CURRENT_ONLY);
  ~CHyprView();

  void render();
  void damage();
  void onDamageReported();
  void onPreRender();

  void setClosing(bool closing);

  void resetSwipe();
  void onSwipeUpdate(double delta);
  void onSwipeEnd();

  // close without a selection
  void close();
  void closeImmediate();
  void closeWithoutFocus();
  void selectHoveredWindow();

  bool blockOverviewRendering = false;
  bool blockDamageReporting = false;

  PHLMONITORREF pMonitor;
  bool m_isSwiping = false;
  bool closing = false;

private:
  void redrawID(int id, bool forcelowres = false);
  void redrawAll(bool forcelowres = false);
  void onWorkspaceChange();
  void fullRender();

  int SIDE_LENGTH = 3;                                    // Grid columns
  int GRID_ROWS = 3;                                      // Grid rows
  int MARGIN = 15;                                        // Space between grid slots
  int PADDING = 10;                                       // Space inside grid slots before window content
  CHyprColor BG_COLOR = CHyprColor{0.1, 0.1, 0.1, 1.0};   // Background color
  CHyprColor GRID_COLOR = CHyprColor{0.0, 0.0, 0.0, 1.0}; // Grid slot background color

  CHyprColor ACTIVE_BORDER_COLOR;
  CHyprColor INACTIVE_BORDER_COLOR;
  int BORDER_WIDTH;
  int BORDER_RADIUS;

  bool damageDirty = false;

  struct SWindowImage {
    CFramebuffer fb;
    PHLWINDOWREF pWindow;
    CBox box;
    Vector2D originalPos;
    Vector2D originalSize;
    PHLWORKSPACE originalWorkspace; // Store original workspace for restoration
  };

  Vector2D lastMousePosLocal = Vector2D{};

  int openedID = -1;
  int closeOnID = -1;

  std::vector<SWindowImage> images;

  PHLWINDOWREF originalFocusedWindow;  // Window that had focus before overview
  bool userExplicitlySelected = false; // Whether user clicked/selected a window
  PHLWINDOWREF lastHoveredWindow;      // Track last hovered window for focus

  PHLWORKSPACE startedOn;
  EWindowCollectionMode m_collectionMode;

  PHLANIMVAR<Vector2D> size;
  PHLANIMVAR<Vector2D> pos;

  SP<HOOK_CALLBACK_FN> mouseMoveHook;
  SP<HOOK_CALLBACK_FN> mouseButtonHook;
  SP<HOOK_CALLBACK_FN> mouseAxisHook;
  SP<HOOK_CALLBACK_FN> touchMoveHook;
  SP<HOOK_CALLBACK_FN> touchDownHook;

  bool swipe = false;
  bool swipeWasCommenced = false;

  friend class CHyprViewPassElement;
  friend CHyprView * ::findInstanceForAnimation(WP<Hyprutils::Animation::CBaseAnimatedVariable>);
  friend void ::removeOverview(WP<Hyprutils::Animation::CBaseAnimatedVariable>);
};

// Map of monitor to CHyprView instance - one overview per monitor
inline std::unordered_map<PHLMONITOR, std::unique_ptr<CHyprView>> g_pHypreEyeInstances;
