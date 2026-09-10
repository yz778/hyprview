# Nested integration test

Build against headers matching the installed Hyprland binary. The test requires
Python 3, Hyprland/hyprctl, foot, and grim, plus a running Wayland compositor.

```sh
make -C src
python3 tests/nested.py build/hyprview.so --scale 1
python3 tests/nested.py build/hyprview.so --scale 2
python3 tests/nested.py build/hyprview.so --scale 1 --fullscreen --selection default
python3 tests/nested.py build/hyprview.so --scale 2 --fullscreen --selection fullscreen
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

`--fullscreen` adds a fullscreen window and checks that the overview temporarily
clears its internal fullscreen mode, then restores both internal and client modes.
`--selection` additionally requires a C compiler, pkg-config, wayland-client
development files, and wayland-scanner. It builds a virtual pointer helper and
checks empty-background clicks, selection across workspaces, the default sticky
behavior, and optional fullscreen selection (including an already-fullscreen
selection). The helper connects only to the test compositor's socket.

`virtual-pointer.xml` comes from swaywm/wlr-protocols, at
`unstable/wlr-virtual-pointer-unstable-v1.xml`; its upstream license notice is
retained in the file.

Use `--check-cursor` to verify that a cursor image remains after dismissal,
selection, and unloading. This builds a small read-only observer plugin against
the installed Hyprland headers and loads it only into the nested compositor.
It requires a C++ compiler and `pkg-config --cflags hyprland` to resolve matching
headers (set `PKG_CONFIG_PATH` if using hyprpm's header installation). The check
fails on the previous cleanup code, which clears the cursor buffer while leaving
the renderer's cached cursor shape intact.
