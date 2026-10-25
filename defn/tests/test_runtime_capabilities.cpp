// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause
#include "runtime_capabilities.h"
#include "test_harness.h"

namespace {
using namespace defn;
DEFN_TEST(runtime_capabilities_leave_the_editor_untouched) {
    // The editor is not the game, whatever else is true of the process hosting it.
    for (const RuntimeHostFacts host : {RuntimeHostFacts{.editor = true}, RuntimeHostFacts{.editor = true, .embedded = true},
                                        RuntimeHostFacts{.editor = true, .headless = true}, RuntimeHostFacts{.editor = true, .web = true}}) {
        DEFN_CHECK(resolve_runtime_capabilities(host) == RuntimeCapabilities{});
    }
}
DEFN_TEST(runtime_capabilities_grant_a_standalone_game_its_window) {
    DEFN_CHECK(resolve_runtime_capabilities({}) == (RuntimeCapabilities{.game_ui = true, .user_settings = true, .window_settings = true}));
}
DEFN_TEST(runtime_capabilities_scale_an_embedded_or_web_canvas_without_owning_its_window) {
    const RuntimeCapabilities expected{.game_ui = true, .user_settings = true, .window_settings = false};
    DEFN_CHECK(resolve_runtime_capabilities({.embedded = true}) == expected);
    DEFN_CHECK(resolve_runtime_capabilities({.web = true}) == expected);
}
DEFN_TEST(runtime_capabilities_lay_out_headless_runs_without_touching_settings) {
    DEFN_CHECK(resolve_runtime_capabilities({.headless = true}) == (RuntimeCapabilities{.game_ui = true, .user_settings = false, .window_settings = false}));
}
} // namespace
