# Copyright (c) 2026 e-soul.org
# SPDX-License-Identifier: BSD-2-Clause
"""Capture real exported-Web input with an isolated presentation save fixture.

Install scripts/requirements-web-test.txt and Playwright Chromium first. The
normal mode owns an ephemeral browser context. --cdp attaches to a test browser
(for example Android Chrome via adb forwarding); use a dedicated test origin.
No game handler or scene method is invoked: interactions are browser input.
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
from pathlib import Path
from urllib.parse import urljoin

from playwright.sync_api import sync_playwright
from capture_fixtures import FIXTURE_NAMES, capture_profile
from capture_web_transport import BrowserCapture, rotate_browser


def seed_profile(page, url: str, fixture: str) -> None:
    profile = capture_profile(fixture)
    # Stop the game before changing its test-origin IDBFS save. The browser
    # mounts this file normally on the next launch, just like a saved campaign.
    seed_url = urljoin(url, "__responsive_fixture__")
    page.route(seed_url, lambda route: route.fulfill(content_type="text/html", body="<!doctype html>"))
    page.goto(seed_url)
    page.evaluate("""async profile => {
        const db = await new Promise((ok, fail) => {
            const r = indexedDB.open('/userfs', 21);
            r.onupgradeneeded = () => {
                if (!r.result.objectStoreNames.contains('FILE_DATA')) {
                    r.result.createObjectStore('FILE_DATA').createIndex('timestamp', 'timestamp');
                }
            };
            r.onsuccess = () => ok(r.result); r.onerror = () => fail(r.error);
        });
        await new Promise((ok, fail) => {
            const tx = db.transaction('FILE_DATA', 'readwrite'), store = tx.objectStore('FILE_DATA');
            const directory = '/userfs/godot/app_userdata/defn';
            for (const path of ['/userfs/godot', '/userfs/godot/app_userdata', directory]) {
                store.put({timestamp: new Date(), mode: 16893}, path);
            }
            store.put({timestamp: new Date(), mode: 33206,
                       contents: new TextEncoder().encode(JSON.stringify(profile))}, directory + '/save_data.json');
            tx.oncomplete = ok; tx.onerror = () => fail(tx.error);
        }); db.close();
    }""", profile)


def browse_campaign(page, steps: int) -> None:
    """Send ordinary keyboard input to the carousel, never call a game handler."""
    page.locator("#canvas").focus()
    for _ in range(abs(steps)):
        page.keyboard.press("ArrowRight" if steps > 0 else "ArrowLeft")
        page.wait_for_timeout(250)


def capture_promotion(page, url: str, out: Path) -> None:
    """Earn a promotion through combat in the desktop Web export, without font fallback."""
    transport = BrowserCapture(page, out)
    seed_profile(page, url, "rewards")
    transport.load(url)

    def click(x, y):
        page.mouse.click(x, y)
        page.wait_for_timeout(500)

    def menu(row):
        bounds = transport.box()
        click(bounds["x"] + bounds["width"] / 2,
              bounds["y"] + bounds["height"] / 2 - 40 + row * 56)

    menu(0)
    menu(0)
    page.wait_for_timeout(1500)
    transport.save("campaign")
    bounds = transport.box()
    click(bounds["x"] + bounds["width"] * .86,
          bounds["y"] + bounds["height"] * .825)
    if not transport.wait_for_log("GameManager: Initializing"):
        (out / "runtime.log").write_text("\n".join(transport.messages), encoding="utf-8")
        raise RuntimeError("Desktop browser input did not deploy the first mission")
    page.wait_for_timeout(1000)
    bounds = transport.box()
    scale = min(bounds["width"] / 1920, bounds["height"] / 1080)
    for offset in (-100, 100):
        click(bounds["x"] + bounds["width"] / 2 + offset * scale,
              bounds["y"] + bounds["height"] - 70 * scale)
    transport.save("deployed")
    for seconds in range(10, 111, 10):
        page.wait_for_timeout(10_000)
        print(f"promotion: {seconds}s combat elapsed", flush=True)
        if seconds in (30, 90, 110):
            transport.save(f"combat-{seconds}")
    (out / "runtime.log").write_text("\n".join(transport.messages), encoding="utf-8")
    if transport.failures:
        raise RuntimeError("\n".join(transport.failures))


def capture(page, url: str, out: Path, fixture: str, rotate=None) -> None:
    transport = BrowserCapture(page, out)
    messages, failures = transport.messages, transport.failures
    seed_profile(page, url, fixture)
    transport.load(url)
    session = transport.session

    box = transport.box

    save = transport.save

    tap = lambda x, y: transport.tap(x, y, settle=350)

    def key(name="Escape"):
        page.locator("#canvas").focus()
        virtual = {"Escape": 27, "Tab": 9, "Enter": 13}[name]
        for kind in ("keyDown", "keyUp"):
            session.send("Input.dispatchKeyEvent", {"type": kind, "key": name, "code": name,
                                                    "windowsVirtualKeyCode": virtual})
        page.wait_for_timeout(350)

    def menu(row):
        bounds = box()
        # Three-row menus now fit their content; target the actual centered rows.
        tap(bounds["x"] + bounds["width"] / 2, bounds["y"] + bounds["height"] / 2 - 40 + row * 56)

    save("main-menu")
    menu(1)
    save("options")
    # Touch devices expose volume rather than window/display controls.
    bounds = box()
    tap(bounds["x"] + bounds["width"] * .4, bounds["y"] + bounds["height"] / 2 + 8)
    save("options-volume")
    key()
    menu(0)
    menu(1)
    save("progress-disabled")
    menu(0)
    save("campaign-carousel")
    bounds = box()
    if fixture == "rescue":
        browse_campaign(page, -1)  # Keep this existing match-flow fixture on mission 01.
    tap(bounds["width"] * .7, bounds["y"] + bounds["height"] - 42)
    page.wait_for_timeout(900)
    if not any("GameManager: Initializing" in message for message in messages):
        raise RuntimeError("Browser input did not navigate into a match; inspect preceding captures")
    save("match-before-drag")
    bounds = box()
    x, y = 95, bounds["y"] + 420
    transport.swipe((x, y), (0, 80), steps=4)
    save("match-after-drag")
    tap(x, y)
    page.wait_for_timeout(2000)
    save("match-deployed")
    tap(26, bounds["y"] + 330)
    save("unit-selected")
    tap(180, bounds["y"] + 330)
    page.wait_for_timeout(900)
    save("unit-reposition")
    if rotate:
        rotate(True)
        page.wait_for_timeout(700)
        save("match-landscape")
        rotate(False)
        page.wait_for_timeout(700)
    key()
    save("pause")
    if rotate:
        rotate(True)
        page.wait_for_timeout(700)
        save("pause-landscape")
        rotate(False)
        page.wait_for_timeout(700)
        save("pause-portrait-restored")
    key()
    save("match-resumed")
    # Fullscreen remains in portrait. The shell supplies the browser gesture.
    page.locator("#fullscreen").click()
    page.wait_for_timeout(700)
    save("fullscreen-portrait")
    if page.evaluate("!!document.fullscreenElement"):
        if rotate:
            rotate(True)
            page.wait_for_timeout(700)
            save("fullscreen-landscape")
            rotate(False)
            page.wait_for_timeout(700)
        page.locator("#fullscreen").click()
        page.wait_for_timeout(350)
    # Let the real simulation finish. The reward fixture survives breaches;
    # the maximum-roster fixture exercises defeat and its reachable footer.
    page.wait_for_timeout(90_000)
    save("results")
    if rotate:
        rotate(True)
        page.wait_for_timeout(700)
        save("results-landscape")
        rotate(False)
        page.wait_for_timeout(700)
    bounds = box()
    page.mouse.move(bounds["width"] / 2, bounds["y"] + bounds["height"] * .65)
    page.mouse.wheel(0, 1500)
    page.wait_for_timeout(500)
    save("results-scrolled")
    if page.viewport_size:
        page.set_viewport_size({"width": 320, "height": 844})
        page.wait_for_timeout(700)
        save("stress-320-results")
    (out / "runtime.log").write_text("\n".join(messages), encoding="utf-8")
    if failures:
        raise RuntimeError("\n".join(failures))


def capture_phone_landscape(page, url: str, out: Path, rotate, android_device=None, adb="adb", stress_resize=None) -> None:
    """Replay the compact phone match through real browser input, including rotation."""
    transport = BrowserCapture(page, out)
    messages, failures = transport.messages, transport.failures
    rotate(False)
    seed_profile(page, url, "max_roster")
    transport.load(url)
    session = transport.session

    box = transport.box

    tap = lambda x, y: transport.tap(x, y, settle=400)

    save = transport.save

    def menu(row):
        bounds = box()
        tap(bounds["x"] + bounds["width"] / 2,
            bounds["y"] + bounds["height"] / 2 - 40 + row * 56)

    save("main-portrait")
    if stress_resize:
        # Browser bars can leave a phone with only 230–250 logical units of game height.
        stress_resize({"width": 864, "height": 303})
        page.wait_for_timeout(700)
        save("main-landscape-short-250")
        menu(0)
        save("game-menu-landscape-short-250")
        stress_resize({"width": 864, "height": 283})
        page.wait_for_timeout(700)
        save("game-menu-landscape-short-230")
        menu(2)  # The last action must work without scrolling it into view.
        save("main-landscape-short-230")
        rotate(False)
        page.wait_for_timeout(700)
        save("main-portrait-restored")
    menu(0)
    page.wait_for_timeout(1000)
    menu(0)
    page.wait_for_timeout(1000)
    save("campaign")
    bounds = box()
    # The completed fixture initially selects mission 01; browse to Summar Beach.
    browse_campaign(page, 2)
    page.wait_for_timeout(700)
    save("mission")
    tap(bounds["x"] + bounds["width"] * .7, bounds["y"] + bounds["height"] - 42)
    page.wait_for_timeout(700)
    if not any("GameManager: Initializing" in message for message in messages):
        raise RuntimeError("Input did not navigate into the match")
    save("portrait")
    rotate(True)
    page.wait_for_timeout(700)
    save("landscape")
    if stress_resize:
        for height in (303, 283):
            stress_resize({"width": 864, "height": height})
            page.wait_for_timeout(700)
            save(f"landscape-windowed-short-{height - 53}")
    bounds = box()
    # Pause has its own 48-unit target at the top-right, outside the centered readings.
    tap(bounds["x"] + bounds["width"] - 28, bounds["y"] + 28)
    save("landscape-pause")
    page.locator("#canvas").focus()
    for kind in ("keyDown", "keyUp"):
        session.send("Input.dispatchKeyEvent", {"type": kind, "key": "Escape", "code": "Escape", "windowsVirtualKeyCode": 27})
    page.wait_for_timeout(400)
    rotate(True)
    page.wait_for_timeout(700)
    page.locator("#fullscreen").click()
    page.wait_for_timeout(700)
    save("fullscreen-landscape")
    bounds = box()
    # Scroll and then tap the card strip, always at its current bottom edge.
    x, y = bounds["x"] + bounds["width"] - 90, bounds["y"] + bounds["height"] - 28
    transport.swipe((x, y), (-160, 0))
    save("after-drag")
    tap(x, y)
    save("after-tap")
    page.wait_for_timeout(2000)
    save("combat")
    # Spend energy through the same visible card to expose enabled/disabled
    # contrasts, rather than setting resources or calling a deployment handler.
    for index in range(3):
        tap(x, y)
        save(f"landscape-card-states-{index + 1}")
    if android_device:
        (out / "device-landscape.png").write_bytes(subprocess.run(
            [adb, "-s", android_device, "exec-out", "screencap", "-p"],
            capture_output=True, check=True).stdout)
    rotate(False)
    page.wait_for_timeout(700)
    save("portrait-restored")
    page.wait_for_timeout(6000)
    save("portrait-card-states-recovering")
    rotate(True)
    page.wait_for_timeout(700)
    save("landscape-restored")
    (out / "runtime.log").write_text("\n".join(messages), encoding="utf-8")
    if failures:
        raise RuntimeError("\n".join(failures))


def capture_score_screens(page, url: str, out: Path, outcome: str, rotate, defeat_fixture="max_roster",
                          interactions=False, stress_resize=None) -> None:
    """Reach results by playing the first mission, then capture both phone orientations."""
    transport = BrowserCapture(page, out)
    messages, failures = transport.messages, transport.failures
    rotate(False)
    seed_profile(page, url, "rewards" if outcome == "victory" else "max_roster" if outcome == "endless" else defeat_fixture)
    transport.load(url)
    inputs = page.evaluate("""() => ({maxTouchPoints: navigator.maxTouchPoints,
        primaryCoarsePointer: matchMedia('(pointer: coarse)').matches,
        anyFinePointer: matchMedia('(any-pointer: fine)').matches})""")
    (out / "browser-inputs.json").write_text(json.dumps(inputs, indent=2), encoding="utf-8")
    print("browser-inputs", json.dumps(inputs), flush=True)
    session = transport.session

    box = transport.box

    tap = lambda x, y: transport.tap(x, y, settle=350)

    def menu(row):
        bounds = box()
        tap(bounds["x"] + bounds["width"] / 2,
            bounds["y"] + bounds["height"] / 2 - 40 + row * 56)

    save = transport.save

    save("main-menu")
    menu(0)
    page.wait_for_timeout(1000)
    save("campaign-menu")
    menu(0)
    page.wait_for_timeout(1000)
    save("campaign-carousel")
    bounds = box()
    if outcome == "endless":
        browse_campaign(page, 5)
    elif outcome == "defeat" and defeat_fixture == "rescue":
        browse_campaign(page, -1)
    page.wait_for_timeout(700)
    save("campaign-detail")
    tap(bounds["x"] + bounds["width"] * .7, bounds["y"] + bounds["height"] - 42)
    if not transport.wait_for_log("GameManager: Initializing", timeout=10_000):
        save("navigation-failed")
        (out / "runtime.log").write_text("\n".join(messages), encoding="utf-8")
        raise RuntimeError("Input did not navigate into the match")
    rotate(True)
    page.wait_for_timeout(700)
    if outcome == "victory":
        # The existing isolated reward fixture has two friendly cards. Deploy both
        # through the real bottom strip; its artificial upgrades make the clear reliable.
        bounds = box()
        tap(bounds["x"] + bounds["width"] / 2 - 100, bounds["y"] + bounds["height"] - 28)
        tap(bounds["x"] + bounds["width"] / 2 + 100, bounds["y"] + bounds["height"] - 28)
        for _ in range(6):
            page.wait_for_timeout(3000)
            tap(bounds["x"] + bounds["width"] / 2 - 100, bounds["y"] + bounds["height"] - 28)
    save("match")
    # No game handlers or time scaling: let the actual simulation end normally.
    for seconds in range(25, (150 if outcome == "victory" else 75) + 1, 25):
        page.wait_for_timeout(25_000)
        print(f"{outcome}: {seconds}s elapsed", flush=True)
    save("landscape")
    bounds = box()
    page.mouse.move(bounds["x"] + bounds["width"] / 2, bounds["y"] + bounds["height"] * .65)
    page.mouse.wheel(0, 1800)
    page.wait_for_timeout(500)
    save("landscape-scrolled")
    rotate(False)
    page.wait_for_timeout(700)
    # Return to the top of the same results body, rather than reseeding a model.
    bounds = box()
    page.mouse.move(bounds["x"] + bounds["width"] / 2, bounds["y"] + bounds["height"] * .65)
    page.mouse.wheel(0, -3000)
    page.wait_for_timeout(500)
    save("portrait")
    page.mouse.wheel(0, 3000)
    page.wait_for_timeout(500)
    save("portrait-scrolled")
    if interactions:
        # Every page is opened through its visible touch target, never a game handler.
        bounds = box()
        reward = outcome == "victory" or defeat_fixture == "rescue"
        if reward:
            tap(bounds["x"] + bounds["width"] / 6, bounds["y"] + bounds["height"] - 36)
            save("portrait-rewards")
            rotate(True)
            page.wait_for_timeout(700)
            save("landscape-rewards")
            if stress_resize:
                stress_resize({"width": 864, "height": 283})
                page.wait_for_timeout(700)
                save("short-landscape-rewards")
                bounds = box()
                tap(bounds["x"] + bounds["width"] * 5 / 6, bounds["y"] + bounds["height"] - 36)
                save("short-landscape-reward-next")
            rotate(False)
            page.wait_for_timeout(700)
            save("portrait-rewards-restored")
            bounds = box()
            tap(bounds["x"] + bounds["width"] / 2, bounds["y"] + bounds["height"] - 36)
            save("portrait-before-selection")
            tap(bounds["x"] + bounds["width"] / 6, bounds["y"] + bounds["height"] - 36)
            # The first full-description card starts below the reward title/subtitle.
            tap(bounds["x"] + bounds["width"] / 2, bounds["y"] + 115)
            save("portrait-after-selection")
            rotate(True)
            page.wait_for_timeout(700)
            save("landscape-after-selection")
            rotate(False)
            page.wait_for_timeout(700)
        has_owned = outcome in ("victory", "endless") or defeat_fixture != "fresh"
        if has_owned:
            bounds = box()
            tap(bounds["x"] + bounds["width"] * .75, bounds["y"] + 36)
            save("portrait-owned")
            rotate(True)
            page.wait_for_timeout(700)
            save("landscape-owned")
            if stress_resize:
                stress_resize({"width": 864, "height": 283})
                page.wait_for_timeout(700)
                save("short-landscape-owned")
                # The rescue fixture has at most two owned types after selection;
                # they fit this stage and therefore have no Next button.
                if outcome in ("victory", "endless") or defeat_fixture == "max_roster":
                    bounds = box()
                    tap(bounds["x"] + bounds["width"] * 5 / 6, bounds["y"] + bounds["height"] - 36)
                    save("short-landscape-owned-next")
            rotate(False)
            page.wait_for_timeout(700)
            save("portrait-owned-restored")
        elif stress_resize:
            stress_resize({"width": 864, "height": 283})
            page.wait_for_timeout(700)
            save("short-landscape")
    (out / "runtime.log").write_text("\n".join(messages), encoding="utf-8")
    if failures:
        raise RuntimeError("\n".join(failures))


def capture_campaign_carousel(page, url: str, out: Path, fixture: str, rotate) -> None:
    """Exercise the exported carousel through touch, with an isolated save."""
    transport = BrowserCapture(page, out)
    messages, failures = transport.messages, transport.failures
    rotate(False)
    seed_profile(page, url, fixture)
    transport.load(url)
    session = transport.session

    box = transport.box

    save = transport.save

    tap = lambda x, y: transport.tap(x, y, settle=400)

    def menu(row=0):
        bounds = box()
        tap(bounds["x"] + bounds["width"] / 2,
            bounds["y"] + bounds["height"] / 2 - 40 + row * 56)

    def swipe(direction):
        transport.swipe(transport.point(.5, .4), (direction * 160, 0))

    menu()
    menu()
    page.wait_for_timeout(1500)
    save("portrait-initial")
    rotate(True)
    page.wait_for_timeout(900)
    save("landscape-initial")
    rotate(False)
    page.wait_for_timeout(700)
    swipe(-1)
    save("portrait-swiped")
    rotate(True)
    page.wait_for_timeout(900)
    save("landscape-preserved")
    swipe(1)
    save("landscape-swiped-back")
    rotate(False)
    page.wait_for_timeout(900)
    # Browse with the actual touch arrow targets as well as the illustration swipe.
    count = 6 if fixture == "max_roster" else 5
    for index in range(count):
        save(f"portrait-card-{index + 1}")
        if index + 1 < count:
            bounds = box()
            nav_width = 112 + count * 32
            tap(bounds["x"] + bounds["width"] / 2 + nav_width / 2 - 24,
                bounds["y"] + bounds["height"] - 92)
    rotate(True)
    page.wait_for_timeout(900)
    save("landscape-final")
    page.locator("#fullscreen").click()
    page.wait_for_timeout(900)
    save("fullscreen-landscape-final")
    bounds = box()
    width = bounds["width"] - 24
    action_width = max(120, width * .18)
    tap(bounds["x"] + bounds["width"] - 12 - action_width / 2,
        bounds["y"] + bounds["height"] - 36)
    page.wait_for_timeout(1200)
    initialized = any("GameManager: Initializing" in message for message in messages)
    if initialized != (fixture == "max_roster"):
        raise RuntimeError("Carousel deployment/unlock gate failed; inspect captures")
    save("endless-deployed" if initialized else "locked-action-stayed-on-card")
    (out / "runtime.log").write_text("\n".join(messages), encoding="utf-8")
    if failures:
        raise RuntimeError("\n".join(failures))


def capture_shell(page, url: str, out: Path, rotate, stress_resize=None) -> None:
    """Inspect the portrait hint and fullscreen without starting a match."""
    transport = BrowserCapture(page, out)
    rotate(False)
    transport.load(url)

    def save(name, portrait):
        if page.locator("#orientation-hint").is_visible() != portrait:
            raise RuntimeError(f"Orientation hint visibility is wrong in {name}")
        if page.evaluate("document.documentElement.scrollWidth > innerWidth"):
            raise RuntimeError(f"Shell overflows horizontally in {name}")
        hint = page.locator("#orientation-hint").bounding_box()
        button = page.locator("#fullscreen").bounding_box()
        if hint and button and hint["x"] + hint["width"] > button["x"]:
            raise RuntimeError(f"Orientation hint overlaps fullscreen in {name}")
        transport.save(name)

    save("portrait", True)
    if stress_resize:
        stress_resize({"width": 320, "height": 844})
        page.wait_for_timeout(500)
        save("portrait-narrow", True)
        rotate(False)
        page.wait_for_timeout(500)
    if page.locator("#fullscreen").is_visible():
        page.locator("#fullscreen").click()
        page.wait_for_timeout(700)
        # Real phones may rotate automatically; emulated browsers may reject the lock.
        save("fullscreen", page.evaluate("innerHeight >= innerWidth"))
        page.locator("#fullscreen").click()
        page.wait_for_timeout(500)
    rotate(True)
    page.wait_for_timeout(700)
    save("landscape", False)
    rotate(False)
    page.wait_for_timeout(700)
    save("portrait-restored", True)
    (out / "runtime.log").write_text("\n".join(transport.messages), encoding="utf-8")
    if transport.failures:
        raise RuntimeError("\n".join(transport.failures))


def read_saved_volume(page) -> float | None:
    """Observe the normal IDBFS settings file, without invoking a game handler."""
    text = page.evaluate("""async () => {
        const db = await new Promise((ok, fail) => {
            const request = indexedDB.open('/userfs', 21);
            request.onsuccess = () => ok(request.result);
            request.onerror = () => fail(request.error);
        });
        const text = await new Promise((ok, fail) => {
            const request = db.transaction('FILE_DATA', 'readonly').objectStore('FILE_DATA')
                .get('/userfs/godot/app_userdata/defn/settings.cfg');
            request.onsuccess = () => ok(request.result?.contents
                ? new TextDecoder().decode(request.result.contents) : '');
            request.onerror = () => fail(request.error);
        });
        db.close();
        return text;
    }""")
    match = re.search(r"^master_volume\s*=\s*([\d.]+)", text, re.MULTILINE)
    return float(match.group(1)) if match else None


def capture_options(page, url: str, out: Path, fixture: str, rotate, stress_resize=None) -> None:
    """Change and restore mobile volume through touch; inspect both orientations."""
    transport = BrowserCapture(page, out)
    rotate(False)
    seed_profile(page, url, fixture)
    transport.load(url)

    def open_options():
        bounds = transport.box()
        transport.tap(bounds["x"] + bounds["width"] / 2,
                      bounds["y"] + bounds["height"] / 2 + 16)

    open_options()
    transport.save("portrait")
    bounds = transport.box()
    transport.tap(bounds["x"] + bounds["width"] * .4,
                  bounds["y"] + bounds["height"] / 2 + 8)
    for _ in range(50):
        volume = read_saved_volume(page)
        if volume is not None and 0 < volume < 100:
            break
        page.wait_for_timeout(100)
    else:
        raise RuntimeError("Touch input did not change and persist Master Volume")
    transport.save("portrait-volume-changed")
    rotate(True)
    page.wait_for_timeout(700)
    transport.save("landscape")
    if stress_resize:
        stress_resize({"width": 864, "height": 283})
        page.wait_for_timeout(700)
        transport.save("short-landscape")
    bounds = transport.box()
    transport.tap(bounds["x"] + bounds["width"] / 2,
                  bounds["y"] + bounds["height"] / 2 + 64)
    transport.save("back-to-main")
    rotate(False)
    page.wait_for_timeout(700)
    open_options()
    transport.save("portrait-reopened")
    transport.load(url)
    open_options()
    transport.save("portrait-reloaded")
    restored_volume = read_saved_volume(page)
    if restored_volume != volume:
        raise RuntimeError(f"Volume changed after reload: {volume} -> {restored_volume}")
    print(f"PASS: mobile volume persisted at {volume}% across navigation, rotation and reload", flush=True)
    page.locator("#canvas").focus()
    page.keyboard.press("Escape")
    page.wait_for_timeout(350)

    def menu(row):
        bounds = transport.box()
        transport.tap(bounds["x"] + bounds["width"] / 2,
                      bounds["y"] + bounds["height"] / 2 - 40 + row * 56)

    menu(0)
    transport.save("game-menu-portrait")
    menu(1)  # A disabled Progress action must leave the game menu open.
    transport.save("progress-disabled-portrait")
    rotate(True)
    page.wait_for_timeout(700)
    menu(1)
    transport.save("progress-disabled-landscape")
    if stress_resize:
        stress_resize({"width": 864, "height": 283})
        page.wait_for_timeout(700)
        menu(1)
        transport.save("progress-disabled-short-landscape")
    menu(2)
    transport.save("game-menu-back-to-main")
    (out / "runtime.log").write_text("\n".join(transport.messages), encoding="utf-8")
    if transport.failures:
        raise RuntimeError("\n".join(transport.failures))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("url")
    parser.add_argument("--out-dir", type=Path, default=Path("build/capture/responsive-web-flow"))
    parser.add_argument("--fixture", choices=FIXTURE_NAMES, default="max_roster")
    parser.add_argument("--cdp", help="Attach to a dedicated test Chrome debugging URL")
    parser.add_argument("--campaign-carousel", action="store_true", help="Capture carousel swipes, arrows, rotation, lock gate and endless mode")
    parser.add_argument("--options", action="store_true", help="Capture mobile volume, rotation, saved settings and disabled Progress")
    parser.add_argument("--shell", action="store_true", help="Capture the portrait hint, narrow header, fullscreen and rotation")
    parser.add_argument("--promotion", action="store_true", help="Capture an earned promotion in the desktop Web export")
    parser.add_argument("--phone-landscape", action="store_true", help="Capture the thin phone HUD and bottom deploy strip")
    parser.add_argument("--score-screens", choices=("victory", "defeat", "endless"), help="Play into results and capture portrait/landscape")
    parser.add_argument("--score-interactions", action="store_true", help="Exercise result/reward/collection paging and selection through touch")
    parser.add_argument("--android-device", help="ADB device for actual rotation in phone/score captures with --cdp")
    parser.add_argument("--adb", default="adb", help="ADB executable")
    args = parser.parse_args()
    if sum(bool(mode) for mode in (args.phone_landscape, args.score_screens, args.campaign_carousel, args.options, args.shell, args.promotion)) > 1:
        parser.error("Choose one of --campaign-carousel, --options, --shell, --promotion, --phone-landscape or --score-screens")
    if args.score_interactions and not args.score_screens:
        parser.error("--score-interactions requires --score-screens")
    if (args.phone_landscape or args.score_screens or args.campaign_carousel or args.options or args.shell) and args.cdp and not args.android_device:
        parser.error("Phone captures with --cdp require --android-device for actual rotation")
    with sync_playwright() as pw:
        if args.cdp:
            browser = pw.chromium.connect_over_cdp(args.cdp)
            page = browser.contexts[0].pages[-1]
            if args.promotion:
                capture_promotion(page, args.url, args.out_dir)
            elif args.phone_landscape or args.score_screens or args.campaign_carousel or args.options or args.shell:
                def rotate(landscape):
                    rotate_browser(page, landscape, args.android_device, args.adb)
                if args.shell:
                    capture_shell(page, args.url, args.out_dir, rotate)
                elif args.options:
                    capture_options(page, args.url, args.out_dir, args.fixture, rotate)
                elif args.campaign_carousel:
                    capture_campaign_carousel(page, args.url, args.out_dir, args.fixture, rotate)
                elif args.score_screens:
                    capture_score_screens(page, args.url, args.out_dir, args.score_screens, rotate, args.fixture, args.score_interactions)
                else:
                    capture_phone_landscape(page, args.url, args.out_dir, rotate, args.android_device, args.adb)
            else:
                capture(page, args.url, args.out_dir, args.fixture)
        else:
            with pw.chromium.launch(headless=True, args=["--enable-unsafe-swiftshader"]) as browser:
                context = browser.new_context(viewport={"width": 1920, "height": 1141} if args.promotion else {"width": 390, "height": 844},
                                              device_scale_factor=1 if args.promotion else 3,
                                              has_touch=not args.promotion, is_mobile=not args.promotion)
                page = context.new_page()
                rotate = lambda landscape: rotate_browser(page, landscape)
                if args.promotion:
                    capture_promotion(page, args.url, args.out_dir)
                elif args.shell:
                    capture_shell(page, args.url, args.out_dir, rotate, page.set_viewport_size)
                elif args.options:
                    capture_options(page, args.url, args.out_dir, args.fixture, rotate, page.set_viewport_size)
                elif args.campaign_carousel:
                    capture_campaign_carousel(page, args.url, args.out_dir, args.fixture, rotate)
                elif args.score_screens:
                    capture_score_screens(page, args.url, args.out_dir, args.score_screens, rotate, args.fixture,
                                          args.score_interactions, page.set_viewport_size)
                elif args.phone_landscape:
                    capture_phone_landscape(page, args.url, args.out_dir, rotate, stress_resize=page.set_viewport_size)
                else:
                    capture(page, args.url, args.out_dir, args.fixture, rotate)


if __name__ == "__main__":
    main()
