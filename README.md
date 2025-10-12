# hyprview

`hyprview` is a Hyprland plugin that provides a GNOME-style workspace overview. It displays all windows from the current workspace in a grid, allowing you to easily see and switch between them.

## Features

*   **Workspace Overview:** See all your open windows on the current workspace at a glance.
*   **Window Selection:** Hover to focus and click to select a window, automatically closing the overview.
*   **Trackpad Gestures:** Use swipe gestures to open and close the overview.
*   **Smooth Animations:** Animated transitions when opening/closing the overview.
*   **Multi-monitor Support:** Provides a separate overview for each monitor.
*   **Customizable Appearance:** Change colors, borders, margins, padding, and radii.
*   **Active Window Highlighting:** Distinguished border color for the currently focused window.
*   **Focus Restoration:** Properly restores window focus when closing the overview.

## Building

This project uses a simple `Makefile`.

1.  **Install dependencies:** Make sure you have the necessary development packages for the libraries listed in the `Makefile` (e.g., `hyprland`, `pixman-1`, `libdrm`, `pangocairo`, etc.).
2.  **Build the plugin:**
    ```sh
    make all
    ```
3.  **Install:** The compiled library `build/hyprview.so` can be loaded by Hyprland.

## Configuration

To use `hyprview`, you first need to load it in your `hyprland.conf`.

```ini
# ~/.config/hypr/hyprland.conf

# Load the plugin
# plugin = /path/to/your/build/hyprview.so
```

### Keybinds

You can bind the overview to a key.

```ini
# Toggle the overview with SUPER + H
bind = SUPER, H, hyprview:toggle
```

### Dispatchers

| Dispatcher | Description |
| --- | --- |
| `hyprview:toggle` | Toggles the overview on or off. |
| `hyprview:off` | Closes the overview. |
| `hyprview:close` | Alias for `hyprview:off`. |
| `hyprview:select` | Selects the currently hovered window and closes the overview. |

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

*   **`<fingers>`:** Number of fingers (e.g., `3`).
*   **`<direction>`:** `up`, `down`, `left`, `right`.
*   **`[mod:<modmask>]`:** Optional modifier keys (e.g., `mod:SUPER`).
*   **`[scale:<scale>]`:** Optional gesture scale factor.
*   **`<action>`:**
    *   `toggle`: Toggles the overview.
    *   `unset`: Removes the gesture.

### Customization

You can customize the appearance and behavior of the overview by setting the following variables in your `hyprland.conf`:

```ini
# Example customization
plugin:hyprview:margin = 20
plugin:hyprview:padding = 15
plugin:hyprview:bg_color = 0xFF222222
plugin:hyprview:grid_color = 0xFF111111
plugin:hyprview:active_border_color = 0xFFCA7815
plugin:hyprview:inactive_border_color = 0x88c0c0c0
plugin:hyprview:border_width = 5
plugin:hyprview:border_radius = 5
plugin:hyprview:gesture_distance = 250
plugin:hyprview:debug_log = 1
```

| Variable | Type | Description | Default |
| --- | --- | --- | --- |
| `plugin:hyprview:margin` | int | Space between grid slots. | `10` |
| `plugin:hyprview:padding` | int | Space inside grid slots before window content. | `10` |
| `plugin:hyprview:bg_color` | int (hex) | Background color (between grid slots). | `0xFF111111` |
| `plugin:hyprview:grid_color` | int (hex) | Grid slot background color (behind windows). | `0xFF000000` |
| `plugin:hyprview:active_border_color` | int (hex) | Border color for the currently focused window. | `0xFFCA7815` |
| `plugin:hyprview:inactive_border_color` | int (hex) | Border color for inactive windows. | `0x88c0c0c0` |
| `plugin:hyprview:border_width` | int | Width of window borders in pixels. | `5` |
| `plugin:hyprview:border_radius` | int | Radius of window borders in pixels. | `5` |
| `plugin:hyprview:gesture_distance` | int | The swipe distance required for the gesture. | `200` |
| `plugin:hyprview:debug_log` | int | Enable debug logging to `/tmp/hyprview.log` (`0` = disabled, `1` = enabled). | `0` |