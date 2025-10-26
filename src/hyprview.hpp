#pragma once
#define WLR_USE_UNSTABLE

#include "globals.hpp"
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/helpers/AnimatedVariable.hpp>
#include <hyprland/src/managers/HookSystemManager.hpp>
#include <hyprland/src/render/Framebuffer.hpp>
#include <unordered_map>
#include <vector>

// Forward declaration for hooks
class CFunctionHook;
extern CFunctionHook *g_pRenderWorkspaceHook;

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

// Function declaration for damageMonitor used by placement algorithms
void damageMonitor(WP<Hyprutils::Animation::CBaseAnimatedVariable> thisptr);

class CHyprView {
public:
  CHyprView(PHLMONITOR pMonitor_, PHLWORKSPACE startedOn_, bool swipe = false, EWindowCollectionMode mode = EWindowCollectionMode::CURRENT_ONLY, const std::string& placement = "grid", bool explicitOn = false);
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
  void selectHoveredWindow();

  // Accurate mouse-to-tile calculation
  int getWindowIndexFromMousePos(const Vector2D& mousePos);
  bool isMouseOverValidTile(const Vector2D& mousePos);
  void updateHoverState(int newIndex);

  bool blockOverviewRendering = false;
  bool blockDamageReporting = false;

  PHLMONITORREF pMonitor;
  bool m_isSwiping = false;
  bool closing = false;
  bool swipe = false;
  bool explicitlyOn = false;  // True if turned on with :on command (sticky mode)

private:
  void redrawID(int id, bool forcelowres = false);
  void redrawAll(bool forcelowres = false);
  void onWorkspaceChange();
  void fullRender();
  void renderWorkspaceIndicator(size_t i, const CBox &borderBox, const CRegion &damage, const bool ISACTIVE);
  void captureBackground();
  void setupWindowImages(std::vector<PHLWINDOW> &windowsToRender);

  CFramebuffer bgFramebuffer; // Store the captured background
  bool bgCaptured = false;    // Flag to track if background is captured

  int MARGIN = 15;     // Margin around each grid tile

  CHyprColor ACTIVE_BORDER_COLOR;
  CHyprColor INACTIVE_BORDER_COLOR;
  int BORDER_WIDTH;
  int BORDER_RADIUS;
  float BG_DIM;

  // Workspace indicator configuration
  bool WORKSPACE_INDICATOR_ENABLED;
  int WORKSPACE_INDICATOR_FONT_SIZE;
  std::string WORKSPACE_INDICATOR_POSITION;
  float WORKSPACE_INDICATOR_BG_OPACITY;

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
  int currentHoveredIndex = -1; // Current tile index under mouse cursor
  int visualHoveredIndex = -1;  // Visual hover state (for immediate feedback)

  std::vector<SWindowImage> images;

  PHLWINDOWREF originalFocusedWindow;  // Window that had focus before overview
  bool userExplicitlySelected = false; // Whether user clicked/selected a window
  PHLWINDOWREF lastHoveredWindow;      // Track last hovered window for focus

  PHLWORKSPACE startedOn;
  EWindowCollectionMode m_collectionMode;
  std::string m_placement;

  PHLANIMVAR<Vector2D> size;
  PHLANIMVAR<Vector2D> pos;
  PHLANIMVAR<float> alpha;  // Fade animation for overview

  SP<HOOK_CALLBACK_FN> mouseMoveHook;
  SP<HOOK_CALLBACK_FN> mouseButtonHook;
  SP<HOOK_CALLBACK_FN> mouseAxisHook;
  SP<HOOK_CALLBACK_FN> touchMoveHook;
  SP<HOOK_CALLBACK_FN> touchDownHook;

  bool swipeWasCommenced = false;

  friend class CHyprViewPassElement;
  friend CHyprView * ::findInstanceForAnimation(WP<Hyprutils::Animation::CBaseAnimatedVariable>);
};

// Map of monitor to CHyprView instance - one overview per monitor
inline std::unordered_map<PHLMONITOR, std::unique_ptr<CHyprView>> g_pHyprViewInstances;
