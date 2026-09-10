# Nested integration test

Build against headers matching the installed Hyprland binary. The test requires
Python 3, Hyprland/hyprctl, foot, and grim, plus a running Wayland compositor.

```sh
make -C src
python3 tests/nested.py build/hyprview.so --scale 1
python3 tests/nested.py build/hyprview.so --scale 2
```

Use `--parent-display wayland-N` if the terminal's `WAYLAND_DISPLAY` is stale.
The test starts its own nested compositor with a temporary Lua configuration and
a headless output. It identifies that compositor by its child PID and sends all
commands to that instance. It never loads the test plugin into the parent.
Logs and screenshots are retained in the printed temporary directory.

Checks cover two windows on separate workspaces, three open/close cycles, exact
workspace/fullscreen-state restoration, config reload, and unloading while open.
This exercises Lua callbacks directly; physical touchpad gestures still require
a manual check. It does not cover older Hyprland versions or rotated outputs.
