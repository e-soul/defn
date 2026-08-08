// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "settings_adapters.h"

#include "godot_string.h"
#include "settings_use_case.h"
#include "variant_tools.h"

#include <algorithm>
#include <cmath>
#include <godot_cpp/classes/audio_server.hpp>
#include <godot_cpp/classes/config_file.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <utility>

namespace defn {

namespace {

SettingsResolution to_settings_resolution(const godot::Vector2i &value) { return {.width = value.x, .height = value.y}; }

godot::Vector2i to_vector2i(const SettingsResolution &value) { return {value.width, value.height}; }

double clamp_volume_percent(double value) { return std::clamp(value, 0.0, 100.0); }

godot::String make_audio_key(const godot::String &bus_name) { return bus_name.to_lower().replace(" ", "_") + "_volume"; }

double decibels_to_percent(float decibels) { return std::round(std::pow(10.0, decibels / 20.0) * 100.0); }

float percent_to_decibels(double volume_percent) {
    const auto linear = static_cast<float>(clamp_volume_percent(volume_percent) / 100.0);
    return linear > 0.001F ? 20.0F * std::log10(linear) : -80.0F;
}

} // namespace

ConfigFileSettingsStore::ConfigFileSettingsStore(godot::String path) : path_(std::move(path)) {}

std::optional<SettingsState> ConfigFileSettingsStore::load(const SettingsState &defaults) const {
    SettingsState state = defaults;

    godot::Ref<godot::ConfigFile> config;
    config.instantiate();
    if (config->load(path_) != godot::OK) {
        return std::nullopt;
    }

    if (config->has_section_key("video", "display_mode")) {
        state.display_mode = VariantTools::as_int(config->get_value("video", "display_mode"));
    }
    if (config->has_section_key("video", "resolution")) {
        state.resolution = SettingsUseCase::parse_resolution_value(to_std_string(godot::String(config->get_value("video", "resolution"))), state.resolution);
    }
    if (config->has_section_key("video", "vsync")) {
        state.vsync_enabled = config->get_value("video", "vsync");
    }

    for (auto &bus_setting : state.bus_volumes) {
        const godot::String audio_key = make_audio_key(to_godot_string(bus_setting.bus_name));
        if (config->has_section_key("audio", audio_key)) {
            bus_setting.volume_percent = clamp_volume_percent(static_cast<double>(config->get_value("audio", audio_key)));
        }
    }

    return state;
}

bool ConfigFileSettingsStore::save(const SettingsState &state) {
    godot::Ref<godot::ConfigFile> config;
    config.instantiate();

    config->set_value("video", "display_mode", state.display_mode);
    config->set_value("video", "resolution", godot::vformat("%dx%d", state.resolution.width, state.resolution.height));
    config->set_value("video", "vsync", state.vsync_enabled);
    for (const auto &bus_setting : state.bus_volumes) {
        config->set_value("audio", make_audio_key(to_godot_string(bus_setting.bus_name)), clamp_volume_percent(bus_setting.volume_percent));
    }

    const godot::Error error = config->save(path_);
    if (error != godot::OK) {
        godot::UtilityFunctions::printerr("ConfigFileSettingsStore: Failed to save settings, error: ", error);
        return false;
    }
    return true;
}

GodotDisplaySettings::GodotDisplaySettings(int32_t window_id) : window_id_(window_id) {}

bool GodotDisplaySettings::is_window_valid() const {
    auto *display_server = godot::DisplayServer::get_singleton();
    const bool valid = display_server != nullptr && window_id_ != godot::DisplayServer::INVALID_WINDOW_ID && display_server->get_window_list().has(window_id_);
    if (!valid) {
        godot::UtilityFunctions::printerr("GodotDisplaySettings: Runtime window is unavailable: ", window_id_);
    }
    return valid;
}

SettingsDisplayState GodotDisplaySettings::capture() const {
    SettingsDisplayState state;
    if (!is_window_valid()) {
        return state;
    }

    auto *display_server = godot::DisplayServer::get_singleton();
    state.display_mode = static_cast<int>(display_server->window_get_mode(window_id_));
    state.resolution = to_settings_resolution(display_server->window_get_size(window_id_));
    state.vsync_enabled = display_server->window_get_vsync_mode(window_id_) != godot::DisplayServer::VSYNC_DISABLED;
    return state;
}

void GodotDisplaySettings::apply(const SettingsDisplayState &state) {
    if (!is_window_valid()) {
        return;
    }

    auto *display_server = godot::DisplayServer::get_singleton();
    const auto display_mode = static_cast<godot::DisplayServer::WindowMode>(state.display_mode);
    const godot::Vector2i resolution = to_vector2i(state.resolution);
    display_server->window_set_mode(display_mode, window_id_);
    if (display_mode == godot::DisplayServer::WINDOW_MODE_WINDOWED) {
        display_server->window_set_size(resolution, window_id_);
        const int32_t screen = display_server->window_get_current_screen(window_id_);
        const godot::Vector2i screen_position = display_server->screen_get_position(screen);
        const godot::Vector2i screen_size = display_server->screen_get_size(screen);
        display_server->window_set_position(screen_position + ((screen_size - resolution) / 2), window_id_);
    }
    display_server->window_set_vsync_mode(state.vsync_enabled ? godot::DisplayServer::VSYNC_ENABLED : godot::DisplayServer::VSYNC_DISABLED, window_id_);
}

NoOpDisplaySettings::NoOpDisplaySettings(SettingsDisplayState captured) : captured_(captured) {}

SettingsDisplayState NoOpDisplaySettings::capture() const { return captured_; }

void NoOpDisplaySettings::apply(const SettingsDisplayState & /*state*/) {}

std::vector<AudioBusSetting> GodotAudioSettings::capture() const {
    std::vector<AudioBusSetting> bus_volumes;
    auto *audio_server = godot::AudioServer::get_singleton();
    if (audio_server == nullptr) {
        return bus_volumes;
    }

    const int bus_count = audio_server->get_bus_count();
    bus_volumes.reserve(static_cast<size_t>(bus_count));
    for (int bus_index = 0; bus_index < bus_count; ++bus_index) {
        bus_volumes.push_back({
            .bus_name = to_std_string(godot::String(audio_server->get_bus_name(bus_index))),
            .volume_percent = decibels_to_percent(audio_server->get_bus_volume_db(bus_index)),
        });
    }
    return bus_volumes;
}

void GodotAudioSettings::apply(const std::vector<AudioBusSetting> &bus_volumes) {
    auto *audio_server = godot::AudioServer::get_singleton();
    if (audio_server == nullptr) {
        return;
    }

    for (const auto &bus_setting : bus_volumes) {
        const int bus_index = audio_server->get_bus_index(to_godot_string(bus_setting.bus_name));
        if (bus_index >= 0) {
            audio_server->set_bus_volume_db(bus_index, percent_to_decibels(bus_setting.volume_percent));
        }
    }
}

} // namespace defn
