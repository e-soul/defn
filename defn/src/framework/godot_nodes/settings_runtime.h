// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef SETTINGS_RUNTIME_H
#define SETTINGS_RUNTIME_H

#include "settings_models.h"

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/string.hpp>
#include <memory>

namespace defn {

class AudioSettings;
class DisplaySettings;
class SettingsSession;
class SettingsStore;

class SettingsRuntime : public godot::Node {
    GDCLASS(SettingsRuntime, godot::Node)

  public:
    SettingsRuntime();
    ~SettingsRuntime() override;

    static SettingsRuntime *get_singleton();

    void _enter_tree() override;
    void _ready() override;
    void _exit_tree() override;

    [[nodiscard]] bool is_available() const;
    [[nodiscard]] const SettingsState *get_state() const;
    [[nodiscard]] bool set_display_mode(int display_mode);
    [[nodiscard]] bool set_resolution(SettingsResolution resolution);
    [[nodiscard]] bool set_vsync(bool enabled);
    [[nodiscard]] bool set_bus_volume_percent(const godot::String &bus_name, double volume_percent);

  protected:
    static void _bind_methods();

  private:
    [[nodiscard]] SettingsSession *require_session() const;

    static SettingsRuntime *singleton_;

    std::unique_ptr<SettingsStore> store_;
    std::unique_ptr<DisplaySettings> display_;
    std::unique_ptr<AudioSettings> audio_;
    std::unique_ptr<SettingsSession> session_;
};

} // namespace defn

#endif
