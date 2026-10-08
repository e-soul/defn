// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef HUD_H
#define HUD_H

#include "hud_meters.h"
#include "hud_presenter.h"
#include "icon_medallion.h"
#include "match_result_cutscene_view_model.h"
#include "responsive_layout.h"
#include "score_screen_models.h"
#include "ui_theme_provider.h"
#include "ui_widgets.h"
#include "unit_definition.h"
#include <functional>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/h_flow_container.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <optional>
#include <string_view>
#include <vector>

namespace defn {

using namespace godot;

struct DeployCardUI {
    std::string unit_type;
    Button *button = nullptr;
    std::optional<bool> enabled;
};

/// A numeric readout that reserves room by digit count, and only ever reserves more. Below the floor the plate is
/// simply fixed; above it, crossing a power of ten widens the plate once and it stays there. Either way the bar
/// stops breathing in and out as values climb and fall, and the font is only measured when the floor rises.
struct HudValueLabel {
    Label *label = nullptr;
    int floor_digits = 1;
    int reserved_digits = 0;

    void set_value(const String &text);
};

class DeployTray;
class HUD : public UiContextControl {
    GDCLASS(HUD, UiContextControl)

  public:
    HUD();

    void _ready() override;
    void apply_layout(const MatchLayout &layout);
    void apply_appearance(const UiAppearance &appearance);
    void set_layout_invalidation(std::function<void()> callback) { invalidate_layout_ = std::move(callback); }
    HudLayoutMetrics measure_layout(float width);
    std::size_t roster_size() const { return hud_input_.deploy_cards.size(); }
    void set_pause_action(const Callable &action);

    void set_friendly_units(const std::vector<UnitConfig> &units);
    void set_level(const String &level_name);
    void update_core_resource(int value);
    // The reserve ceiling this match plays under. Fixed for the match, so it is pushed once.
    void set_energy_cap(int energy_cap);
    // The line: how much of the allowance is standing, and what the allowance currently *is*.
    //
    // The allowance is pushed on every refresh rather than once at match start, because in endless it widens with
    // the wave. Sending it once sends the level's ceiling, and the readout then says "7 / 24" while the player is
    // in fact full at seven and every deploy card is refused -- which is how this was shipped and reported.
    void update_supply(int used, int cap, bool energy_ceiling_engaged);
    void update_wave(int current, int total);
    void update_integrity(int health, int max_health);
    void update_score(int score);
    void show_match_result_banner(const MatchResultCutsceneModel &model);
    void hide_match_result_banner();
    void show_score_screen(const ScoreScreenModel &summary);

  protected:
    static void _bind_methods();

  private:
    bool configure_match_controls(const MatchLayout &layout);
    void prepare_metrics();
    void reserve_level_width();
    void invalidate_measurement();
    [[nodiscard]] std::array<Control *, 6> reading_groups() const;
    HudLayoutMetrics measure_single_row() const;
    void reflow_single_row();
    void reflow_groups(HudArrangement arrangement);
    void place_match_controls(const MatchLayout &layout);
    void build_ui();
    PanelContainer *build_plate(const char *name, std::string_view surface, Control::LayoutPreset preset);
    void build_energy_plate();
    void build_info_plate();
    void build_integrity_plate();
    void refresh();
    void render(const HudModel &model);
    void render_supply_state(const HudCapModel &supply);
    void render_integrity(const HudIntegrityModel &integrity);
    void render_deploy_cards(const std::vector<HudDeployCardModel> &cards);
    void clear_deploy_cards();
    void on_card_pressed(const String &unit_type);
    void on_next_level_pressed(const String &level_id);
    void on_retry_pressed(const String &level_id);
    void on_endless_pressed();
    void on_campaign_pressed();
    void on_upgrade_card_pressed(const String &upgrade_id);

    // Energy plate
    HudValueLabel energy_value_label;
    Label *energy_cap_label = nullptr;

    // Info plate
    HBoxContainer *level_group = nullptr;
    Label *level_label = nullptr;
    HudValueLabel wave_current_label;
    Label *wave_total_label = nullptr;
    HBoxContainer *supply_group = nullptr;
    IconMedallionNodes supply_medallion;
    HudValueLabel supply_current_label;
    Label *supply_cap_label = nullptr;
    std::optional<bool> supply_at_cap;
    HudValueLabel score_label;

    // Integrity plate
    IconMedallionNodes integrity_medallion;
    HudIntegrityMeter *integrity_meter = nullptr;
    std::optional<IntegrityTier> integrity_tier;

    Control *match_controls_ = nullptr;
    bool desktop_reference_ = false;
    bool phone_landscape_ = false;
    UiAppearance appearance_;
    std::function<void()> invalidate_layout_;
    bool measure_dirty_ = true;
    bool appearance_ready_ = false;
    float cached_measure_width_ = 0;
    HudLayoutMetrics cached_measurement_;
    DeployTray *tray_ = nullptr;
    PanelContainer *metrics_ = nullptr;
    HFlowContainer *flow_ = nullptr;
    HBoxContainer *single_row_ = nullptr;
    BoxContainer *info_row_ = nullptr;
    HBoxContainer *energy_group_ = nullptr;
    HBoxContainer *integrity_group_ = nullptr;
    HBoxContainer *wave_group_ = nullptr;
    HBoxContainer *score_group_ = nullptr;
    PanelContainer *energy_plate_ = nullptr;
    PanelContainer *info_plate_ = nullptr;
    PanelContainer *integrity_plate_ = nullptr;
    Button *pause_button_ = nullptr;
    float metrics_width_ = 0;
    HudArrangement arrangement_ = HudArrangement::Instruments;
    bool show_pause_ = true;
    bool reflowed_ = false;
    std::vector<DeployCardUI> deploy_cards;
    HudPresentationInput hud_input_{.energy = 100, .current_wave = 1, .total_waves = 3, .base_health = 300, .base_max_health = 300, .score = 0};

    // Score screen
    ColorRect *match_result_overlay = nullptr;
    Label *match_result_label = nullptr;
    Control *score_screen_overlay = nullptr;
    PanelContainer *score_screen_panel = nullptr;
};

} // namespace defn

#endif
