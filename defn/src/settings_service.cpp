#include "settings_service.h"

#include "variant_tools.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <godot_cpp/classes/audio_server.hpp>
#include <godot_cpp/classes/config_file.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

namespace {

int32_t to_int32(int64_t value) { return static_cast<int32_t>(value); }

double clamp_volume_percent(double value) { return std::clamp(value, 0.0, 100.0); }

String make_audio_key(const String &bus_name) { return bus_name.to_lower().replace(" ", "_") + "_volume"; }

double decibels_to_percent(float decibels) { return std::round(std::pow(10.0, decibels / 20.0) * 100.0); }

float percent_to_decibels(double volume_percent) {
    const auto linear = static_cast<float>(clamp_volume_percent(volume_percent) / 100.0);
    return linear > 0.001F ? 20.0F * std::log10(linear) : -80.0F;
}

void center_window(DisplayServer *display_server, const Vector2i &window_size) {
    if (display_server == nullptr) {
        return;
    }

    const Vector2i screen_size = display_server->screen_get_size();
    display_server->window_set_position((screen_size - window_size) / 2);
}

std::vector<AudioBusSetting>::iterator find_bus_setting(std::vector<AudioBusSetting> &bus_settings, const String &bus_name) {
    return std::ranges::find_if(bus_settings, [&](const AudioBusSetting &setting) { return setting.bus_name == bus_name; });
}

std::vector<AudioBusSetting>::const_iterator find_bus_setting(const std::vector<AudioBusSetting> &bus_settings, const String &bus_name) {
    return std::ranges::find_if(bus_settings, [&](const AudioBusSetting &setting) { return setting.bus_name == bus_name; });
}

} // namespace

SettingsState SettingsService::capture_current_state() {
    SettingsState state;

    auto *display_server = DisplayServer::get_singleton();
    if (display_server != nullptr) {
        state.display_mode = display_server->window_get_mode();
        state.resolution = display_server->window_get_size();
        state.vsync_enabled = display_server->window_get_vsync_mode() != DisplayServer::VSYNC_DISABLED;
    }

    auto *audio_server = AudioServer::get_singleton();
    if (audio_server != nullptr) {
        const int bus_count = audio_server->get_bus_count();
        for (int bus_index = 0; bus_index < bus_count; ++bus_index) {
            state.bus_volumes.push_back({
                .bus_name = String(audio_server->get_bus_name(bus_index)),
                .volume_percent = decibels_to_percent(audio_server->get_bus_volume_db(bus_index)),
            });
        }
    }

    return state;
}

Vector2i SettingsService::parse_resolution_value(const String &value_str, const Vector2i &fallback) {
    const PackedStringArray parts = value_str.split("x");
    if (parts.size() != 2) {
        return fallback;
    }

    return {to_int32(parts[0].to_int()), to_int32(parts[1].to_int())};
}

SettingsState SettingsService::load_or_default(const String &path) {
    SettingsState state = capture_current_state();

    Ref<ConfigFile> cfg;
    cfg.instantiate();
    if (cfg->load(path) != OK) {
        return state;
    }

    if (cfg->has_section_key("video", "display_mode")) {
        state.display_mode = static_cast<DisplayServer::WindowMode>(VariantTools::as_int(cfg->get_value("video", "display_mode")));
    }

    if (cfg->has_section_key("video", "resolution")) {
        state.resolution = parse_resolution_value(String(cfg->get_value("video", "resolution")), state.resolution);
    }

    if (cfg->has_section_key("video", "vsync")) {
        state.vsync_enabled = cfg->get_value("video", "vsync");
    }

    for (auto &bus_setting : state.bus_volumes) {
        const String audio_key = make_audio_key(bus_setting.bus_name);
        if (cfg->has_section_key("audio", audio_key)) {
            bus_setting.volume_percent = clamp_volume_percent(static_cast<double>(cfg->get_value("audio", audio_key)));
        }
    }

    return state;
}

