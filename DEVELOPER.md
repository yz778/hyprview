# Hyprview Developer Guide

## Core Architecture

### Main Classes
- `CHyprView` - Main overview class, one per monitor
- `CHyprViewPassElement` - Render pass element implementing Hyprland's `IPassElement`
- `CViewGesture` - Trackpad gesture handler implementing `ITrackpadGesture`
- `g_pHyprViewInstances` - Global map of monitor to overview instance

```cpp
inline std::unordered_map<PHLMONITOR, std::unique_ptr<CHyprView>> g_pHyprViewInstances;
```

## Key Implementation Details

### Window Collection Modes
```cpp
enum class EWindowCollectionMode {
  CURRENT_ONLY,      // Default: current workspace only
  ALL_WORKSPACES,    // All workspaces on monitor (excl. special)
  WITH_SPECIAL,      // Current + special workspace
  ALL_WITH_SPECIAL   // All workspaces + special
};
```

### File Structure
- `main.cpp` - Plugin entry point, hooks, dispatchers, configuration
- `hyprview.cpp` - Core overview logic (`CHyprView` class)
- `HyprViewPassElement.*` - Render pass implementation
- `ViewGesture.*` - Gesture handling implementation
- `PlacementAlgorithms.hpp` - Header for all placement algorithms
- `GridPlacement.cpp` - Grid-based window placement algorithm
- `SpiralPlacement.cpp` - Spiral window placement algorithm
- `FlowPlacement.cpp` - Flow-based window placement algorithm
- `AdaptivePlacement.cpp` - Adaptive window placement algorithm
- `WidePlacement.cpp` - Wide uniform grid placement algorithm
- `ScalePlacement.cpp` - Wayfire scale algorithm implementation

### Hyprland Hooks Used
- `renderWorkspace` - Intercepts workspace rendering when overview active
- `addDamageA/B` - Damage reporting hooks for efficient updates
- `swipeBegin/Update/End` - Gesture blocking when overview active
- `preRender` - Cleanup and rendering updates
- `mouseMove/mouseButton/mouseAxis` - Mouse interaction handling
- `touchMove/touchDown` - Touch interaction handling

### Key Functions
- `CHyprView::captureBackground()` - Captures desktop background before overview
- `CHyprView::fullRender()` - Main rendering function
- `CHyprView::close()` - Cleanup and restore windows to original workspaces
- `CHyprViewPassElement::draw()` - Render pass element drawing
- `CHyprView::setupWindowImages()` - Moves windows to active workspace and renders to framebuffers
- `CHyprView::getWindowIndexFromMousePos()` - Accurate mouse-to-tile calculation
- `CHyprView::updateHoverState()` - Handles hover state changes
- `CViewGesture::begin/update/end()` - Swipe gesture handling

### Global Configuration Values
Located in `main.cpp`, registered via `HyprlandAPI::addConfigValue`:
- `plugin:hyprview:active_border_color`
- `plugin:hyprview:inactive_border_color`
- `plugin:hyprview:border_width`
- `plugin:hyprview:border_radius`
- `plugin:hyprview:bg_dim`
- `plugin:hyprview:margin`
- `plugin:hyprview:workspace_indicator_enabled`
- `plugin:hyprview:workspace_indicator_font_size`
- `plugin:hyprview:workspace_indicator_position`
- `plugin:hyprview:workspace_indicator_bg_opacity`
- `plugin:hyprview:window_name_enabled`
- `plugin:hyprview:window_name_font_size`
- `plugin:hyprview:window_name_bg_opacity`
- `plugin:hyprview:window_text_color`
- `plugin:hyprview:gesture_distance`

### Framebuffer Management
- Individual framebuffers per window stored in `SWindowImage::fb`
- Background framebuffer in `CHyprView::bgFramebuffer`
- All cleaned up in `CHyprView` destructor

### Animation System
- Uses Hyprland's `CAnimatedVariable` for position and alpha transitions
- Position animation: translates the position of overview elements (`CHyprView::pos`)
- Alpha animation: fade in/out effect (`CHyprView::alpha`) 
- Size variable is created but not currently used in rendering (`CHyprView::size`)
- Animations are configured using Hyprland's animation properties ("fadeIn", "windowsMove")
- Swipe gestures update the alpha value directly for smooth interaction

### Placement Algorithms
The plugin supports multiple window placement algorithms:
- `grid` - Traditional grid-based layout
- `spiral` - Spiral arrangement of windows
- `flow` - Proportional, row-based flow layout
- `adaptive` - Individual scaling with hierarchy
- `wide` - Wide, uniform grid layout
- `scale` - Wayfire scale algorithm implementation

Each algorithm is implemented as a pure mathematical function in dedicated files that calculate window positions without Hyprland dependencies.

### Dispatcher Commands
The plugin provides flexible dispatcher commands with various options:
- `hyprview:toggle` - Toggle overview on/off
- `hyprview:toggle all` - Toggle overview showing all workspaces
- `hyprview:toggle special` - Toggle overview including special workspace
- `hyprview:toggle all special` - Toggle overview with all workspaces and special
- `hyprview:toggle placement:grid` - Toggle with specific placement algorithm
- `hyprview:on` - Turn on overview (sticky mode)
- `hyprview:off` - Turn off overview
- `hyprview:select` - Select the currently hovered window
- `hyprview:toggle monitor:MonitorName` - Toggle overview on specific monitor

### Gesture Handling
- 3-finger swipe gestures handled by `CViewGesture`
- Swipe detection uses distance threshold from config
- Gestures blocked when overview is active to prevent conflicts
- Swipe gestures support opening and closing the overview
- Configurable via `hyprview-gesture` keyword

### Window Name Features
- Configurable window name display with `plugin:hyprview:window_name_enabled`
- Window names show class and title with workspace ID if indicators are enabled
- Configurable font size, background opacity, and text color
- Smart truncation with ellipsis for long window names