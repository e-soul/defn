#include "progression_stats_screen_view.h"

#include "godot_string.h"
#include "owned_upgrades_panel.h"
#include "progression_stats_presenter.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scroll_container.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/color.hpp>

namespace defn {

namespace {

godot::Ref<godot::StyleBoxFlat> panel_style(const godot::Color &border = godot::Color(0.2, 0.34, 0.48)) {
    godot::Ref<godot::StyleBoxFlat> style;
    style.instantiate();
    style->set_bg_color(godot::Color(0.035, 0.075, 0.13, 0.96));
    style->set_border_width_all(2);
    style->set_border_color(border);
    style->set_corner_radius_all(8);
    style->set_content_margin_all(16.0F);
    return style;
}

godot::Ref<godot::StyleBoxFlat> roster_button_style(const godot::Color &background, const godot::Color &border) {
    godot::Ref<godot::StyleBoxFlat> style;
    style.instantiate();
    style->set_bg_color(background);
    style->set_border_width_all(2);
    style->set_border_color(border);
    style->set_corner_radius_all(8);
    style->set_content_margin_all(8.0F);
    return style;
}

void apply_roster_button_styles(godot::Button &button, bool selected) {
    const godot::Color normal_background = selected ? godot::Color(0.12, 0.11, 0.07, 0.98) : godot::Color(0.035, 0.075, 0.13, 0.96);
    const godot::Color hover_background = selected ? godot::Color(0.16, 0.14, 0.07, 1.0) : godot::Color(0.055, 0.11, 0.18, 0.98);
    const godot::Color pressed_background = selected ? godot::Color(0.19, 0.16, 0.07, 1.0) : godot::Color(0.07, 0.14, 0.22, 1.0);
    const godot::Color border = selected ? godot::Color(1.0, 0.75, 0.2) : godot::Color(0.2, 0.34, 0.48);
    const godot::Color focus_border = selected ? godot::Color(1.0, 0.86, 0.42) : godot::Color(0.42, 0.65, 0.84);

    button.add_theme_stylebox_override("normal", roster_button_style(normal_background, border));
    button.add_theme_stylebox_override("hover", roster_button_style(hover_background, border));
    button.add_theme_stylebox_override("pressed", roster_button_style(pressed_background, border));
    button.add_theme_stylebox_override("focus", roster_button_style(normal_background, focus_border));
    button.add_theme_stylebox_override("disabled", roster_button_style(normal_background, border));
}

godot::String frame_zero_path(const std::string &path_template) {
    godot::String path = to_godot_string(path_template);
    path = path.replace("%03d", "000");
    return path.replace("%d", "0");
}

godot::Ref<godot::Texture2D> load_portrait(const std::string &path_template) {
    if (path_template.empty()) {
        return {};
    }
    return godot::ResourceLoader::get_singleton()->load(frame_zero_path(path_template));
}

godot::Label *make_label(const godot::String &text, int size, const godot::Color &color) {
    auto *label = memnew(godot::Label);
    label->set_text(text);
    label->add_theme_font_size_override("font_size", size);
    label->add_theme_color_override("font_color", color);
    return label;
}

godot::Button *make_action_button(const godot::String &text) {
    auto *button = memnew(godot::Button);
    button->set_text(text);
    button->set_custom_minimum_size({190.0F, 44.0F});
    button->set_focus_mode(godot::Control::FOCUS_ALL);
    return button;
}

} // namespace

void ProgressionStatsScreenView::_bind_methods() {
    godot::ClassDB::bind_method(godot::D_METHOD("select_entity", "entity_id"), &ProgressionStatsScreenView::select_entity);
    godot::ClassDB::bind_method(godot::D_METHOD("show_owned_upgrades"), &ProgressionStatsScreenView::show_owned_upgrades);
    godot::ClassDB::bind_method(godot::D_METHOD("show_dossier"), &ProgressionStatsScreenView::show_dossier);
    godot::ClassDB::bind_method(godot::D_METHOD("go_back"), &ProgressionStatsScreenView::go_back);
}

void ProgressionStatsScreenView::configure(ProgressionOverviewSnapshot snapshot, std::vector<UpgradeCardViewModel> owned_upgrades,
                                           const godot::Callable &back_action) {
    snapshot_ = std::move(snapshot);
    owned_upgrades_ = std::move(owned_upgrades);
    back_action_ = back_action;
    selected_entity_id_ = ProgressionStatsPresenter::default_selection(snapshot_);
    showing_all_upgrades_ = false;
    set_name("ProgressionStatsScreen");
    set_h_size_flags(godot::Control::SIZE_EXPAND_FILL);
    set_v_size_flags(godot::Control::SIZE_EXPAND_FILL);
    add_theme_constant_override("separation", 12);
    rebuild();
}

void ProgressionStatsScreenView::select_entity(const godot::String &entity_id) {
    const std::string candidate = to_std_string(entity_id);
    for (const auto &entity : snapshot_.entities) {
        if (entity.id == candidate && entity.unlocked) {
            selected_entity_id_ = candidate;
            showing_all_upgrades_ = false;
            rebuild();
            return;
        }
    }
}

void ProgressionStatsScreenView::show_owned_upgrades() {
    showing_all_upgrades_ = true;
    rebuild();
}

void ProgressionStatsScreenView::show_dossier() {
    showing_all_upgrades_ = false;
    rebuild();
}

void ProgressionStatsScreenView::go_back() {
    if (back_action_.is_valid()) {
        back_action_.call();
    }
}

void ProgressionStatsScreenView::clear_content() {
    while (get_child_count() > 0) {
        godot::Node *child = get_child(0);
        remove_child(child);
        child->queue_free();
    }
}

void ProgressionStatsScreenView::rebuild() {
    clear_content();
    const ProgressionStatsScreenViewModel model = ProgressionStatsPresenter::present(snapshot_, selected_entity_id_);
    selected_entity_id_ = model.selected_entity_id;

    auto *title = make_label(showing_all_upgrades_ ? "ALL OWNED UPGRADES" : "COMMAND ROSTER", 34, godot::Color(1.0, 0.85, 0.3));
    title->set_horizontal_alignment(godot::HORIZONTAL_ALIGNMENT_CENTER);
    add_child(title);

    if (showing_all_upgrades_) {
        OwnedUpgradesPanel::Options options;
        options.min_size = {880.0F, 430.0F};
        options.layout = OwnedUpgradesPanel::Layout::VerticalGrid;
        options.grid_columns = 4;
        add_child(OwnedUpgradesPanel::build(owned_upgrades_, options));
        auto *actions = memnew(godot::HBoxContainer);
        actions->set_alignment(godot::BoxContainer::ALIGNMENT_CENTER);
        auto *return_button = make_action_button("Return to Commnad Roster");
        return_button->set_name("ReturnToDossierButton");
        return_button->connect("pressed", callable_mp(this, &ProgressionStatsScreenView::show_dossier));
        actions->add_child(return_button);
        auto *back = make_action_button(to_godot_string(model.back_label));
        back->set_name("ProgressionBackButton");
        back->connect("pressed", callable_mp(this, &ProgressionStatsScreenView::go_back));
        actions->add_child(back);
        add_child(actions);
        return;
    }

    auto *selector_scroll = memnew(godot::ScrollContainer);
    selector_scroll->set_name("EntitySelectorScroll");
    selector_scroll->set_horizontal_scroll_mode(godot::ScrollContainer::SCROLL_MODE_AUTO);
    selector_scroll->set_vertical_scroll_mode(godot::ScrollContainer::SCROLL_MODE_DISABLED);
    auto *selector_row = memnew(godot::HBoxContainer);
    selector_row->set_alignment(godot::BoxContainer::ALIGNMENT_CENTER);
    selector_row->add_theme_constant_override("separation", 10);
    selector_scroll->add_child(selector_row);
    for (const auto &selector : model.selectors) {
        auto *button = memnew(godot::Button);
        button->set_name(to_godot_string("Selector_" + selector.id));
        button->set_text(to_godot_string(selector.label));
        button->set_custom_minimum_size({150.0F, 74.0F});
        button->set_focus_mode(godot::Control::FOCUS_ALL);
        button->set_disabled(!selector.unlocked);
        apply_roster_button_styles(*button, selector.selected);
        if (!selector.locked_message.empty()) {
            button->set_tooltip_text(to_godot_string(selector.locked_message));
        }
        if (const auto texture = load_portrait(selector.portrait_path_template); texture.is_valid()) {
            button->set_button_icon(texture);
            button->set_expand_icon(true);
        }
        if (!selector.unlocked) {
            button->set_modulate(godot::Color(0.4, 0.4, 0.45, 0.7));
        }
        if (selector.unlocked) {
            button->connect("pressed", callable_mp(this, &ProgressionStatsScreenView::select_entity).bind(to_godot_string(selector.id)));
        }
        selector_row->add_child(button);
    }
    add_child(selector_scroll);

    auto *dossier = memnew(godot::PanelContainer);
    dossier->set_name("EntityDossier");
    dossier->add_theme_stylebox_override("panel", panel_style());
    dossier->set_custom_minimum_size({880.0F, 330.0F});
    auto *columns = memnew(godot::HBoxContainer);
    columns->add_theme_constant_override("separation", 28);
    dossier->add_child(columns);

    auto *identity = memnew(godot::VBoxContainer);
    identity->set_custom_minimum_size({390.0F, 0.0F});
    if (const auto texture = load_portrait(model.portrait_path_template); texture.is_valid()) {
        auto *portrait = memnew(godot::TextureRect);
        portrait->set_name("EntityPortrait");
        portrait->set_custom_minimum_size({220.0F, 150.0F});
        portrait->set_expand_mode(godot::TextureRect::EXPAND_IGNORE_SIZE);
        portrait->set_stretch_mode(godot::TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
        portrait->set_texture(texture);
        identity->add_child(portrait);
    } else {
        const std::string initial = model.title.empty() ? "?" : model.title.substr(0, 1);
        auto *fallback = make_label(to_godot_string(initial), 72, godot::Color(0.55, 0.68, 0.82));
        fallback->set_name("EntityPortraitFallback");
        fallback->set_custom_minimum_size({220.0F, 150.0F});
        fallback->set_horizontal_alignment(godot::HORIZONTAL_ALIGNMENT_CENTER);
        fallback->set_vertical_alignment(godot::VERTICAL_ALIGNMENT_CENTER);
        identity->add_child(fallback);
    }
    auto *name = make_label(to_godot_string(model.title), 28, godot::Color(1.0, 0.85, 0.3));
    name->set_horizontal_alignment(godot::HORIZONTAL_ALIGNMENT_CENTER);
    identity->add_child(name);
    auto *description =
        make_label(to_godot_string(model.description.empty() ? "No field briefing available." : model.description), 17, godot::Color(0.82, 0.87, 0.94));
    description->set_autowrap_mode(godot::TextServer::AUTOWRAP_WORD_SMART);
    description->set_horizontal_alignment(godot::HORIZONTAL_ALIGNMENT_CENTER);
    identity->add_child(description);
    columns->add_child(identity);

    auto *stats_column = memnew(godot::VBoxContainer);
    stats_column->set_h_size_flags(godot::Control::SIZE_EXPAND_FILL);
    stats_column->add_theme_constant_override("separation", 8);
    for (const auto &stat : model.stats) {
        auto *row = memnew(godot::HBoxContainer);
        auto *label = make_label(to_godot_string(stat.label), 19, godot::Color(0.72, 0.79, 0.88));
        label->set_h_size_flags(godot::Control::SIZE_EXPAND_FILL);
        row->add_child(label);
        auto *value = make_label(to_godot_string(stat.value), 25, godot::Color(0.95, 0.97, 1.0));
        value->set_horizontal_alignment(godot::HORIZONTAL_ALIGNMENT_RIGHT);
        row->add_child(value);
        stats_column->add_child(row);
        if (!stat.detail.empty()) {
            auto *detail = make_label(to_godot_string(stat.detail), 15, godot::Color(0.58, 0.67, 0.78));
            detail->set_horizontal_alignment(godot::HORIZONTAL_ALIGNMENT_RIGHT);
            stats_column->add_child(detail);
        }
    }
    stats_column->add_child(make_label("UPGRADE SOURCES", 17, godot::Color(1.0, 0.78, 0.3)));
    if (model.upgrades.empty()) {
        stats_column->add_child(make_label(to_godot_string(model.empty_upgrade_message), 15, godot::Color(0.65, 0.72, 0.8)));
    } else {
        for (const auto &upgrade : model.upgrades) {
            stats_column->add_child(make_label(to_godot_string(upgrade.emoji + " " + upgrade.label), 16, godot::Color(0.85, 0.9, 0.96)));
        }
    }
    columns->add_child(stats_column);
    add_child(dossier);

    auto *actions = memnew(godot::HBoxContainer);
    actions->set_alignment(godot::BoxContainer::ALIGNMENT_CENTER);
    actions->add_theme_constant_override("separation", 16);
    auto *all_upgrades = make_action_button(to_godot_string(model.all_upgrades_label));
    all_upgrades->set_name("AllOwnedUpgradesButton");
    all_upgrades->connect("pressed", callable_mp(this, &ProgressionStatsScreenView::show_owned_upgrades));
    actions->add_child(all_upgrades);
    auto *back = make_action_button(to_godot_string(model.back_label));
    back->set_name("ProgressionBackButton");
    back->connect("pressed", callable_mp(this, &ProgressionStatsScreenView::go_back));
    actions->add_child(back);
    add_child(actions);
}

} // namespace defn
