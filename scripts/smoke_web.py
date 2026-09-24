# Copyright (c) 2026 e-soul.org
# SPDX-License-Identifier: BSD-2-Clause

from __future__ import annotations

import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from threading import Thread
from urllib.parse import quote

from playwright.sync_api import sync_playwright

from web_toolchain import load_toolchain


def smoke_export(browser, directory: Path, screenshots: Path | None = None) -> None:
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
        for width, height in ((1920, 1080), (2560, 1080), (1024, 768), (667, 375), (1920, 1080)):
            page.set_viewport_size({"width": width, "height": height})
            page.wait_for_function("""() => {
                const canvas = document.getElementById('canvas');
                const stage = document.getElementById('stage');
                return canvas.width === Math.round(stage.clientWidth * devicePixelRatio)
                    && canvas.height === Math.round(stage.clientHeight * devicePixelRatio)
                    && document.documentElement.scrollWidth === innerWidth;
            }""")
            if screenshots is not None:
                screenshots.mkdir(parents=True, exist_ok=True)
                page.wait_for_timeout(250)
                page.screenshot(path=str(screenshots / f"{directory.name}-{width}x{height}.png"))
                print("Display:", page.locator("#stage").evaluate("""stage => ({
                    width: stage.clientWidth, height: stage.clientHeight, density: devicePixelRatio,
                    fittedWidth: Math.min(stage.clientWidth, stage.clientHeight * 16 / 9),
                    fittedHeight: Math.min(stage.clientHeight, stage.clientWidth * 9 / 16)
                })"""))
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
    parser.add_argument("--screenshots", type=Path, help="Save the representative landscape menu captures.")
    args = parser.parse_args()
    with sync_playwright() as playwright:
        with playwright.chromium.launch(args=[
            "--use-gl=angle", "--use-angle=swiftshader", "--enable-unsafe-swiftshader",
        ]) as browser:
            for directory in args.directories:
                smoke_export(browser, directory.resolve(), args.screenshots)


if __name__ == "__main__":
    main()
