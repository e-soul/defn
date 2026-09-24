// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const test = require("node:test");
const vm = require("node:vm");

const html = fs.readFileSync(path.join(__dirname, "../export_templates/web_shell.html"), "utf8");
const config = { executable: "renamed-game", gdextensionLibs: ["extension.wasm"] };
const source = [...html.matchAll(/<script(?:\s[^>]*)?>([\s\S]*?)<\/script>/g)]
	.map(match => match[1])
	.find(script => script.includes("$GODOT_CONFIG"))
	.replace("$GODOT_CONFIG", JSON.stringify(config))
	.replace("$GODOT_THREADS_ENABLED", "false")
	.replace("$GODOT_URL", "renamed-game.js");

function harness({ missing = [], startGame = async () => {}, fullscreenEnabled = true } = {}) {
	const elements = new Map();
	const calls = { errors: [], reloads: 0, fullscreenRequests: 0, fullscreenExits: 0 };
	const makeElement = () => ({
		hidden: false, dataset: {}, attributes: {}, listeners: {},
		textContent: "", clientWidth: 1280, clientHeight: 640,
		addEventListener(name, callback) { this.listeners[name] = callback; },
		setAttribute(name, value) { this.attributes[name] = value; },
		removeAttribute(name) { delete this.attributes[name]; if (name === "value") delete this.value; },
		focus() { calls.focus = this; },
	});
	for (const match of html.matchAll(/<[^>]+\bid="([^"]+)"[^>]*>/g)) {
		const element = makeElement();
		element.hidden = /\bhidden\b/.test(match[0]);
		elements.set(match[1], element);
	}
	const document = {
		...makeElement(),
		fullscreenEnabled,
		fullscreenElement: null,
		getElementById(id) {
			return elements.get(id) || null;
		},
		createElement: makeElement,
		body: { appendChild(script) { calls.script = script; } },
		documentElement: {
			async requestFullscreen() {
				calls.fullscreenRequests++;
				document.fullscreenElement = document.documentElement;
				document.listeners.fullscreenchange();
			},
		},
		async exitFullscreen() {
			calls.fullscreenExits++;
			document.fullscreenElement = null;
			document.listeners.fullscreenchange();
		},
	};
	const window = {
		...makeElement(),
		devicePixelRatio: 2,
		location: { reload() { calls.reloads++; } },
	};
	class Engine {
		static getMissingFeatures(options) {
			calls.features = options;
			return missing;
		}
		constructor(settings) { calls.config = settings; }
		startGame(options) {
			calls.options = options;
			return startGame(options);
		}
	}
	class ResizeObserver {
		constructor(callback) { calls.resize = callback; }
		observe(element) { calls.observed = element; }
	}
	vm.runInNewContext(source, {
		document, window, Engine, ResizeObserver, Error,
		console: { error(...args) { calls.errors.push(args); } },
	});
	return { calls, document, window, element: id => document.getElementById(id) };
}

