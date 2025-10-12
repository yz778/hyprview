# Hyprview Developer Guide

## Architecture

The plugin follows a modular architecture with the following main components:

### Core Classes

1. **`CHyprView`**: The main class representing an overview instance for a single monitor
2. **`CHyprViewPassElement`**: Render pass element for drawing the overview
3. **`CViewGesture`**: Handles trackpad gesture interactions
4. **Global state**: Managed through `g_pHyprViewInstances` map

### Global State Management

```cpp
inline std::unordered_map<PHLMONITOR, std::unique_ptr<CHyprView>> g_pHyprViewInstances;
```

This map associates each monitor with its corresponding overview instance, allowing for per-monitor overview functionality.

## Key Components

### CHyprView Class

The `CHyprView` class is responsible for:

1. **Window Collection and Management**:

   - Collects windows based on the specified `EWindowCollectionMode`
   - Temporarily moves windows to the active workspace for rendering
   - Maintains original window positions and workspaces
2. **Grid Layout**:

   - Dynamically calculates grid size (2x1, 2x2, or 3xN) based on window count
   - Calculates tile positions and sizes for each window
3. **Rendering Pipeline**:

   - Creates individual framebuffers for each window
   - Renders each window to its dedicated framebuffer
   - Provides animation support for opening/closing transitions
4. **Input Handling**:

   - Mouse movement for window selection and focus
   - Mouse/touch events for window selection
   - Swipe gesture support for touchpad interactions

### Window Collection Modes

```cpp
enum class EWindowCollectionMode {
  CURRENT_ONLY,     // Default: only current workspace windows
  ALL_WORKSPACES,   // All workspaces on monitor (excluding special)
  WITH_SPECIAL,     // Current workspace + special workspace
  ALL_WITH_SPECIAL  // All workspaces + special workspace
};
```

### CHyprViewPassElement Class

This class implements the Hyprland renderer's `IPassElement` interface:

1. **Rendering**: Draws the complete overview grid using pre-rendered window framebuffers
2. **Damage Tracking**: Manages rendering damage for efficient updates
3. **Layer Management**: Ensures the overview layer appears above other windows

### CViewGesture Class

Implements Hyprland's `ITrackpadGesture` interface to support:

1. **Swipe Detection**: Responds to 3-finger swipe gestures
2. **Smooth Animations**: Provides animated transitions during swipe
3. **Threshold Detection**: Requires minimum swipe distance to trigger actions

## Hook System Integration

The plugin modifies Hyprland's rendering behavior through strategic hooks:

### `renderWorkspace` Hook

- Intercepts normal workspace rendering when overview is active
- Replaces normal rendering with overview rendering pass
- Prevents background windows from showing underneath the overview

### Damage Reporting Hooks

- `addDamageA` and `addDamageB` hooks intercept damage reporting
- Allow the overview to update only necessary regions
- Ensure smooth animation performance

## Rendering Process

### Window Preparation

1. **Window Collection**: Selects windows based on the current mode
2. **Workspace Migration**: Temporarily moves all selected windows to the active workspace
3. **Framebuffer Creation**: Creates individual framebuffers for each window
4. **Pre-rendering**: Renders each window to its dedicated framebuffer

### Grid Layout Calculation

1. **Dynamic Sizing**: Calculates grid dimensions based on window count
2. **Position Calculation**: Determines positions and sizes for each window tile
3. **Aspect Ratio Adjustment**: Maintains window aspect ratios within tiles

### Display Rendering

1. **Background**: Renders a full-screen background to mask underlying windows
2. **Border Drawing**: Draws borders around each window tile
3. **Window Content**: Renders pre-captured window framebuffers
4. **Animation**: Applies size and position animations

## Input Handling System

### Mouse Events

- **Hover Detection**: Tracks mouse movement to determine which window is under cursor
- **Focus Management**: Automatically focuses windows when hovered over
- **Selection**: Allows selecting windows via mouse click

### Touch Events

- **Touch Selection**: Supports touch-based window selection
- **Gesture Integration**: Works alongside swipe gestures

### Gesture Support

- **3-Finger Swipes**: Configurable direction-based gestures
- **Animation Feedback**: Smooth transitions during gesture operations
- **Distance Threshold**: Configurable swipe distance for triggering

## Configuration System

The plugin provides extensive customization through Hyprland's configuration system:

### Configuration Variables

