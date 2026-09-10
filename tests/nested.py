#!/usr/bin/env python3
"""Exercise Hyprview only in a compositor launched by this test."""

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("plugin", type=lambda p: Path(p).resolve())
    parser.add_argument("--parent-display", default=os.environ.get("WAYLAND_DISPLAY"))
    parser.add_argument("--scale", type=int, choices=(1, 2), default=1)
    args = parser.parse_args()
    if not args.plugin.is_file() or not args.parent_display:
        parser.error("provide a built plugin and an existing parent Wayland display")
    for command in ("Hyprland", "hyprctl", "foot", "grim"):
        if not shutil.which(command):
            parser.error(f"missing test dependency: {command}")

    # Retain the log and screenshots on failure as well as success.
    root = Path(tempfile.mkdtemp(prefix="hyprview-test-"))
    print(f"Test artifacts: {root}", flush=True)
    config = root / "hyprland.lua"
    config.write_text(
        'hl.monitor({ output="", mode="1280x800@60", position="1280x0", scale=1 })\n'
        f'hl.monitor({{ output="TEST", mode="1280x800@60", position="0x0", scale={args.scale} }})\n'
        'hl.config({ misc={disable_hyprland_logo=true,disable_splash_rendering=true} })\n'
    )
    env = dict(os.environ, WAYLAND_DISPLAY=args.parent_display, AQ_DRM_DEVICES="/dev/null")
    with (root / "compositor.log").open("w") as log:
        child = subprocess.Popen(["Hyprland", "--config", str(config)], env=env,
                                 stdout=log, stderr=subprocess.STDOUT)
        try:
            instance = None
            for _ in range(50):
                if child.poll() is not None:
                    raise RuntimeError("nested compositor exited; see compositor.log")
                instances = json.loads(subprocess.check_output(
                    ["hyprctl", "-j", "instances"], timeout=10))
                instance = next((i for i in instances if i["pid"] == child.pid), None)
                if instance:
                    break
                time.sleep(0.2)
            assert instance, "nested compositor did not become ready"

            def ctl(*command):
                assert child.poll() is None, "nested compositor crashed"
                result = subprocess.run(["hyprctl", "-i", instance["instance"], *command],
                                        capture_output=True, text=True, timeout=10, check=True)
                if "error" in result.stdout.lower() or "invalid" in result.stdout.lower():
                    raise RuntimeError(result.stdout)
                return result.stdout

            def lua(code):
                return ctl("eval", code)

            def clients():
                return json.loads(ctl("-j", "clients"))

            def state():
                return {c["address"]: (c["workspace"]["id"], c["fullscreen"],
                                       c["fullscreenClient"]) for c in clients()}

            def overview(command):
                lua(f"assert(hl.plugin.hyprview.toggle({json.dumps(command)}))")
                time.sleep(1.5)

            time.sleep(1)
            ctl("output", "create", "headless", "TEST")
            ctl("plugin", "load", str(args.plugin))
            lua('hl.dispatch(hl.dsp.focus({monitor="TEST"}))')
            lua('hl.exec_cmd("foot --title=Hyprview-test-one"); '
                'hl.exec_cmd("foot --title=Hyprview-test-two")')
            time.sleep(2)
            assert len(clients()) == 2, clients()
            test_monitor = next(m for m in json.loads(ctl("-j", "monitors")) if m["name"] == "TEST")
            assert all(c["monitor"] == test_monitor["id"] for c in clients())
            other_workspace = clients()[0]["workspace"]["id"] + 1
            lua(f'hl.dispatch(hl.dsp.window.move({{workspace="{other_workspace}",follow=false}}))')
            time.sleep(0.5)
            before = state()
            assert len({s[0] for s in before.values()}) == 2, before
            for cycle in range(3):
                overview("on all special")
                if cycle == 0:
                    subprocess.run(["grim", "-o", "TEST", str(root / "overview.png")],
                                   env=dict(env, WAYLAND_DISPLAY=instance["wl_socket"]),
                                   check=True, timeout=10)
                overview("off")
                assert state() == before, (before, state())
                print(f"Open/close cycle {cycle + 1}: PASS", flush=True)
            ctl("reload")
            time.sleep(1)
            assert not ctl("configerrors").strip()
            overview("on all special")
            overview("off")
            assert state() == before
            # Unload while open must restore windows as well.
            overview("on all special")
            ctl("plugin", "unload", str(args.plugin))
            time.sleep(1)
            assert state() == before
            print("Reload, reopen, and unload restoration: PASS", flush=True)
        finally:
            if child.poll() is None:
                child.terminate()
                try:
                    child.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    child.kill()
                    child.wait()


if __name__ == "__main__":
    main()
