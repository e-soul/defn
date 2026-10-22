# Copyright (c) 2026 e-soul.org
# SPDX-License-Identifier: BSD-2-Clause

from __future__ import annotations

import argparse
from contextlib import contextmanager
from functools import partial
from html import escape
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import tempfile
from threading import Thread
from urllib.parse import quote
import zipfile

from playwright.sync_api import sync_playwright

from web_toolchain import load_toolchain


@contextmanager
def serve(directory: Path):
    server = ThreadingHTTPServer(("127.0.0.1", 0), partial(SimpleHTTPRequestHandler, directory=str(directory)))
    thread = Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        yield server.server_port
    finally:
        server.shutdown()
        server.server_close()
        thread.join()


def wait_for_startup(frame) -> None:
    frame.wait_for_function(
        "() => document.getElementById('overlay').hidden"
        " || document.getElementById('overlay').dataset.state === 'error'",
        timeout=120_000,
    )
    if frame.locator("#overlay").is_visible():
        raise RuntimeError(frame.locator("#message").inner_text())


def check_canvas(frame) -> None:
    frame.wait_for_function("""() => {
        const canvas = document.getElementById('canvas');
        const stage = document.getElementById('stage');
        return canvas.width === Math.round(stage.clientWidth * devicePixelRatio)
            && canvas.height === Math.round(stage.clientHeight * devicePixelRatio)
            && stage.clientWidth === innerWidth && stage.clientHeight === innerHeight
            && document.documentElement.scrollWidth === innerWidth
            && document.documentElement.scrollHeight === innerHeight;
    }""")


def smoke_itch(browser, archive_path: Path) -> None:
    """Exercise the actual upload ZIP in a cross-site iframe, including its parent-owned fullscreen."""
    with tempfile.TemporaryDirectory(prefix="defn-itch-smoke-") as temporary:
        directory = Path(temporary) / "upload"
        with zipfile.ZipFile(archive_path) as archive:
            archive.extractall(directory)
        with serve(directory.parent) as game_port, serve(directory.parent) as parent_port:
            game_url = f"http://127.0.0.1:{game_port}/upload/index.html"
            # Different hosts as well as ports exercise third-party iframe storage.
            parent_url = f"http://localhost:{parent_port}/embed"
            context = browser.new_context(viewport={"width": 1280, "height": 800})
            context.add_init_script("""(() => {
                window.__defnTestAudioContexts = [];
                for (const name of ['AudioContext', 'webkitAudioContext']) {
                    if (!window[name]) continue;
                    window[name] = new Proxy(window[name], {construct(target, args) {
                        const context = Reflect.construct(target, args);
                        window.__defnTestAudioContexts.push(context);
                        return context;
                    }});
                }
            })();""")
            page = context.new_page()
            messages, failures = [], []
            page.on("console", lambda message: (
                messages.append(message.text),
                failures.append(message.text) if message.type == "error" else None,
            ))
            page.on("pageerror", lambda error: failures.append(str(error)))
            page.on("requestfailed", lambda request: failures.append(f"{request.url}: {request.failure}"))
            page.on("response", lambda response: (
                failures.append(f"HTTP {response.status}: {response.url}") if response.status >= 400 else None
            ))
            page.route("https://www.googletagmanager.com/**", lambda route: route.fulfill(
                status=200, content_type="application/javascript", body="",
            ))
            page.route(parent_url, lambda route: route.fulfill(content_type="text/html", body=f"""<!doctype html>
                <title>Embedded Defn test</title><link rel="icon" href="data:,">
                <button id="fullscreen" onclick="document.getElementById('game').requestFullscreen()">Fullscreen</button>
                <button id="outside">Outside the game</button>
                <iframe id="game" title="Defn" width="960" height="540" style="border:0;display:block"
                    allow="autoplay; fullscreen; gamepad" allowfullscreen
                    sandbox="allow-scripts allow-same-origin allow-pointer-lock allow-orientation-lock"
                    onload="this.dataset.loaded='true'" src="{escape(game_url, quote=True)}"></iframe>"""))
            try:
                page.goto(parent_url, wait_until="domcontentloaded")
                page.wait_for_function("document.getElementById('game').dataset.loaded === 'true'")
                frame = page.frame(url=game_url)
                if frame is None:
                    raise RuntimeError("The itch test iframe did not load.")
                wait_for_startup(frame)
                if frame.locator("header").is_visible() or frame.locator("#fullscreen").is_visible():
                    raise RuntimeError("The itch shell duplicated the host's header or fullscreen control.")
                if frame.locator("#storage-notice").is_visible():
                    raise RuntimeError("Campaign storage is unavailable in the embedded game.")
                frame.locator("#canvas").click(position={"x": 8, "y": 8})
                frame.wait_for_function("window.__defnTestAudioContexts.some(context => context.state === 'running')")
                for width, height in ((960, 540), (640, 360), (390, 700)):
                    page.locator("#game").evaluate("(element, size) => { element.width = size[0]; element.height = size[1]; }", [width, height])
                    check_canvas(frame)
                page.locator("#fullscreen").click()
                page.wait_for_function("document.fullscreenElement?.id === 'game'")
                check_canvas(frame)
                page.evaluate("document.exitFullscreen()")
                page.wait_for_function("!document.fullscreenElement")
                check_canvas(frame)

                # Round-trip a real IDBFS campaign record through a new engine session.
                frame.wait_for_function("""async () => {
                    const db = await new Promise((ok, fail) => {
                        const request = indexedDB.open('/userfs', 21);
                        request.onsuccess = () => ok(request.result); request.onerror = () => fail(request.error);
                    });
                    try {
                        if (!db.objectStoreNames.contains('FILE_DATA')) return false;
                        return await new Promise((ok, fail) => {
                            const tx = db.transaction('FILE_DATA', 'readonly');
                            const request = tx.objectStore('FILE_DATA').get('/userfs/godot/app_userdata/defn/save_data.json');
                            request.onsuccess = () => ok(!!request.result); request.onerror = () => fail(request.error);
                        });
                    } finally { db.close(); }
                }""")
                frame.evaluate("""async () => {
                    const db = await new Promise((ok, fail) => {
                        const request = indexedDB.open('/userfs', 21);
                        request.onsuccess = () => ok(request.result); request.onerror = () => fail(request.error);
                    });
                    try {
                        await new Promise((ok, fail) => {
                            const tx = db.transaction('FILE_DATA', 'readwrite'), store = tx.objectStore('FILE_DATA');
                            const path = '/userfs/godot/app_userdata/defn/save_data.json', request = store.get(path);
                            request.onsuccess = () => {
                                const record = request.result;
                                const profile = JSON.parse(new TextDecoder().decode(record.contents));
                                profile.total_score = 123;
                                record.contents = new TextEncoder().encode(JSON.stringify(profile));
                                record.timestamp = new Date(); store.put(record, path);
                            };
                            tx.oncomplete = ok; tx.onerror = () => fail(tx.error);
                        });
                    } finally { db.close(); }
                }""")
                messages.clear()
                frame.goto(game_url, wait_until="domcontentloaded")
                wait_for_startup(frame)
                if not any("Loaded save - total_score=123" in message for message in messages):
                    raise RuntimeError("The embedded game did not restore its persisted campaign.")
                if failures:
                    raise RuntimeError("\n".join(failures))
                print("PASS: itch ZIP starts cross-site, fills its frame, enables audio, resizes/fullscreens and restores campaign storage.")
            finally:
                for message in messages:
                    print(f"[itch browser] {message}")
                context.close()


