#include "settings_use_case.h"

#include <algorithm>
#include <charconv>

namespace defn {

namespace {

double clamp_volume_percent(double value) { return std::clamp(value, 0.0, 100.0); }

int parse_int_or_zero(std::string_view value) {
    int result = 0;
    std::from_chars(value.data(), value.data() + value.size(), result);
    return result;
}

SettingsDisplayState display_state_from_settings(const SettingsState &state) {
    return {
        .display_mode = state.display_mode,
        .resolution = state.resolution,
        .vsync_enabled = state.vsync_enabled,
    };
}

std::vector<AudioBusSetting>::iterator find_bus_setting(std::vector<AudioBusSetting> &bus_settings, const std::string &bus_name) {
    return std::ranges::find_if(bus_settings, [&](const AudioBusSetting &setting) { return setting.bus_name == bus_name; });
}

std::vector<AudioBusSetting>::const_iterator find_bus_setting(const std::vector<AudioBusSetting> &bus_settings, const std::string &bus_name) {
    return std::ranges::find_if(bus_settings, [&](const AudioBusSetting &setting) { return setting.bus_name == bus_name; });
}

} // namespace

SettingsUseCase::SettingsUseCase(SettingsStore &store, DisplaySettings &display, AudioSettings &audio) : store_(store), display_(display), audio_(audio) {}

SettingsState SettingsUseCase::load_or_default() const {
    const SettingsDisplayState display_state = display_.capture();
    SettingsState defaults{
        .display_mode = display_state.display_mode,
        .resolution = display_state.resolution,
        .vsync_enabled = display_state.vsync_enabled,
        .bus_volumes = audio_.capture(),
    };

    const std::optional<SettingsState> loaded = store_.load(defaults);
    return loaded.value_or(defaults);
}

bool SettingsUseCase::save(const SettingsState &state) const { return store_.save(state); }

void SettingsUseCase::apply(const SettingsState &state) const {
    display_.apply(display_state_from_settings(state));
    audio_.apply(state.bus_volumes);
}

void SettingsUseCase::set_display_mode(SettingsState &state, int display_mode) const {
    state.display_mode = display_mode;
    display_.apply(display_state_from_settings(state));
}

void SettingsUseCase::set_resolution(SettingsState &state, SettingsResolution resolution) const {
    state.resolution = resolution;
    display_.apply(display_state_from_settings(state));
}

void SettingsUseCase::set_vsync(SettingsState &state, bool enabled) const {
    state.vsync_enabled = enabled;
    display_.apply(display_state_from_settings(state));
}

void SettingsUseCase::set_bus_volume_percent(SettingsState &state, const std::string &bus_name, double volume_percent) const {
    const double clamped_volume = clamp_volume_percent(volume_percent);
    auto existing = find_bus_setting(state.bus_volumes, bus_name);
    if (existing == state.bus_volumes.end()) {
        state.bus_volumes.push_back({.bus_name = bus_name, .volume_percent = clamped_volume});
    } else {
        existing->volume_percent = clamped_volume;
    }
    audio_.apply(state.bus_volumes);
}

SettingsResolution SettingsUseCase::parse_resolution_value(const std::string &value, SettingsResolution fallback) {
    const size_t separator = value.find('x');
    if (separator == std::string::npos || value.find('x', separator + 1) != std::string::npos) {
        return fallback;
    }

    return {
        .width = parse_int_or_zero(std::string_view(value).substr(0, separator)),
        .height = parse_int_or_zero(std::string_view(value).substr(separator + 1)),
    };
}

double SettingsUseCase::get_bus_volume_percent(const SettingsState &state, const std::string &bus_name, double fallback) {
    const auto existing = find_bus_setting(state.bus_volumes, bus_name);
    if (existing == state.bus_volumes.end()) {
        return fallback;
    }

    return existing->volume_percent;
}

} // namespace defn