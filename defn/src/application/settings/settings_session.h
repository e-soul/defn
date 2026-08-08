// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef SETTINGS_SESSION_H
#define SETTINGS_SESSION_H

#include "settings_use_case.h"

#include <string>

namespace defn {

class SettingsSession {
  public:
    SettingsSession(SettingsStore &store, DisplaySettings &display, AudioSettings &audio);

    void start();
    [[nodiscard]] bool is_started() const { return started_; }
    [[nodiscard]] const SettingsState &state() const { return state_; }

    [[nodiscard]] bool set_display_mode(int display_mode);
    [[nodiscard]] bool set_resolution(SettingsResolution resolution);
    [[nodiscard]] bool set_vsync(bool enabled);
    [[nodiscard]] bool set_bus_volume_percent(const std::string &bus_name, double volume_percent);

  private:
    SettingsUseCase use_case_;
    SettingsState state_;
    bool started_ = false;
};

} // namespace defn

#endif