bool SettingsService::save(const SettingsState &state, const String &path) {
    Ref<ConfigFile> cfg;
    cfg.instantiate();

    cfg->set_value("video", "display_mode", static_cast<int>(state.display_mode));
    cfg->set_value("video", "resolution", vformat("%dx%d", state.resolution.x, state.resolution.y));
    cfg->set_value("video", "vsync", state.vsync_enabled);

    for (const auto &bus_setting : state.bus_volumes) {
        cfg->set_value("audio", make_audio_key(bus_setting.bus_name), clamp_volume_percent(bus_setting.volume_percent));
    }

    const Error err = cfg->save(path);
    if (err != OK) {
        UtilityFunctions::printerr("SettingsService: Failed to save settings, error: ", err);
        return false;
    }

    return true;
}

void SettingsService::apply(const SettingsState &state) {
    auto *display_server = DisplayServer::get_singleton();
    if (display_server != nullptr) {
        display_server->window_set_mode(state.display_mode);
        if (state.display_mode == DisplayServer::WINDOW_MODE_WINDOWED) {
            display_server->window_set_size(state.resolution);
            center_window(display_server, state.resolution);
        }
        display_server->window_set_vsync_mode(state.vsync_enabled ? DisplayServer::VSYNC_ENABLED : DisplayServer::VSYNC_DISABLED);
    }

    auto *audio_server = AudioServer::get_singleton();
    if (audio_server != nullptr) {
        for (const auto &bus_setting : state.bus_volumes) {
            const int bus_index = audio_server->get_bus_index(bus_setting.bus_name);
            if (bus_index >= 0) {
                audio_server->set_bus_volume_db(bus_index, percent_to_decibels(bus_setting.volume_percent));
            }
        }
    }
}

void SettingsService::set_display_mode(SettingsState &state, DisplayServer::WindowMode mode) {
    state.display_mode = mode;

    auto *display_server = DisplayServer::get_singleton();
    if (display_server == nullptr) {
        return;
    }

    display_server->window_set_mode(mode);
    if (mode == DisplayServer::WINDOW_MODE_WINDOWED) {
        display_server->window_set_size(state.resolution);
        center_window(display_server, state.resolution);
    }
}

void SettingsService::set_resolution(SettingsState &state, const Vector2i &resolution) {
    state.resolution = resolution;

    auto *display_server = DisplayServer::get_singleton();
    if (display_server == nullptr || display_server->window_get_mode() != DisplayServer::WINDOW_MODE_WINDOWED) {
        return;
    }

    display_server->window_set_size(resolution);
    center_window(display_server, resolution);
}

void SettingsService::set_vsync(SettingsState &state, bool enabled) {
    state.vsync_enabled = enabled;

    auto *display_server = DisplayServer::get_singleton();
    if (display_server != nullptr) {
        display_server->window_set_vsync_mode(enabled ? DisplayServer::VSYNC_ENABLED : DisplayServer::VSYNC_DISABLED);
    }
}

void SettingsService::set_bus_volume_percent(SettingsState &state, const String &bus_name, double volume_percent) {
    const double clamped_volume = clamp_volume_percent(volume_percent);
    auto existing = find_bus_setting(state.bus_volumes, bus_name);
    if (existing == state.bus_volumes.end()) {
        state.bus_volumes.push_back({
            .bus_name = bus_name,
            .volume_percent = clamped_volume,
        });
    } else {
        existing->volume_percent = clamped_volume;
    }

    auto *audio_server = AudioServer::get_singleton();
    if (audio_server != nullptr) {
        const int bus_index = audio_server->get_bus_index(bus_name);
        if (bus_index >= 0) {
            audio_server->set_bus_volume_db(bus_index, percent_to_decibels(clamped_volume));
        }
    }
}

double SettingsService::get_bus_volume_percent(const SettingsState &state, const String &bus_name, double fallback) {
    const auto existing = find_bus_setting(state.bus_volumes, bus_name);
    if (existing == state.bus_volumes.end()) {
        return fallback;
    }

    return existing->volume_percent;
}

} // namespace defn