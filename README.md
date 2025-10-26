# hyprview

`hyprview` is a Hyprland plugin that provides a GNOME-style overview. It can display windows from the current workspace, all workspaces on a monitor, or include special workspaces, organizing them in a configurable grid layout for easy navigation.

The grid layout adapts to the number of windows and the selected placement algorithm:

- **`grid` (default):** Adaptive layout that follows the original algorithm:
  - **1 window:** Displayed at 80% screen size, centered
  - **2 windows:** 2×1 grid
  - **3-4 windows:** 2×2 grid
  - **5-9 windows:** 3×3 grid
  - **10+ windows:** 4×N grid

- **`grid2` (row-based packing):** Dynamically fills each row with windows before moving to the next row, optimizing horizontal space usage.

- **`grid3` (optimal rectangle):** Calculates the most "square-like" grid layout by finding factor pairs that best match the monitor's aspect ratio, minimizing wasted space.

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
bind = SUPER, G, hyprview:toggle, placement:grid2
bind = SUPER, T, hyprview:toggle, placement:grid3
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
  * `placement:<algorithm>`: Select the grid placement algorithm. Available algorithms:
    * `placement:grid` (default): Original adaptive layout
    * `placement:grid2`: Row-based packing algorithm
    * `placement:grid3`: Optimal rectangle algorithm
  * Combining them (e.g., `all special placement:grid2`) works as expected.

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
| `plugin:hyprview:active_border_color`            | int (hex) | Border color for the currently focused window.                                | `0xFFCA7815` |
| `plugin:hyprview:bg_dim`                         | float     | Opacity of the background dim overlay (0.0 = no dim, 1.0 = fully black).      | `0.4`        |
| `plugin:hyprview:border_radius`                  | int       | Radius of window borders in pixels.                                           | `5`          |
| `plugin:hyprview:border_width`                   | int       | Width of window borders in pixels.                                            | `5`          |
| `plugin:hyprview:gesture_distance`               | int       | The swipe distance required for the gesture.                                  | `200`        |
| `plugin:hyprview:inactive_border_color`          | int (hex) | Border color for inactive windows.                                            | `0x88c0c0c0` |
| `plugin:hyprview:margin`                         | int       | Margin around each grid tile.                                                 | `10`         |
| `plugin:hyprview:workspace_indicator_enabled`    | int       | Show workspace indicator on window tiles (`0` = disabled, `1` = enabled).     | `1`          |
| `plugin:hyprview:workspace_indicator_font_size`  | int       | Font size for workspace indicator in points.                                  | `28`         |
| `plugin:hyprview:workspace_indicator_position`   | string    | Position: `top-left`, `top-right`, `bottom-left`, `bottom-right` (empty = top-right). | (empty)      |
| `plugin:hyprview:workspace_indicator_bg_opacity` | float     | Opacity of the indicator background (0.0 = transparent, 1.0 = opaque).        | `0.85`       |
