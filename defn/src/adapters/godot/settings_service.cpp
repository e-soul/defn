#include "settings_service.h"

#include "settings_use_case.h"
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

std::string to_std_string(const String &value) { return value.utf8().get_data(); }

String to_godot_string(const std::string &value) { return {value.c_str()}; }

SettingsResolution to_settings_resolution(const Vector2i &value) { return {.width = value.x, .height = value.y}; }

Vector2i to_vector2i(const SettingsResolution &value) { return {to_int32(value.width), to_int32(value.height)}; }

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

class ConfigFileSettingsStore : public SettingsStore {
  public:
    explicit ConfigFileSettingsStore(String path) : path_(std::move(path)) {}

    [[nodiscard]] std::optional<SettingsState> load(const SettingsState &defaults) const override {
        SettingsState state = defaults;

        Ref<ConfigFile> cfg;
        cfg.instantiate();
        if (cfg->load(path_) != OK) {
            return std::nullopt;
        }

        if (cfg->has_section_key("video", "display_mode")) {
            state.display_mode = VariantTools::as_int(cfg->get_value("video", "display_mode"));
        }

        if (cfg->has_section_key("video", "resolution")) {
            state.resolution = SettingsUseCase::parse_resolution_value(to_std_string(String(cfg->get_value("video", "resolution"))), state.resolution);
        }

        if (cfg->has_section_key("video", "vsync")) {
            state.vsync_enabled = cfg->get_value("video", "vsync");
        }

        for (auto &bus_setting : state.bus_volumes) {
            const String audio_key = make_audio_key(to_godot_string(bus_setting.bus_name));
            if (cfg->has_section_key("audio", audio_key)) {
                bus_setting.volume_percent = clamp_volume_percent(static_cast<double>(cfg->get_value("audio", audio_key)));
            }
        }

        return state;
    }

    bool save(const SettingsState &state) override {
        Ref<ConfigFile> cfg;
        cfg.instantiate();

        cfg->set_value("video", "display_mode", state.display_mode);
        cfg->set_value("video", "resolution", vformat("%dx%d", state.resolution.width, state.resolution.height));
        cfg->set_value("video", "vsync", state.vsync_enabled);

        for (const auto &bus_setting : state.bus_volumes) {
            cfg->set_value("audio", make_audio_key(to_godot_string(bus_setting.bus_name)), clamp_volume_percent(bus_setting.volume_percent));
        }

        const Error err = cfg->save(path_);
        if (err != OK) {
            UtilityFunctions::printerr("SettingsService: Failed to save settings, error: ", err);
            return false;
        }

        return true;
    }

  private:
    String path_;
};

class GodotDisplaySettings : public DisplaySettings {
  public:
    [[nodiscard]] SettingsDisplayState capture() const override {
        SettingsDisplayState state;

        auto *display_server = DisplayServer::get_singleton();
        if (display_server != nullptr) {
            state.display_mode = static_cast<int>(display_server->window_get_mode());
            state.resolution = to_settings_resolution(display_server->window_get_size());
            state.vsync_enabled = display_server->window_get_vsync_mode() != DisplayServer::VSYNC_DISABLED;
        }

        return state;
    }

    void apply(const SettingsDisplayState &state) override {
        auto *display_server = DisplayServer::get_singleton();
        if (display_server == nullptr) {
            return;
        }

        const auto display_mode = static_cast<DisplayServer::WindowMode>(state.display_mode);
        const Vector2i resolution = to_vector2i(state.resolution);
        display_server->window_set_mode(display_mode);
        if (display_mode == DisplayServer::WINDOW_MODE_WINDOWED) {
            display_server->window_set_size(resolution);
            center_window(display_server, resolution);
        }
        display_server->window_set_vsync_mode(state.vsync_enabled ? DisplayServer::VSYNC_ENABLED : DisplayServer::VSYNC_DISABLED);
    }
};

class GodotAudioSettings : public AudioSettings {
  public:
    [[nodiscard]] std::vector<AudioBusSetting> capture() const override {
        std::vector<AudioBusSetting> bus_volumes;

        auto *audio_server = AudioServer::get_singleton();
        if (audio_server != nullptr) {
            const int bus_count = audio_server->get_bus_count();
            for (int bus_index = 0; bus_index < bus_count; ++bus_index) {
                bus_volumes.push_back({
                    .bus_name = to_std_string(String(audio_server->get_bus_name(bus_index))),
                    .volume_percent = decibels_to_percent(audio_server->get_bus_volume_db(bus_index)),
                });
            }
        }

        return bus_volumes;
    }

    void apply(const std::vector<AudioBusSetting> &bus_volumes) override {
        auto *audio_server = AudioServer::get_singleton();
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
};

} // namespace

Vector2i SettingsService::parse_resolution_value(const String &value_str, const Vector2i &fallback) {
    return to_vector2i(SettingsUseCase::parse_resolution_value(to_std_string(value_str), to_settings_resolution(fallback)));
}

SettingsState SettingsService::load_or_default(const String &path) {
    ConfigFileSettingsStore store(path);
    GodotDisplaySettings display;
    GodotAudioSettings audio;
    return SettingsUseCase(store, display, audio).load_or_default();
}

bool SettingsService::save(const SettingsState &state, const String &path) {
    ConfigFileSettingsStore store(path);
    GodotDisplaySettings display;
    GodotAudioSettings audio;
    return SettingsUseCase(store, display, audio).save(state);
}

void SettingsService::apply(const SettingsState &state) {
    ConfigFileSettingsStore store(SETTINGS_PATH);
    GodotDisplaySettings display;
    GodotAudioSettings audio;
    SettingsUseCase(store, display, audio).apply(state);
}

void SettingsService::set_display_mode(SettingsState &state, DisplayServer::WindowMode mode) {
    ConfigFileSettingsStore store(SETTINGS_PATH);
    GodotDisplaySettings display;
    GodotAudioSettings audio;
    SettingsUseCase(store, display, audio).set_display_mode(state, static_cast<int>(mode));
}

void SettingsService::set_resolution(SettingsState &state, const Vector2i &resolution) {
    ConfigFileSettingsStore store(SETTINGS_PATH);
    GodotDisplaySettings display;
    GodotAudioSettings audio;
    SettingsUseCase(store, display, audio).set_resolution(state, to_settings_resolution(resolution));
}

void SettingsService::set_vsync(SettingsState &state, bool enabled) {
    ConfigFileSettingsStore store(SETTINGS_PATH);
    GodotDisplaySettings display;
    GodotAudioSettings audio;
    SettingsUseCase(store, display, audio).set_vsync(state, enabled);
}

void SettingsService::set_bus_volume_percent(SettingsState &state, const String &bus_name, double volume_percent) {
    ConfigFileSettingsStore store(SETTINGS_PATH);
    GodotDisplaySettings display;
    GodotAudioSettings audio;
    SettingsUseCase(store, display, audio).set_bus_volume_percent(state, to_std_string(bus_name), volume_percent);
}

double SettingsService::get_bus_volume_percent(const SettingsState &state, const String &bus_name, double fallback) {
    return SettingsUseCase::get_bus_volume_percent(state, to_std_string(bus_name), fallback);
}

} // namespace defn