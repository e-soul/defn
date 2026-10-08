# Copyright (c) 2026 e-soul.org
# SPDX-License-Identifier: BSD-2-Clause
"""Small browser transport helpers; scenarios still describe ordinary player input."""
import json
import subprocess
import time


class BrowserCapture:
    def __init__(self, page, out):
        self.page, self.out = page, out
        out.mkdir(parents=True, exist_ok=True)
        self.messages, self.failures = [], []
        page.route("https://www.googletagmanager.com/**",
                   lambda route: route.fulfill(content_type="application/javascript", body=""))
        page.on("pageerror", lambda error: self.failures.append(str(error)))
        page.on("console", self._console)
        self.session = page.context.new_cdp_session(page)

    def _console(self, message):
        self.messages.append(message.text)
        if message.type == "error":
            self.failures.append(message.text)

    def load(self, url):
        self.page.goto(url, wait_until="domcontentloaded")
        self.page.wait_for_function("document.getElementById('overlay').hidden", timeout=120_000)
        self.page.locator("#canvas").wait_for(state="visible", timeout=10_000)
        # The shell has no exposed signal for the first rendered game frame.
        self.page.wait_for_timeout(700)

    def wait_for_log(self, text, timeout=10_000):
        deadline = time.monotonic() + timeout / 1000
        while time.monotonic() < deadline:
            if any(text in message for message in self.messages):
                return True
            self.page.wait_for_timeout(100)
        return False

    def box(self):
        bounds = self.page.locator("#canvas").bounding_box()
        if not bounds:
            raise RuntimeError("Game canvas is not visible")
        return bounds

    def point(self, x_fraction, y_fraction):
        bounds = self.box()
        return (bounds["x"] + bounds["width"] * x_fraction,
                bounds["y"] + bounds["height"] * y_fraction)

    def touch(self, kind, x=None, y=None):
        points = [] if kind == "touchEnd" else [{"x": x, "y": y}]
        self.session.send("Input.dispatchTouchEvent", {"type": kind, "touchPoints": points})

    def tap(self, x, y, settle=400):
        self.touch("touchStart", x, y)
        self.touch("touchEnd")
        self.page.wait_for_timeout(settle)

    def swipe(self, start, delta, steps=8):
        self.touch("touchStart", *start)
        for step in range(1, steps + 1):
            self.touch("touchMove", start[0] + delta[0] * step / steps,
                       start[1] + delta[1] * step / steps)
            self.page.wait_for_timeout(20)
        self.touch("touchEnd")
        self.page.wait_for_timeout(400)

    def save(self, name):
        self.page.screenshot(path=str(self.out / f"{name}.png"))
        bounds = self.box()
        exit_bounds = self.page.locator("#fullscreen").bounding_box()
        if exit_bounds and exit_bounds["y"] + exit_bounds["height"] > bounds["y"] + 1:
            raise RuntimeError("Shell fullscreen button overlaps the game canvas")
        print(name, json.dumps({"stage": bounds, "dpr": self.page.evaluate("devicePixelRatio")}), flush=True)


def rotate_browser(page, landscape, android_device=None, adb="adb"):
    if android_device:
        subprocess.run([adb, "-s", android_device, "shell", "settings", "put", "system",
                        "user_rotation", "1" if landscape else "0"], check=True)
    else:
        page.set_viewport_size({"width": 667 if landscape else 390,
                               "height": 375 if landscape else 844})
