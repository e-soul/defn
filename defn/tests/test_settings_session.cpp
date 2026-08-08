// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "test_harness.h"

#include "settings_session.h"

#include <optional>

namespace defn {

namespace {

class SessionSettingsStore : public SettingsStore {
  public:
    std::optional<SettingsState> loaded_state;
    std::optional<SettingsState> saved_state;
    mutable int load_count = 0;
    int save_count = 0;
    bool save_result = true;

    [[nodiscard]] std::optional<SettingsState> load(const SettingsState & /*defaults*/) const override {
        ++load_count;
        return loaded_state;
    }

    bool save(const SettingsState &state) override {
        ++save_count;
        saved_state = state;
        return save_result;
    }
};

class SessionDisplaySettings : public DisplaySettings {
  public:
    SettingsDisplayState captured{.display_mode = 1, .resolution = {.width = 1280, .height = 720}, .vsync_enabled = false};
    std::optional<SettingsDisplayState> applied;
    mutable int capture_count = 0;
    int apply_count = 0;

    [[nodiscard]] SettingsDisplayState capture() const override {
        ++capture_count;
        return captured;
    }

    void apply(const SettingsDisplayState &state) override {
        ++apply_count;
        applied = state;
    }
};

class SessionAudioSettings : public AudioSettings {
  public:
    std::vector<AudioBusSetting> captured{{.bus_name = "Master", .volume_percent = 80.0}};
    std::vector<AudioBusSetting> applied;
    mutable int capture_count = 0;
    int apply_count = 0;

    [[nodiscard]] std::vector<AudioBusSetting> capture() const override {
        ++capture_count;
        return captured;
    }

    void apply(const std::vector<AudioBusSetting> &bus_volumes) override {
        ++apply_count;
        applied = bus_volumes;
    }
};

} // namespace

DEFN_TEST(settings_session_starts_once_with_saved_state) {
    SessionSettingsStore store;
    store.loaded_state = SettingsState{
        .display_mode = 3,
        .resolution = {.width = 1920, .height = 1080},
        .vsync_enabled = true,
        .bus_volumes = {{.bus_name = "Master", .volume_percent = 35.0}},
    };
    SessionDisplaySettings display;
    SessionAudioSettings audio;
    SettingsSession session(store, display, audio);

    session.start();
    session.start();

    DEFN_CHECK(session.is_started());
    DEFN_CHECK_EQ(store.load_count, 1);
    DEFN_CHECK_EQ(display.capture_count, 1);
    DEFN_CHECK_EQ(audio.capture_count, 1);
    DEFN_CHECK_EQ(display.apply_count, 1);
    DEFN_CHECK_EQ(audio.apply_count, 1);
    DEFN_CHECK_EQ(session.state().display_mode, 3);
    DEFN_REQUIRE(display.applied.has_value());
    DEFN_CHECK_EQ(display.applied->resolution.width, 1920);
    DEFN_REQUIRE(audio.applied.size() == static_cast<size_t>(1));
    DEFN_CHECK_EQ(audio.applied[0].volume_percent, 35.0);
}

DEFN_TEST(settings_session_uses_captured_defaults_when_save_is_missing) {
    SessionSettingsStore store;
    SessionDisplaySettings display;
    SessionAudioSettings audio;
    SettingsSession session(store, display, audio);

    session.start();

    DEFN_CHECK_EQ(session.state().display_mode, display.captured.display_mode);
    DEFN_CHECK(session.state().resolution == display.captured.resolution);
    DEFN_CHECK_EQ(session.state().vsync_enabled, display.captured.vsync_enabled);
    DEFN_REQUIRE(session.state().bus_volumes.size() == static_cast<size_t>(1));
    DEFN_CHECK_EQ(session.state().bus_volumes[0].volume_percent, 80.0);
}

DEFN_TEST(settings_session_mutations_apply_relevant_adapter_and_persist) {
    SessionSettingsStore store;
    SessionDisplaySettings display;
    SessionAudioSettings audio;
    SettingsSession session(store, display, audio);
    session.start();
    display.apply_count = 0;
    audio.apply_count = 0;

    DEFN_CHECK(session.set_display_mode(2));
    DEFN_CHECK_EQ(display.apply_count, 1);
    DEFN_CHECK_EQ(audio.apply_count, 0);
    DEFN_CHECK_EQ(store.save_count, 1);

    DEFN_CHECK(session.set_resolution({.width = 1024, .height = 576}));
    DEFN_CHECK_EQ(display.apply_count, 2);
    DEFN_CHECK_EQ(audio.apply_count, 0);
    DEFN_CHECK_EQ(store.save_count, 2);

    DEFN_CHECK(session.set_vsync(true));
    DEFN_CHECK_EQ(display.apply_count, 3);
    DEFN_CHECK_EQ(audio.apply_count, 0);
    DEFN_CHECK_EQ(store.save_count, 3);

    DEFN_CHECK(session.set_bus_volume_percent("Master", 125.0));
    DEFN_CHECK_EQ(display.apply_count, 3);
    DEFN_CHECK_EQ(audio.apply_count, 1);
    DEFN_CHECK_EQ(store.save_count, 4);
    DEFN_CHECK_EQ(SettingsUseCase::get_bus_volume_percent(session.state(), "Master"), 100.0);
    DEFN_REQUIRE(store.saved_state.has_value());
    DEFN_CHECK_EQ(SettingsUseCase::get_bus_volume_percent(*store.saved_state, "Master"), 100.0);
}

DEFN_TEST(settings_session_reports_persistence_failure_without_reverting_state) {
    SessionSettingsStore store;
    store.save_result = false;
    SessionDisplaySettings display;
    SessionAudioSettings audio;
    SettingsSession session(store, display, audio);
    session.start();

    DEFN_CHECK(!session.set_vsync(true));
    DEFN_CHECK(session.state().vsync_enabled);
    DEFN_REQUIRE(display.applied.has_value());
    DEFN_CHECK(display.applied->vsync_enabled);
    DEFN_CHECK_EQ(store.save_count, 1);
}

} // namespace defn