test("export placeholders and the Google tag are wired", () => {
	for (const placeholder of ["$GODOT_CONFIG", "$GODOT_THREADS_ENABLED", "$GODOT_URL", "$GODOT_HEAD_INCLUDE"]) {
		assert.ok(html.includes(placeholder), placeholder);
	}
	const externalUrls = [...html.matchAll(/(?:src|href)=["'](https?:[^"']+)/g)].map(match => match[1]);
	assert.deepEqual(externalUrls, ["https://www.googletagmanager.com/gtag/js?id=G-TJ1Q6YT9EG"]);
	assert.match(html, /gtag\('config', 'G-TJ1Q6YT9EG'\)/);
	const presets = fs.readFileSync(path.join(__dirname, "../export_presets.cfg"), "utf8");
	assert.match(presets, /html\/custom_html_shell="res:\/\/export_templates\/web_shell.html"/);
});

test("all script element references exist in the actual shell markup", () => {
	const ids = [...html.matchAll(/<[^>]+\bid="([^"]+)"[^>]*>/g)].map(match => match[1]);
	assert.equal(new Set(ids).size, ids.length, "Element IDs must be unique");
	for (const match of source.matchAll(/getElementById\("([^"]+)"\)/g)) {
		assert.ok(ids.includes(match[1]), `Missing shell element: ${match[1]}`);
	}
	assert.equal(harness().document.getElementById("missing-element"), null);
});

test("the minimal loading layout keeps diagnostic messages hidden", () => {
	const h = harness();
	assert.equal(h.element("status-message").hidden, true);
	assert.equal(h.element("browser-notice").hidden, true);
	assert.equal(h.element("retry").hidden, true);
});

test("startup preserves exported configuration and focuses the visible game", async () => {
	const h = harness();
	assert.equal(h.calls.script.src, "renamed-game.js");
	await h.calls.script.onload();
	assert.equal(JSON.stringify(h.calls.config), JSON.stringify(config));
	assert.equal(h.calls.features.threads, false);
	assert.equal(h.calls.options.canvasResizePolicy, 0);
	assert.equal(h.calls.options.canvas, h.element("canvas"));
	assert.equal(h.element("overlay").hidden, true);
	assert.equal(h.element("canvas").attributes["aria-hidden"], "false");
	assert.equal(h.calls.focus, h.element("canvas"));
	assert.equal(h.calls.errors.length, 0);
});

test("the canvas follows its container and device pixel ratio, not window height", () => {
	const h = harness();
	assert.equal(h.element("canvas").width, 2560);
	assert.equal(h.element("canvas").height, 1280);
	h.element("stage").clientWidth = 390;
	h.element("stage").clientHeight = 660;
	h.window.devicePixelRatio = 1;
	h.calls.resize();
	assert.equal(h.element("canvas").width, 390);
	assert.equal(h.element("canvas").height, 660);
	assert.equal(h.calls.observed, h.element("stage"));
});

test("startup reapplies the container size after engine window initialization", async () => {
	const h = harness({ startGame: async options => {
		options.canvas.width = 1920;
		options.canvas.height = 1080;
	} });
	await h.calls.script.onload();
	assert.equal(h.element("canvas").width, 2560);
	assert.equal(h.element("canvas").height, 1280);
	assert.equal(h.document.title, "Defn");
});

test("progress supports unknown totals, clamps percentages, and reports engine startup", async () => {
	const h = harness({ startGame: async options => {
		options.onProgress(12, 0);
		assert.equal(h.element("progress").value, undefined);
		assert.equal(h.element("progress-text").textContent, "Downloading...");
		options.onProgress(25, 100);
		assert.equal(h.element("progress").value, 25);
		options.onProgress(101, 100);
		assert.equal(h.element("progress").value, 100);
		assert.equal(h.element("progress-text").textContent, "Starting engine...");
	} });
	await h.calls.script.onload();
});

test("missing browser features are visible and do not start the engine", async () => {
	const h = harness({ missing: ["WebGL 2"] });
	await h.calls.script.onload();
	assert.equal(h.calls.options, undefined);
	assert.equal(h.element("overlay").dataset.state, "error");
	assert.match(h.element("message").textContent, /WebGL 2/);
	assert.equal(h.element("retry").hidden, false);
	assert.equal(h.element("status-message").hidden, false);
	assert.equal(h.calls.focus, h.element("retry"));
	h.element("retry").listeners.click();
	assert.equal(h.calls.reloads, 1);
});

test("engine download and game startup failures stay visible", async () => {
	const download = harness();
	download.calls.script.onerror();
	assert.match(download.element("message").textContent, /could not be downloaded/);
	assert.equal(download.calls.errors.length, 1);
	const startup = harness({ startGame: async () => { throw new Error("Missing extension.wasm"); } });
	await startup.calls.script.onload();
	assert.match(startup.element("message").textContent, /Missing extension.wasm/);
	assert.equal(startup.element("overlay").hidden, false);
	assert.equal(startup.element("download").hidden, true);
});

test("an asynchronous startup error cannot be hidden by a later successful promise", async () => {
	let resolve;
	const h = harness({ startGame: () => new Promise(done => { resolve = done; }) });
	const start = h.calls.script.onload();
	h.window.listeners.unhandledrejection({ reason: new Error("WASM import failed") });
	resolve();
	await start;
	assert.equal(h.element("overlay").dataset.state, "error");
	assert.equal(h.element("overlay").hidden, false);
});

test("runtime exceptions are reported as text, never HTML", async () => {
	const h = harness();
	await h.calls.script.onload();
	h.window.listeners.error({ message: "<img src=x onerror=alert(1)>" });
	assert.match(h.element("message").textContent, /<img/);
	assert.equal(h.element("overlay").dataset.state, "error");
	assert.equal(h.element("canvas").tabIndex, -1);
});

test("exit and failure callbacks cannot be overwritten by startup completion", async () => {
	for (const code of [0, 1]) {
		const h = harness({ startGame: async options => options.onExit(code) });
		await h.calls.script.onload();
		assert.equal(h.element("overlay").hidden, false);
		assert.equal(h.element("overlay").dataset.state, code ? "error" : "stopped");
		assert.equal(h.element("retry").textContent, code ? "Try again" : "Play again");
		assert.equal(h.element("status-message").hidden, false);
	}
});

test("fullscreen toggles correctly and returns focus to the game", async () => {
	const h = harness();
	await h.calls.script.onload();
	assert.equal(h.element("fullscreen").hidden, false);
	await h.element("fullscreen").listeners.click();
	assert.equal(h.calls.fullscreenRequests, 1);
	assert.equal(h.element("fullscreen").attributes["aria-pressed"], "true");
	assert.equal(h.element("fullscreen").textContent, "Exit fullscreen");
	await h.element("fullscreen").listeners.click();
	assert.equal(h.calls.fullscreenExits, 1);
	assert.equal(h.element("fullscreen").attributes["aria-pressed"], "false");
	assert.equal(h.calls.focus, h.element("canvas"));
});

test("unsupported fullscreen is hidden; rejected requests do not interrupt play", async () => {
	assert.equal(harness({ fullscreenEnabled: false }).element("fullscreen").hidden, true);
	const h = harness();
	await h.calls.script.onload();
	h.document.documentElement.requestFullscreen = async () => { throw new Error("Denied"); };
	await h.element("fullscreen").listeners.click();
	assert.equal(h.element("browser-notice").hidden, false);
	assert.equal(h.element("overlay").hidden, true);
	assert.equal(h.calls.errors.length, 1);
});
