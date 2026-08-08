// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "settings_session.h"

namespace defn {

SettingsSession::SettingsSession(SettingsStore &store, DisplaySettings &display, AudioSettings &audio) : use_case_(store, display, audio) {}

void SettingsSession::start() {
    if (started_) {
        return;
    }

    state_ = use_case_.load_or_default();
    use_case_.apply(state_);
    started_ = true;
}

bool SettingsSession::set_display_mode(int display_mode) {
    use_case_.set_display_mode(state_, display_mode);
    return use_case_.save(state_);
}

bool SettingsSession::set_resolution(SettingsResolution resolution) {
    use_case_.set_resolution(state_, resolution);
    return use_case_.save(state_);
}

bool SettingsSession::set_vsync(bool enabled) {
    use_case_.set_vsync(state_, enabled);
    return use_case_.save(state_);
}

bool SettingsSession::set_bus_volume_percent(const std::string &bus_name, double volume_percent) {
    use_case_.set_bus_volume_percent(state_, bus_name, volume_percent);
    return use_case_.save(state_);
}

} // namespace defn