def smoke_export(browser, directory: Path) -> None:
    if not (directory / "index.html").is_file():
        raise FileNotFoundError(f"No Web export at {directory / 'index.html'}")
    # Project sites such as GitHub Pages serve the export below a URL prefix.
    handler = partial(SimpleHTTPRequestHandler, directory=str(directory.parent))
    server = ThreadingHTTPServer(("127.0.0.1", 0), handler)
    thread = Thread(target=server.serve_forever, daemon=True)
    thread.start()
    context = browser.new_context(viewport={"width": 1280, "height": 800})
    page = context.new_page()
    # Web startup must not depend on third-party telemetry availability.
    page.route(
        "https://www.googletagmanager.com/**",
        lambda route: route.fulfill(status=200, content_type="application/javascript", body=""),
    )
    messages = []
    failures = []
    page.on("console", lambda message: (
        messages.append(message.text),
        failures.append(message.text) if message.type == "error" else None,
    ))
    page.on("pageerror", lambda error: failures.append(str(error)))
    page.on("requestfailed", lambda request: failures.append(f"{request.url}: {request.failure}"))
    page.on("response", lambda response: (
        failures.append(f"HTTP {response.status}: {response.url}") if response.status >= 400 else None
    ))
    try:
        page.goto(
            f"http://127.0.0.1:{server.server_port}/{quote(directory.name)}/",
            wait_until="domcontentloaded",
        )
        page.wait_for_function(
            "() => document.getElementById('overlay').hidden"
            " || document.getElementById('overlay').dataset.state === 'error'",
            timeout=120_000,
        )
        if page.locator("#overlay").is_visible():
            raise RuntimeError(page.locator("#message").inner_text())
        pin = load_toolchain()
        for expected in (
            f"Godot Engine v{pin.godot}.stable",
            f"Emscripten {pin.emscripten}, single-threaded, GDExtension support.",
            "ContentValidator: content validation passed",
        ):
            if not any(expected in message for message in messages):
                raise RuntimeError(f"Missing runtime diagnostic: {expected}")
        for width, height in ((390, 844), (844, 390), (1280, 800)):
            page.set_viewport_size({"width": width, "height": height})
            page.wait_for_function("""() => {
                const canvas = document.getElementById('canvas');
                const stage = document.getElementById('stage');
                return canvas.width === Math.round(stage.clientWidth * devicePixelRatio)
                    && canvas.height === Math.round(stage.clientHeight * devicePixelRatio)
                    && document.documentElement.scrollWidth === innerWidth;
            }""")
        page.wait_for_timeout(1000)
        if failures:
            raise RuntimeError("\n".join(failures))
        print(f"PASS: {directory.name} starts the real extension and renders a responsive viewport.")
    finally:
        for message in messages:
            print(f"[browser] {message}")
        for failure in failures:
            print(f"[browser error] {failure}")
        context.close()
        server.shutdown()
        server.server_close()
        thread.join()


def main() -> None:
    parser = argparse.ArgumentParser(description="Smoke-test exported Web builds in Chromium.")
    parser.add_argument("directories", type=Path, nargs="+")
    parser.add_argument("--itch-archive", type=Path, help="Also test the itch upload ZIP in a cross-site iframe.")
    args = parser.parse_args()
    with sync_playwright() as playwright:
        with playwright.chromium.launch(args=[
            "--use-gl=angle", "--use-angle=swiftshader", "--enable-unsafe-swiftshader",
        ]) as browser:
            for directory in args.directories:
                smoke_export(browser, directory.resolve())
            if args.itch_archive:
                smoke_itch(browser, args.itch_archive.resolve())


if __name__ == "__main__":
    main()
