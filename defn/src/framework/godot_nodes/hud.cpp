#include "hud.h"
#include "deploy_card_presenter.h"
#include "godot_string.h"
#include "score_screen_view.h"
#include <godot_cpp/classes/box_container.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <cctype>
#include <utility>

namespace defn {

namespace {

DeployCardPresentationInput to_deploy_card_input(const UnitConfig &config) {
    DeployCardPresentationInput input;
    input.unit_id = config.name;
    input.title = config.name;
    if (!input.title.empty()) {
        input.title[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(input.title[0])));
    }
    input.cost = config.cost;
    input.animation_path_templates.reserve(config.animations.size());
    for (const auto &[name, animation] : config.animations) {
        input.animation_path_templates.emplace_back(name, animation.path_template);
    }
    return input;
}

} // namespace

HUD::HUD() = default;

void HUD::_bind_methods() {
    ADD_SIGNAL(MethodInfo("deploy_requested", PropertyInfo(Variant::STRING, "unit_type")));
    ADD_SIGNAL(MethodInfo("score_screen_next_level", PropertyInfo(Variant::STRING, "level_id")));
    ADD_SIGNAL(MethodInfo("score_screen_retry", PropertyInfo(Variant::STRING, "level_id")));
    ADD_SIGNAL(MethodInfo("score_screen_main_menu"));
    ADD_SIGNAL(MethodInfo("score_screen_upgrade_selected", PropertyInfo(Variant::STRING, "upgrade_id")));
}

void HUD::_ready() { build_ui(); }

void HUD::build_ui() {
    // ==========================================================
    // Top bar
    // ==========================================================
    auto *top_bar = memnew(HBoxContainer);
    top_bar->set_anchors_preset(Control::PRESET_TOP_WIDE);
    top_bar->set_offset(Side::SIDE_LEFT, 16.0);
    top_bar->set_offset(Side::SIDE_RIGHT, -16.0);
    top_bar->set_offset(Side::SIDE_TOP, 8.0);
    top_bar->set_offset(Side::SIDE_BOTTOM, 56.0);
    top_bar->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    add_child(top_bar);

    // Energy label (left)
    core_resource_label = memnew(Label);
    core_resource_label->set_text(String::utf8("\u26A1 Energy: 100"));
    core_resource_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    core_resource_label->add_theme_font_size_override("font_size", 28);
    core_resource_label->add_theme_color_override("font_color", Color(0.05, 0.2, 0.55));
    top_bar->add_child(core_resource_label);

    // Score label
    score_label = memnew(Label);
    score_label->set_text("Score: 0");
    score_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    score_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    score_label->add_theme_font_size_override("font_size", 24);
    score_label->add_theme_color_override("font_color", Color(1.0, 0.85, 0.3));
    top_bar->add_child(score_label);

    // Wave label (center)
    wave_label = memnew(Label);
    wave_label->set_text("WAVE 1 / 3");
    wave_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    wave_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    wave_label->add_theme_font_size_override("font_size", 28);
    wave_label->add_theme_color_override("font_color", Color(1, 1, 1));
    top_bar->add_child(wave_label);

    // Hearts container (right)
    hearts_container = memnew(HBoxContainer);
    hearts_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    hearts_container->set_alignment(BoxContainer::ALIGNMENT_END);
    top_bar->add_child(hearts_container);
    ensure_heart_icons(3);

    level_label = memnew(Label);
    level_label->set_anchors_preset(Control::PRESET_TOP_WIDE);
    level_label->set_offset(Side::SIDE_LEFT, 16.0);
    level_label->set_offset(Side::SIDE_RIGHT, -16.0);
    level_label->set_offset(Side::SIDE_TOP, 52.0);
    level_label->set_offset(Side::SIDE_BOTTOM, 78.0);
    level_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    level_label->add_theme_font_size_override("font_size", 20);
    level_label->add_theme_color_override("font_color", Color(0.95, 0.95, 0.95));
    level_label->set_text("LEVEL");
    add_child(level_label);

    // ==========================================================
    // Deploy card container (bottom center)
    // ==========================================================
    card_container = memnew(HBoxContainer);
    card_container->set_anchor(SIDE_LEFT, 0.5);
    card_container->set_anchor(SIDE_RIGHT, 0.5);
    card_container->set_anchor(SIDE_TOP, 1.0);
    card_container->set_anchor(SIDE_BOTTOM, 1.0);
    card_container->set_h_grow_direction(Control::GROW_DIRECTION_BOTH);
    card_container->set_v_grow_direction(Control::GROW_DIRECTION_BEGIN);
    card_container->set_offset(Side::SIDE_BOTTOM, -10.0);
    card_container->add_theme_constant_override("separation", 12);
    add_child(card_container);

    render(HudPresenter::build(hud_input_));
}

void HUD::set_friendly_units(const std::vector<UnitConfig> &units) {
    hud_input_.deploy_cards.clear();
    hud_input_.deploy_cards.reserve(units.size());
    for (const auto &cfg : units) {
        hud_input_.deploy_cards.push_back(to_deploy_card_input(cfg));
    }

    render(HudPresenter::build(hud_input_));
}

