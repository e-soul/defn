// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "settings_runtime.h"

#include "godot_string.h"
#include "settings_adapters.h"
#include "settings_session.h"

#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <utility>

namespace defn {

SettingsRuntime *SettingsRuntime::singleton_ = nullptr;

SettingsRuntime::SettingsRuntime() = default;

SettingsRuntime::~SettingsRuntime() = default;

void SettingsRuntime::_bind_methods() {}

SettingsRuntime *SettingsRuntime::get_singleton() { return singleton_; }

void SettingsRuntime::_enter_tree() {
    if (singleton_ == nullptr) {
        singleton_ = this;
        return;
    }
    if (singleton_ != this) {
        godot::UtilityFunctions::printerr("SettingsRuntime: Duplicate runtime owner ignored");
    }
}

void SettingsRuntime::_ready() {
    if (singleton_ != this) {
        return;
    }

    auto *engine = godot::Engine::get_singleton();
    if (engine == nullptr || engine->is_editor_hint()) {
        return;
    }

    auto *display_server = godot::DisplayServer::get_singleton();
    if (display_server == nullptr || display_server->get_name().to_lower() == "headless") {
        return;
    }

    store_ = std::make_unique<ConfigFileSettingsStore>();
    audio_ = std::make_unique<GodotAudioSettings>();

    if (engine->is_embedded_in_editor()) {
        display_ = std::make_unique<NoOpDisplaySettings>();
    } else {
        godot::Window *root_window = get_tree() != nullptr ? get_tree()->get_root() : nullptr;
        const int32_t window_id = root_window != nullptr ? root_window->get_window_id() : godot::DisplayServer::INVALID_WINDOW_ID;
        if (window_id == godot::DisplayServer::INVALID_WINDOW_ID || !display_server->get_window_list().has(window_id)) {
            godot::UtilityFunctions::printerr("SettingsRuntime: Game root window is unavailable; display settings are disabled");
            display_ = std::make_unique<NoOpDisplaySettings>();
        } else {
            display_ = std::make_unique<GodotDisplaySettings>(window_id);
        }
    }

    session_ = std::make_unique<SettingsSession>(*store_, *display_, *audio_);
    session_->start();
}

void SettingsRuntime::_exit_tree() {
    session_.reset();
    audio_.reset();
    display_.reset();
    store_.reset();
    if (singleton_ == this) {
        singleton_ = nullptr;
    }
}

bool SettingsRuntime::is_available() const { return session_ != nullptr && session_->is_started(); }

SettingsSession *SettingsRuntime::require_session() const {
    if (!is_available()) {
        godot::UtilityFunctions::printerr("SettingsRuntime: Settings session is unavailable");
        return nullptr;
    }
    return session_.get();
}

const SettingsState *SettingsRuntime::get_state() const {
    SettingsSession *session = require_session();
    return session != nullptr ? &session->state() : nullptr;
}

bool SettingsRuntime::set_display_mode(int display_mode) {
    SettingsSession *session = require_session();
    return session != nullptr && session->set_display_mode(display_mode);
}

bool SettingsRuntime::set_resolution(SettingsResolution resolution) {
    SettingsSession *session = require_session();
    return session != nullptr && session->set_resolution(resolution);
}

bool SettingsRuntime::set_vsync(bool enabled) {
    SettingsSession *session = require_session();
    return session != nullptr && session->set_vsync(enabled);
}

bool SettingsRuntime::set_bus_volume_percent(const godot::String &bus_name, double volume_percent) {
    SettingsSession *session = require_session();
    return session != nullptr && session->set_bus_volume_percent(to_std_string(bus_name), volume_percent);
}

} // namespace defn
