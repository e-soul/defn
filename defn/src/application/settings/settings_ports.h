#ifndef SETTINGS_PORTS_H
#define SETTINGS_PORTS_H

#include "settings_models.h"

#include <optional>
#include <vector>

namespace defn {

class SettingsStore {
  public:
    virtual ~SettingsStore() = default;

    [[nodiscard]] virtual std::optional<SettingsState> load(const SettingsState &defaults) const = 0;
    virtual bool save(const SettingsState &state) = 0;
};

class DisplaySettings {
  public:
    virtual ~DisplaySettings() = default;

    [[nodiscard]] virtual SettingsDisplayState capture() const = 0;
    virtual void apply(const SettingsDisplayState &state) = 0;
};

class AudioSettings {
  public:
    virtual ~AudioSettings() = default;

    [[nodiscard]] virtual std::vector<AudioBusSetting> capture() const = 0;
    virtual void apply(const std::vector<AudioBusSetting> &bus_volumes) = 0;
};

} // namespace defn

#endif