- `plugin:hyprview:margin`: Margin around each grid tile
- `plugin:hyprview:active_border_color`: Border color for focused window
- `plugin:hyprview:inactive_border_color`: Border color for inactive windows
- `plugin:hyprview:border_width`: Width of window borders
- `plugin:hyprview:border_radius`: Radius of window borders
- `plugin:hyprview:gesture_distance`: Swipe distance for gestures
- `plugin:hyprview:debug_log`: Enable debug logging

## Dispatcher System

The plugin uses Hyprland's dispatcher system for command execution:

### Available Dispatchers

- `hyprview:toggle`: Toggles the overview on/off
- `hyprview:on`: Opens the overview
- `hyprview:off`: Closes the overview
- `hyprview:select`: Selects the currently hovered window

### Command Parameters

- `all`: Includes all workspaces on the monitor
- `special`: Includes special (scratchpad) workspaces
- Combinations supported (e.g., `all,special`)

## Life Cycle Management

### Initialization

1. **Constructor**: Sets up the overview for a specific monitor
2. **Window Migration**: Moves windows to active workspace for rendering
3. **Framebuffer Creation**: Pre-renders windows to framebuffers
4. **Animation Setup**: Configures opening animations
5. **Input Hooking**: Sets up mouse/touch event handlers

### Cleanup

1. **Workspace Restoration**: Returns all windows to their original workspaces
2. **Framebuffer Cleanup**: Frees allocated framebuffers
3. **Hook Removal**: Unregisters input event handlers
4. **Instance Cleanup**: Removes from global instance map

## Animation System

The plugin uses Hyprland's animation system for smooth transitions:

### Opening Animation

- Scales from a point to full screen
- Positioned based on the initially focused window

### Closing Animation

- Scales from full screen back to the selected window
- Can target any window in the grid based on user selection
- Uses the window position as the final animation destination

## Memory Management

### Framebuffer Allocation

- Individual framebuffers per window with appropriate sizing
- Background framebuffer for seamless desktop integration
- Automatic cleanup in destructor
- Size adjustments when window sizes change

### Resource Tracking

- Reference counting for window objects
- Automatic cleanup of allocated resources
- Prevention of memory leaks during overview lifecycle

## Background Capture System

### Seamless Background Integration

The plugin implements a background capture system that provides a seamless experience by showing the original desktop background instead of a solid color:

1. **Window Hiding**: Before creating the overview, all windows on the target monitor are temporarily hidden
2. **Background Capture**: The original desktop background (wallpaper, etc.) is captured to a framebuffer
3. **Window Restoration**: Windows are restored to their original visibility state
4. **Background Rendering**: The captured background is used as the overview backdrop

### Implementation Details

- `captureBackground()` method in `CHyprView` class handles the background capture process
- Temporarily sets `m_hidden = true` on relevant windows
- Preserves original visibility states for proper restoration
- Uses the captured background texture in the `fullRender()` method

## Multi-Monitor Support

The plugin supports per-monitor overview functionality:

1. **Independent Instances**: Each monitor can have its own overview state
2. **Synchronized Actions**: Toggle commands affect all monitors simultaneously
3. **Monitor-Specific Rendering**: Each overview renders only windows relevant to its monitor

## Error Handling and Recovery

The plugin includes several safeguards:

1. **Null Pointer Checks**: Prevents crashes from invalid pointers
2. **Bounds Checking**: Ensures array indices are within valid ranges
3. **Resource Validation**: Verifies resources exist before use
4. **State Consistency**: Maintains consistent internal state during operations

## Debugging Features

### Logging System

- Configurable debug logging to `/tmp/hyprview.log`
- Timestamped log entries for performance analysis
- Detailed state tracking for troubleshooting

### Debug Variables

- `debug_log` configuration option to enable/disable logging
- Strategic log points throughout the codebase

## Customization Points

Developers can extend the plugin by:

1. **Adding New Collection Modes**: Extending the `EWindowCollectionMode` enum
2. **Enhancing Render Effects**: Modifying the rendering pipeline in `fullRender()`
3. **Gesture Enhancement**: Adding support for new gesture types
4. **Animation Customization**: Modifying animation parameters and behaviors
5. **UI Elements**: Adding additional visual elements to the overview

## Build and Integration

The plugin follows standard Hyprland plugin development practices:

1. **API Version Check**: Ensures compatibility with Hyprland version
2. **Dynamic Registration**: Registers dispatchers, hooks, and configuration values
3. **Clean Exit**: Proper cleanup on plugin unloading
4. **Shared Library**: Compiles to a loadable `.so` file

This architecture allows for efficient, smooth operation while maintaining the look and feel of a native Hyprland feature.
