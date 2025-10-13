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

### Hyprland Hooks Used
- `renderWorkspace` - Intercepts workspace rendering when overview active
- `addDamageA/B` - Damage reporting hooks for efficient updates
- `swipeBegin/Update/End` - Gesture blocking when overview active

### Key Functions
- `CHyprView::captureBackground()` - Captures desktop background before overview
- `CHyprView::fullRender()` - Main rendering function
- `CHyprView::close()` - Cleanup and restore windows to original workspaces
- `CHyprViewPassElement::draw()` - Render pass element drawing

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
- `plugin:hyprview:gesture_distance`

### Framebuffer Management
- Individual framebuffers per window stored in `SWindowImage::fb`
- Background framebuffer in `CHyprView::bgFramebuffer`
- All cleaned up in `CHyprView` destructor

### Animation System
- Uses Hyprland's `CAnimatedVariable` for size/position transitions
- Opening animation: scales from point to full screen
- Animation variables: `CHyprView::size`, `CHyprView::pos`

### Gesture Handling
- 3-finger swipe gestures handled by `CViewGesture`
- Swipe detection uses distance threshold from config
- Gestures blocked when overview is active to prevent conflicts