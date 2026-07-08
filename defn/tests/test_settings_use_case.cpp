#include "test_harness.h"

#include "settings_use_case.h"

namespace defn {

namespace {

class FakeSettingsStore : public SettingsStore {
  public:
    std::optional<SettingsState> loaded_state;
    std::optional<SettingsState> saved_state;
    bool save_result = true;

    [[nodiscard]] std::optional<SettingsState> load(const SettingsState & /*defaults*/) const override { return loaded_state; }

    bool save(const SettingsState &state) override {
        saved_state = state;
        return save_result;
    }
};

class FakeDisplaySettings : public DisplaySettings {
  public:
    SettingsDisplayState captured{.display_mode = 1, .resolution = {.width = 1280, .height = 720}, .vsync_enabled = false};
    std::optional<SettingsDisplayState> applied;

    [[nodiscard]] SettingsDisplayState capture() const override { return captured; }
    void apply(const SettingsDisplayState &state) override { applied = state; }
};

class FakeAudioSettings : public AudioSettings {
  public:
    std::vector<AudioBusSetting> captured{{.bus_name = "Master", .volume_percent = 80.0}};
    std::vector<AudioBusSetting> applied;

    [[nodiscard]] std::vector<AudioBusSetting> capture() const override { return captured; }
    void apply(const std::vector<AudioBusSetting> &bus_volumes) override { applied = bus_volumes; }
};

} // namespace

DEFN_TEST(settings_use_case_loads_store_state_or_captured_defaults) {
    FakeSettingsStore store;
    FakeDisplaySettings display;
    FakeAudioSettings audio;
    SettingsUseCase use_case(store, display, audio);

    SettingsState defaults = use_case.load_or_default();
    DEFN_CHECK_EQ(defaults.display_mode, 1);
    DEFN_CHECK_EQ(defaults.resolution.width, 1280);
    DEFN_CHECK_EQ(defaults.resolution.height, 720);
    DEFN_CHECK(!defaults.vsync_enabled);
    DEFN_REQUIRE(defaults.bus_volumes.size() == static_cast<size_t>(1));
    DEFN_CHECK_EQ(defaults.bus_volumes[0].bus_name, std::string("Master"));

    store.loaded_state = SettingsState{.display_mode = 2, .resolution = {.width = 1920, .height = 1080}, .vsync_enabled = true};
    const SettingsState loaded = use_case.load_or_default();
    DEFN_CHECK_EQ(loaded.display_mode, 2);
    DEFN_CHECK_EQ(loaded.resolution.width, 1920);
    DEFN_CHECK(loaded.vsync_enabled);
}

DEFN_TEST(settings_use_case_mutates_applies_and_saves_settings) {
    FakeSettingsStore store;
    FakeDisplaySettings display;
    FakeAudioSettings audio;
    SettingsUseCase use_case(store, display, audio);
    SettingsState state;

    use_case.set_display_mode(state, 3);
    DEFN_REQUIRE(display.applied.has_value());
    DEFN_CHECK_EQ(display.applied->display_mode, 3);

    use_case.set_resolution(state, {.width = 1024, .height = 576});
    DEFN_CHECK_EQ(display.applied->resolution.width, 1024);
    DEFN_CHECK_EQ(display.applied->resolution.height, 576);

    use_case.set_vsync(state, false);
    DEFN_CHECK(!display.applied->vsync_enabled);

    use_case.set_bus_volume_percent(state, "Master", 250.0);
    DEFN_CHECK_EQ(SettingsUseCase::get_bus_volume_percent(state, "Master"), 100.0);
    DEFN_REQUIRE(audio.applied.size() == static_cast<size_t>(1));
    DEFN_CHECK_EQ(audio.applied[0].volume_percent, 100.0);

    use_case.set_bus_volume_percent(state, "Master", -10.0);
    DEFN_CHECK_EQ(SettingsUseCase::get_bus_volume_percent(state, "Master"), 0.0);
    DEFN_CHECK(use_case.save(state));
    DEFN_REQUIRE(store.saved_state.has_value());
    DEFN_CHECK_EQ(store.saved_state->display_mode, state.display_mode);
}

DEFN_TEST(settings_use_case_parses_resolution_values_with_fallbacks) {
    constexpr SettingsResolution hd{.width = 1280, .height = 720};
    constexpr SettingsResolution fallback{.width = 800, .height = 600};
    constexpr SettingsResolution ignored_fallback{.width = 640, .height = 360};
    constexpr SettingsResolution zero{.width = 0, .height = 0};

    DEFN_CHECK(SettingsUseCase::parse_resolution_value("1280x720") == hd);
    DEFN_CHECK(SettingsUseCase::parse_resolution_value("1920", fallback) == fallback);
    DEFN_CHECK(SettingsUseCase::parse_resolution_value("badxvalue", ignored_fallback) == zero);
}

} // namespace defn