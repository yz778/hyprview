# hyprview

`hyprview` is a Hyprland plugin that provides a GNOME-style overview. It can display windows from the current workspace, all workspaces on a monitor, or include special workspaces, organizing them in an adaptive grid layout for easy navigation.

The grid layout adapts to the number of windows:
- **1 window:** Displayed at 80% screen size, centered
- **2 windows:** 2×1 grid
- **3-4 windows:** 2×2 grid
- **5-9 windows:** 3×3 grid
- **10+ windows:** 4×N grid

https://github.com/user-attachments/assets/c0553bfe-6357-48e5-a4d0-50068096d800

## Features

* **Workspace Overview:** See all your open windows on the current workspace at a glance.
* **Multi-Workspace Modes:** View windows from the current workspace, all workspaces on the monitor, or include special (scratchpad) workspaces.
* **Window Selection:** Hover to focus and click to select a window, automatically closing the overview.
* **Trackpad Gestures:** Use swipe gestures to open and close the overview.
* **Smooth Animations:** Animated transitions when opening/closing the overview.
* **Multi-monitor Support:** Provides a separate overview for each monitor.
* **Customizable Appearance:** Change colors, borders, margins, and radii.
* **Active Window Highlighting:** Distinguished border color for the currently focused window.
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
bind = SUPER, ESC, hyprview:off
```

### Dispatchers

The `hyprview` dispatcher uses a flexible argument format: `hyprview:<action>[,<mode>]`.

* **`<action>`** (required): `toggle`, `on`, `off`, `select`.
* **`<mode>`** (optional): A comma-separated list of keywords to define which windows to display.
  * `all`: Show windows from all workspaces on the monitor.
  * `special`: Include windows from the special (scratchpad) workspace.
  * Combining them (e.g., `all,special`) works as expected.

If no mode is specified, it defaults to showing windows from the **current workspace only**.


| Dispatcher / Action | Mode Argument(s)    | Description                                                                                                       |
| --------------------- | --------------------- | ------------------------------------------------------------------------------------------------------------------- |
| `hyprview:toggle`   | `[all]` `[special]` | Toggles the overview. If arguments are provided, the overview will open with that mode if it wasn't already open. |
| `hyprview:on`       | `[all]` `[special]` | Turns the overview on with the specified mode. Does nothing if already active.                                    |
| `hyprview:off`      | (none)              | Closes the overview.                                                                                              |
| `hyprview:close`    | (none)              | Alias for`hyprview:off`.                                                                                          |
| `hyprview:select`   | (none)              | Selects the currently hovered window and closes the overview.                                                     |

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

```ini
# Example customization
plugin:hyprview:margin = 20
plugin:hyprview:active_border_color = 0xFFCA7815
plugin:hyprview:inactive_border_color = 0x88c0c0c0
plugin:hyprview:border_width = 5
plugin:hyprview:border_radius = 5
plugin:hyprview:gesture_distance = 250
plugin:hyprview:debug_log = 1
```


| Variable                                | Type      | Description                                                                 | Default      |
| ----------------------------------------- | ----------- | ----------------------------------------------------------------------------- | -------------- |
| `plugin:hyprview:margin`                | int       | Margin around each grid tile.                                               | `10`         |
| `plugin:hyprview:active_border_color`   | int (hex) | Border color for the currently focused window.                              | `0xFFCA7815` |
| `plugin:hyprview:inactive_border_color` | int (hex) | Border color for inactive windows.                                          | `0x88c0c0c0` |
| `plugin:hyprview:border_width`          | int       | Width of window borders in pixels.                                          | `5`          |
| `plugin:hyprview:border_radius`         | int       | Radius of window borders in pixels.                                         | `5`          |
| `plugin:hyprview:gesture_distance`      | int       | The swipe distance required for the gesture.                                | `200`        |
| `plugin:hyprview:debug_log`             | int       | Enable debug logging to`/tmp/hyprview.log` (`0` = disabled, `1` = enabled). | `0`          |
