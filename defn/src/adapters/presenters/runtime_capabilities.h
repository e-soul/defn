// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause
#ifndef RUNTIME_CAPABILITIES_H
#define RUNTIME_CAPABILITIES_H

namespace defn {

// Where the extension is running, as reported by the Godot host.
struct RuntimeHostFacts {
    // The editor itself loaded the extension: any window or theme reached from a node belongs to the editor.
    bool editor = false;
    // The game runs in the editor's Game dock: its own canvas is the game's, its native window is not.
    bool embedded = false;
    bool headless = false;
    bool web = false;
};

// What game code may change about its host. Settings persistence and responsive layout each read the part they own.
struct RuntimeCapabilities {
    // Responsive layout and canvas scaling of the game's own window.
    bool game_ui = false;
    // A persisted settings session, including audio buses.
    bool user_settings = false;
    // Native window mode, size and vsync.
    bool window_settings = false;
    bool operator==(const RuntimeCapabilities &) const = default;
};

[[nodiscard]] constexpr RuntimeCapabilities resolve_runtime_capabilities(const RuntimeHostFacts &host) {
    if (host.editor) {
        return {};
    }
    return {
        .game_ui = true,
        .user_settings = !host.headless,
        .window_settings = !host.headless && !host.embedded && !host.web,
    };
}

} // namespace defn

#endif