void HUD::set_level(int level_number, const String &level_name) {
    hud_input_.level_number = level_number;
    hud_input_.level_name = level_name.utf8().get_data();
    render(HudPresenter::build(hud_input_));
}

void HUD::render(const HudModel &model) {
    if (core_resource_label != nullptr) {
        core_resource_label->set_text(to_godot_string(model.energy_text));
    }
    if (score_label != nullptr) {
        score_label->set_text(to_godot_string(model.score_text));
    }
    if (wave_label != nullptr) {
        wave_label->set_text(to_godot_string(model.wave_text));
    }
    if (level_label != nullptr) {
        level_label->set_text(to_godot_string(model.level_text));
        level_label->set_visible(model.level_visible);
    }

    ensure_heart_icons(model.visible_hearts);
    for (int i = 0; std::cmp_less(i, heart_icons.size()); ++i) {
        heart_icons[i]->set_visible(i < model.visible_hearts);
    }

    render_deploy_cards(model.deploy_cards);
}

void HUD::render_deploy_cards(const std::vector<HudDeployCardModel> &cards) {
    if (card_container == nullptr) {
        return;
    }

    bool needs_rebuild = deploy_cards.size() != cards.size();
    for (size_t index = 0; !needs_rebuild && index < cards.size(); ++index) {
        needs_rebuild = deploy_cards[index].unit_type != cards[index].card.unit_id;
    }

    if (needs_rebuild) {
        clear_deploy_cards();
        deploy_cards.reserve(cards.size());
        for (const auto &card_model : cards) {
            auto *button = DeployCardPresenter::create(card_model.card,
                                                       callable_mp(this, &HUD::on_card_pressed).bind(to_godot_string(card_model.card.unit_id)));
            card_container->add_child(button);
            deploy_cards.push_back({.unit_type = card_model.card.unit_id, .button = button});
        }
    }

    for (size_t index = 0; index < cards.size(); ++index) {
        Button *button = deploy_cards[index].button;
        if (button == nullptr) {
            continue;
        }
        const bool enabled = cards[index].enabled;
        button->set_disabled(!enabled);
        button->set_modulate(enabled ? Color(1, 1, 1, 1) : Color(0.5, 0.5, 0.5, 0.7));
    }
}

void HUD::clear_deploy_cards() {
    if (card_container != nullptr) {
        while (card_container->get_child_count() > 0) {
            Node *child = card_container->get_child(0);
            card_container->remove_child(child);
            child->queue_free();
        }
    }
    deploy_cards.clear();
}

void HUD::ensure_heart_icons(int count) {
    if (hearts_container == nullptr) {
        return;
    }

    while (std::cmp_less(heart_icons.size(), count)) {
        auto *heart = memnew(Label);
        heart->set_text(String::utf8("\u2665"));
        heart->add_theme_font_size_override("font_size", 32);
        heart->add_theme_color_override("font_color", Color(0.9, 0.15, 0.15));
        hearts_container->add_child(heart);
        heart_icons.push_back(heart);
    }
}

void HUD::on_card_pressed(const String &unit_type) { emit_signal("deploy_requested", unit_type); }

void HUD::update_core_resource(int value) {
    hud_input_.energy = value;
    render(HudPresenter::build(hud_input_));
}

void HUD::update_wave(int current, int total) {
    hud_input_.current_wave = current;
    hud_input_.total_waves = total;
    render(HudPresenter::build(hud_input_));
}

void HUD::update_hearts(int integrity) {
    hud_input_.hearts = integrity;
    render(HudPresenter::build(hud_input_));
}

void HUD::update_card_affordability(int energy) {
    hud_input_.energy = energy;
    render(HudPresenter::build(hud_input_));
}

void HUD::update_score(int score) {
    hud_input_.score = score;
    render(HudPresenter::build(hud_input_));
}

void HUD::show_score_screen(const ScoreScreenModel &summary) {
    if (score_screen_overlay != nullptr && !score_screen_overlay->is_queued_for_deletion()) {
        score_screen_overlay->queue_free();
    }

    const ScoreScreenViewNodes view = ScoreScreenView::show(
        this, summary,
        {
            .on_next_level = callable_mp(this, &HUD::on_next_level_pressed).bind(to_godot_string(summary.next_level_id)),
            .on_retry = callable_mp(this, &HUD::on_retry_pressed).bind(to_godot_string(summary.current_level_id)),
            .on_main_menu = callable_mp(this, &HUD::on_main_menu_pressed),
            .on_select_upgrade = callable_mp(this, &HUD::on_upgrade_card_pressed),
        });

    score_screen_overlay = view.overlay;
    score_screen_panel = view.panel;
}

void HUD::on_next_level_pressed(const String &level_id) { emit_signal("score_screen_next_level", level_id); }

void HUD::on_retry_pressed(const String &level_id) { emit_signal("score_screen_retry", level_id); }

void HUD::on_main_menu_pressed() { emit_signal("score_screen_main_menu"); }

void HUD::on_upgrade_card_pressed(const String &upgrade_id) { emit_signal("score_screen_upgrade_selected", upgrade_id); }

} // namespace defn
