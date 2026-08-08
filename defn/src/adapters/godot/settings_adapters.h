// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef SETTINGS_ADAPTERS_H
#define SETTINGS_ADAPTERS_H

#include "settings_ports.h"

#include <cstdint>
#include <godot_cpp/variant/string.hpp>

namespace defn {

inline constexpr const char *SETTINGS_PATH = "user://settings.cfg";

class ConfigFileSettingsStore final : public SettingsStore {
  public:
    explicit ConfigFileSettingsStore(godot::String path = SETTINGS_PATH);

    [[nodiscard]] std::optional<SettingsState> load(const SettingsState &defaults) const override;
    bool save(const SettingsState &state) override;

  private:
    godot::String path_;
};

class GodotDisplaySettings final : public DisplaySettings {
  public:
    explicit GodotDisplaySettings(int32_t window_id);

    [[nodiscard]] SettingsDisplayState capture() const override;
    void apply(const SettingsDisplayState &state) override;

  private:
    [[nodiscard]] bool is_window_valid() const;

    int32_t window_id_;
};

class NoOpDisplaySettings final : public DisplaySettings {
  public:
    explicit NoOpDisplaySettings(SettingsDisplayState captured = {});

    [[nodiscard]] SettingsDisplayState capture() const override;
    void apply(const SettingsDisplayState &state) override;

  private:
    SettingsDisplayState captured_;
};

class GodotAudioSettings final : public AudioSettings {
  public:
    [[nodiscard]] std::vector<AudioBusSetting> capture() const override;
    void apply(const std::vector<AudioBusSetting> &bus_volumes) override;
};

} // namespace defn

#endif
