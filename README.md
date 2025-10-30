# hyprview

`hyprview` is a Hyprland plugin that provides a window overview with multiple placement algorithms. It can display windows from the current workspace, all workspaces on a monitor, or include special workspaces, organizing them using various layout algorithms for easy navigation.

The plugin includes five different placement algorithms to suit different preferences:

- **`grid` (default):** Efficient dynamic grid that adapts to window count and screen aspect ratio, packing windows without wasted space.

- **`spiral`:** Arranges windows in a spiral pattern starting from the center, creating a unique and balanced layout.

- **`flow`:** Proportional row-based layout that maintains relative window sizes and distributes them evenly across rows to avoid bunching small or large windows together.

- **`adaptive`:** Individual scaling with visual hierarchy - maintains each window's aspect ratio while applying subtle size variations (larger windows scaled down slightly, smaller windows boosted) for a dynamic, organic layout.

- **`wide`:** Wide horizontal grid with uniform tile sizes, prioritizing more columns and fewer rows with generous spacing between tiles.

https://github.com/user-attachments/assets/c0553bfe-6357-48e5-a4d0-50068096d800

## Features

* **Workspace Overview:** See all your open windows on the current workspace at a glance.
* **Multi-Workspace Modes:** View windows from the current workspace, all workspaces on the monitor, or include special (scratchpad) workspaces.
* **Workspace Indicator:** Each window tile shows its workspace ID (displayed as "wsid:N") in a configurable position with customizable size and styling. The indicator color automatically matches the window's border color (active or inactive) for easy navigation across multiple workspaces.
* **Window Selection:** Hover to focus and click to select a window, automatically closing the overview.
* **Trackpad Gestures:** Use swipe gestures to open and close the overview.
* **Gesture Conflict Prevention:** Automatically blocks workspace gestures when overview is active to prevent accidental workspace switches.
* **Smooth Animations:** Animated transitions when opening/closing the overview.
* **Multi-monitor Support:** Provides a separate overview for each monitor with proper scaling and positioning.
* **Customizable Appearance:** Change colors, borders, margins, background dimming, and radii.
* **Active Window Highlighting:** Distinguished border color for the currently focused window.
* **Background Dimming:** Configurable dark overlay when overview is active for clear visual distinction.
* **Centered Grid Layout:** Window grid is centered on screen for balanced visual appearance.
* **Focus Restoration:** Properly restores window focus when closing the overview.

## Install with hyprpm

```bash
hyprpm add https://github.com/yz778/hyprview
hyprpm enable hyprview
```

## Manual install

1. Clone this repository
2. Build the plugin: `make -C src all`
3. Add plugin to your `hyprland.conf`

```ini
plugin = /full_path_to/hyprview.so
```

### Keybinds

You can bind the overview to a key. The dispatcher accepts optional arguments to control the behavior.

```ini
# Toggle overview, show windows from current workspace only (default)
bind = SUPER, H, hyprview:toggle

# Toggle overview, show windows from all workspaces
bind = SUPER, H, hyprview:toggle, all

# Toggle overview, show windows from all workspaces including special workspaces
bind = SUPER, S, hyprview:toggle, all special

# Close the overview
bind = SUPER, ESC, hyprview:toggle, off

# Toggle overview with different placement algorithms
bind = SUPER, G, hyprview:toggle, placement:spiral
bind = SUPER, T, hyprview:toggle, placement:flow
bind = SUPER, A, hyprview:toggle, placement:adaptive
bind = SUPER, W, hyprview:toggle, placement:wide
```

### Dispatchers

#### hyprview:toggle

The `hyprview` dispatcher uses a flexible argument format: `hyprview:<action>[,<mode>]`.

* **`<action>`** (required): `toggle`
* **`<mode>`** (optional): A comma-separated list of keywords to define which windows to display.
  * `on`: Turn overview on
  * `off`: Turn overview off
  * `all`: Show windows from all workspaces on the monitor.
  * `special`: Include windows from the special (scratchpad) workspace.
  * `placement:<algorithm>`: Select the placement algorithm. Available algorithms:
    * `placement:grid` (default): Efficient dynamic grid
    * `placement:spiral`: Spiral pattern from center
    * `placement:flow`: Proportional row-based flow
    * `placement:adaptive`: Individual scaling with hierarchy
    * `placement:wide`: Wide horizontal uniform grid
  * Combining them (e.g., `all special placement:flow`) works as expected.

### Gestures

You can configure a trackpad gesture to control the overview.

```ini
# hyprland.conf

# 3-finger swipe down to toggle the overview
hyprview-gesture = 3,down,toggle

# To remove a gesture
hyprview-gesture = 3,down,unset
```

