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
CHyprView *findInstanceForAnimation(
    WP<Hyprutils::Animation::CBaseAnimatedVariable> thisptr);

class CHyprView {
public:
  CHyprView(PHLMONITOR pMonitor_, PHLWORKSPACE startedOn_, bool swipe = false,
            EWindowCollectionMode mode = EWindowCollectionMode::CURRENT_ONLY);
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

  bool blockOverviewRendering = false;
  bool blockDamageReporting = false;

  PHLMONITORREF pMonitor;
  bool m_isSwiping = false;
  bool closing = false;
  bool swipe = false;

private:
  void redrawID(int id, bool forcelowres = false);
  void redrawAll(bool forcelowres = false);
  void onWorkspaceChange();
  void fullRender();
  void renderWorkspaceIndicator(size_t i, const CBox &borderBox,
                                const CRegion &damage, const bool ISACTIVE);
  void captureBackground();
  void calculateAdaptiveRowHeights(size_t windowCount, double screenHeight);

  // Layout configuration constants
  // Maximum columns for readability
  static constexpr int kMaxGridColumns = 5;
  // Single window takes 80% of screen
  static constexpr double kSingleWindowScale = 0.8;
  // Margin multiplier for tile spacing
  static constexpr double kMarginMultiplier = 2.0;

  // Workspace indicator constants
  // Padding around indicator text
  static constexpr double kIndicatorTextPadding = 15.0;
  // Scale factor for indicator text size
  static constexpr double kIndicatorTextScale = 0.8;
  // Padding for indicator background
  static constexpr double kIndicatorBgPadding = 8.0;
  // Corner radius for indicator background
  static constexpr int kIndicatorBgRound = 8;

  CFramebuffer bgFramebuffer; // Store the captured background
  bool bgCaptured = false;    // Flag to track if background is captured

  int SIDE_LENGTH = 3; // Grid columns
  int GRID_ROWS = 3;   // Grid rows
  int MARGIN = 15;     // Margin around each grid tile

  // Adaptive row heights for better space utilization
  // Height of each row
  // Y position of each row start
  // Number of windows in each row for centering
  std::vector<double> rowHeights;
  std::vector<double> rowYPositions;
  std::vector<int> windowsPerRow;

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

  bool swipeWasCommenced = false;

  friend class CHyprViewPassElement;
  friend CHyprView * ::findInstanceForAnimation(
      WP<Hyprutils::Animation::CBaseAnimatedVariable>);
};

// Map of monitor to CHyprView instance - one overview per monitor
inline std::unordered_map<PHLMONITOR, std::unique_ptr<CHyprView>>
    g_pHyprViewInstances;
