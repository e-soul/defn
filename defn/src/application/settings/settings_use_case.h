#ifndef SETTINGS_USE_CASE_H
#define SETTINGS_USE_CASE_H

#include "settings_ports.h"

#include <string>

namespace defn {

class SettingsUseCase {
  public:
    SettingsUseCase(SettingsStore &store, DisplaySettings &display, AudioSettings &audio);

    [[nodiscard]] SettingsState load_or_default() const;
    bool save(const SettingsState &state) const;
    void apply(const SettingsState &state) const;

    void set_display_mode(SettingsState &state, int display_mode) const;
    void set_resolution(SettingsState &state, SettingsResolution resolution) const;
    void set_vsync(SettingsState &state, bool enabled) const;
    void set_bus_volume_percent(SettingsState &state, const std::string &bus_name, double volume_percent) const;

    [[nodiscard]] static SettingsResolution parse_resolution_value(const std::string &value, SettingsResolution fallback = {});
    [[nodiscard]] static double get_bus_volume_percent(const SettingsState &state, const std::string &bus_name, double fallback = 100.0);

  private:
    SettingsStore &store_;
    DisplaySettings &display_;
    AudioSettings &audio_;
};

} // namespace defn

#endif