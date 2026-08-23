// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef HUD_H
#define HUD_H

#include "hud_meters.h"
#include "hud_presenter.h"
#include "icon_medallion.h"
#include "match_result_cutscene_view_model.h"
#include "score_screen_models.h"
#include "unit_definition.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
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

class HUD : public CanvasLayer {
    GDCLASS(HUD, CanvasLayer)

  public:
    HUD();

    void _ready() override;

    void set_friendly_units(const std::vector<UnitConfig> &units);
    void set_level(const String &level_name);
    void update_core_resource(int value);
    void update_wave(int current, int total);
    void update_integrity(int health, int max_health);
    void update_score(int score);
    void show_match_result_banner(const MatchResultCutsceneModel &model);
    void hide_match_result_banner();
    void show_score_screen(const ScoreScreenModel &summary);

  protected:
    static void _bind_methods();

  private:
    void build_ui();
    PanelContainer *build_plate(const char *name, std::string_view surface, Control::LayoutPreset preset);
    void build_energy_plate();
    void build_info_plate();
    void build_integrity_plate();
    void refresh();
    void render(const HudModel &model);
    void render_integrity(const HudIntegrityModel &integrity);
    void render_deploy_cards(const std::vector<HudDeployCardModel> &cards);
    void clear_deploy_cards();
    void on_card_pressed(const String &unit_type);
    void on_next_level_pressed(const String &level_id);
    void on_retry_pressed(const String &level_id);
    void on_campaign_pressed();
    void on_upgrade_card_pressed(const String &upgrade_id);

    class UiSfxPlayer *ui_sfx_player_ = nullptr;

    // Energy plate
    HudValueLabel energy_value_label;

    // Info plate
    HBoxContainer *level_group = nullptr;
    Label *level_label = nullptr;
    HudValueLabel wave_current_label;
    Label *wave_total_label = nullptr;
    HudValueLabel score_label;

    // Integrity plate
    IconMedallionNodes integrity_medallion;
    HudIntegrityMeter *integrity_meter = nullptr;
    std::optional<IntegrityTier> integrity_tier;

    HBoxContainer *card_container = nullptr;
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