The syntax is `hyprview-gesture = <fingers>,<direction>[,mod:<modmask>][,scale:<scale>],<action>`.

* **`<fingers>`:** Number of fingers (e.g., `3`).
* **`<direction>`:** `up`, `down`, `left`, `right`.
* **`[mod:<modmask>]`:** Optional modifier keys (e.g., `mod:SUPER`).
* **`[scale:<scale>]`:** Optional gesture scale factor.
* **`<action>`:**
  * `toggle`: Toggles the overview.
  * `unset`: Removes the gesture.

### Customization

You can customize the appearance and behavior of the overview by setting the following variables in your `hyprland.conf`:


| Variable                                         | Type      | Description                                                                   | Default      |
| -------------------------------------------------- | ----------- | ------------------------------------------------------------------------------- | -------------- |
| `plugin:hyprview:active_border_color`            | int (hex) | Border color for the currently focused window. Also used for workspace ID text in active window labels. | `0xFFCA7815` |
| `plugin:hyprview:bg_dim`                         | float     | Opacity of the background dim overlay (0.0 = no dim, 1.0 = fully black).      | `0.4`        |
| `plugin:hyprview:border_radius`                  | int       | Radius of window borders in pixels.                                           | `5`          |
| `plugin:hyprview:border_width`                   | int       | Width of window borders in pixels.                                            | `5`          |
| `plugin:hyprview:gesture_distance`               | int       | The swipe distance required for the gesture.                                  | `200`        |
| `plugin:hyprview:inactive_border_color`          | int (hex) | Border color for inactive windows. Also used for workspace ID text in inactive window labels. | `0x88c0c0c0` |
| `plugin:hyprview:margin`                         | int       | Margin around each grid tile.                                                 | `10`         |
| `plugin:hyprview:workspace_indicator_enabled`    | int       | Show workspace ID in window labels (`0` = disabled, `1` = enabled).           | `1`          |
| `plugin:hyprview:window_name_enabled`            | int       | Show window info centered on bottom border as `[wsid] class • title` (`0` = disabled, `1` = enabled). When enabled, replaces the old workspace indicator overlay. | `1`          |
| `plugin:hyprview:window_name_font_size`          | int       | Font size for window labels in points.                                        | `20`         |
| `plugin:hyprview:window_name_bg_opacity`         | float     | Opacity of the window label background (0.0 = transparent, 1.0 = opaque).     | `0.85`       |
| `plugin:hyprview:window_text_color`              | int (hex) | Color for window class and title text in labels (format: 0xRRGGBBAA).         | `0xFFFFFFFF` (white) |

#### Deprecated Settings (Will be removed in next major version)

| Variable                                         | Type      | Description                                                                   | Default      | Replacement |
| -------------------------------------------------- | ----------- | ------------------------------------------------------------------------------- | -------------- | ----------- |
| `plugin:hyprview:workspace_indicator_font_size`  | int       | ⚠️ **DEPRECATED** - Font size now controlled by `window_name_font_size`.     | `28`         | Use `window_name_font_size` |
| `plugin:hyprview:workspace_indicator_position`   | string    | ⚠️ **DEPRECATED** - Position now centered on bottom border when `window_name_enabled = 1`. | (empty)      | Always centered (when window names enabled) |
| `plugin:hyprview:workspace_indicator_bg_opacity` | float     | ⚠️ **DEPRECATED** - Background opacity now controlled by `window_name_bg_opacity`. | `0.85`       | Use `window_name_bg_opacity` |

**Note:** When `window_name_enabled = 1` (default), the workspace indicator is integrated into the window label format: `[workspace_id] class • title`. The label is centered on the bottom border with separate color control:
- **Workspace ID** (`[workspace_id]`): Uses border color (active = `active_border_color`, inactive = `inactive_border_color`)
- **Window info** (`class • title`): Uses `window_text_color` (default: white)

The deprecated workspace indicator settings (`font_size`, `position`, `bg_opacity`) are ignored when window names are enabled. To use the old overlay-style workspace indicator, set `window_name_enabled = 0`.

**Migration Guide:** The old overlay-style workspace indicator (shown in the corner of each tile) has been replaced with an integrated label system. If you have custom settings for the old workspace indicator:
- Replace `workspace_indicator_font_size` with `window_name_font_size`
- Replace `workspace_indicator_bg_opacity` with `window_name_bg_opacity`
- The `workspace_indicator_position` setting is no longer needed as labels are always centered on the bottom border
- These deprecated settings will be removed in the next major version
