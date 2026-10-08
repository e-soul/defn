// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause
#include "responsive_ui_root.h"
#include "score_screen_view.h"
#include "ui_test_helpers.h"
#include "ui_theme_provider.h"
#include <algorithm>
#include <godot_cpp/classes/style_box.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/core/memory.hpp>
#include <string>
#include <vector>

namespace defn {
using namespace ui_test;
namespace {
ScoreScreenModel mobile_score_fixture() {
    ScoreScreenModel model;
    model.victory = true;
    model.hearts_remaining = 3;
    model.hearts_total = 3;
    model.level_score = 328;
    model.new_total_score = 123457117;
    model.next_level_id = "level_02";
    model.new_unlocks.emplace_back("NEW UNLOCK: Level 02!");
    model.reward.title = "FIRST CLEAR UPGRADE: Level 01";
    model.reward.subtitle = "Level 01 cleared for the first time.";
    model.reward.available_upgrades = {{.id = "bulkhead", .name = "Reinforced Bulkhead", .description = "+1 base integrity next mission.", .icon = "integrity"},
                                       {.id = "boots", .name = "Field Boots", .description = "Marksmen gain +10 move speed.", .icon = "speed"},
                                       {.id = "permit", .name = "Demolition Permit", .description = "Unlock the Impact deploy card.", .icon = "recruit"}};
    for (int index = 0; index < 17; ++index) {
        model.owned_upgrades.push_back({.id = "owned_" + std::to_string(index),
                                        .name = "Sharpshooter Contract",
                                        .description = "Unlock the Marksman deploy card.",
                                        .icon = "recruit",
                                        .owned_count = 1000});
    }
    return model;
}

void check_mobile_card_bounds(Button *button) {
    DEFN_CHECK(button->get_size().y >= 48);
    for (int child_index = 0; child_index < button->get_child_count(); ++child_index) {
        check_mobile_control_bounds(Object::cast_to<Control>(button->get_child(child_index)), button->get_size());
    }
}

void check_mobile_content_bounds(ScoreScreenView *view, const godot::Vector2 &size) {
    auto *content = Object::cast_to<Control>(view->find_child("ScoreContent", true, false));
    DEFN_REQUIRE(content != nullptr);
    // The manual content host uses the panel's themed 8-unit inset and 4-unit screen margin.
    const godot::Vector2 available = size - godot::Vector2(24, 24);
    for (int index = 0; index < content->get_child_count(); ++index) {
        auto *control = Object::cast_to<Control>(content->get_child(index));
        if (!control->is_visible()) {
            continue;
        }
        check_mobile_control_bounds(control, available);
        if (auto *button = Object::cast_to<Button>(control); button != nullptr) {
            check_mobile_card_bounds(button);
        }
    }
}
void check_mobile_primary_button(Button *button) {
    const Ref<StyleBoxFlat> primary_frame = button->get_theme_stylebox("normal");
    DEFN_REQUIRE(primary_frame.is_valid());
    DEFN_CHECK(primary_frame->get_bg_color().is_equal_approx(UiThemeProvider::color("accent")));
}
void check_mobile_reward_gate(ScoreScreenView *view, const godot::Vector2 &size) {
    view->layout_in_rect({{}, size});
    check_mobile_content_bounds(view, size);
    DEFN_CHECK(view->find_child("ScreenScroll", true, false) == nullptr);
    DEFN_CHECK(has_all_labels(view, {"VICTORY", "Level Score:", "Career Total:", "328", "123457117"}));
    auto *choose = find_button_by_text(view, "Choose upgrade");
    DEFN_REQUIRE(choose != nullptr);
    DEFN_CHECK(!choose->is_disabled());
    check_mobile_primary_button(choose);
    DEFN_CHECK(find_button_by_text(view, "Retry")->is_disabled());
    choose->emit_signal("pressed");
    view->layout_in_rect({{}, size});
    check_mobile_content_bounds(view, size);
    DEFN_CHECK(has_label_text(view, "FIRST CLEAR UPGRADE: Level 01"));
    DEFN_CHECK(has_label_text(view, "Level 01 cleared for the first time."));
    find_button_by_text(view, "Back")->emit_signal("pressed");
    view->layout_in_rect({{}, size});
    DEFN_CHECK(find_button_by_text(view, "Campaign")->is_disabled());
}
} // namespace

DEFN_TEST(mobile_score_pages_fit_and_keep_reward_gate_after_rotation_and_back) {
    const TreeMountedNode<ScoreScreenView> owner;
    auto *view = owner.get();
    view->configure(mobile_score_fixture(), {});
    for (const godot::Vector2 size : {godot::Vector2(390, 783), godot::Vector2(667, 322), godot::Vector2(864, 230), godot::Vector2(320, 480)}) {
        check_mobile_reward_gate(view, size);
    }
}

DEFN_TEST(mobile_score_reward_paging_keeps_the_viewed_choice_and_forwards_its_id) {
    const TreeMountedNode<ScoreScreenView> owner;
    auto *view = owner.get();
    view->configure(mobile_score_fixture(), {.on_select_upgrade = Callable(view, "set_meta").bind(true)});
    find_button_by_text(view, "Choose upgrade")->emit_signal("pressed");
    const Rect2 short_rect(0, 0, 864, 230);
    view->layout_in_rect(short_rect);
    auto *next = find_button_by_text(view, "Next");
    DEFN_REQUIRE(next != nullptr);
    next->emit_signal("pressed");
    view->layout_in_rect(short_rect);
    DEFN_CHECK(has_label_text(view, "Field Boots"));
    view->layout_in_rect({0, 0, 390, 783});
    DEFN_CHECK(has_all_labels(view, {"Reinforced Bulkhead", "Field Boots", "Demolition Permit"}));
    view->layout_in_rect(short_rect);
    DEFN_CHECK(has_label_text(view, "Field Boots"));
    auto *choice = Object::cast_to<Button>(view->find_child("RewardUpgrade_boots", true, false));
    DEFN_REQUIRE(choice != nullptr);
    choice->emit_signal("pressed");
    DEFN_CHECK(view->has_meta("boots"));
    // Applying a reward is the caller's job; emitting the action cannot fake a successful selection.
    DEFN_CHECK(has_label_text(view, "FIRST CLEAR UPGRADE: Level 01"));
}

DEFN_TEST(mobile_score_owned_collection_pages_preserve_counts_and_clamp_through_reflow) {
    const TreeMountedNode<ScoreScreenView> owner;
    auto *view = owner.get();
    view->configure(mobile_score_fixture(), {});
    auto *owned = Object::cast_to<Button>(view->find_child("ScoreOwnedButton", true, false));
    DEFN_REQUIRE(owned != nullptr);
    owned->emit_signal("pressed");
    view->layout_in_rect({0, 0, 864, 230});
    check_mobile_content_bounds(view, {864, 230});
    DEFN_CHECK(has_label_text(view, "Sharpshooter Contract  x1000"));
    for (int page = 0; page < 20; ++page) {
        auto *next = find_button_by_text(view, "Next");
        DEFN_REQUIRE(next != nullptr);
        if (next->is_disabled()) {
            break;
        }
        next->emit_signal("pressed");
        view->layout_in_rect({0, 0, 864, 230});
    }
    DEFN_CHECK(view->find_child("OwnedUpgrade_owned_16", true, false) != nullptr);
    view->layout_in_rect({0, 0, 390, 783});
    check_mobile_content_bounds(view, {390, 783});
    DEFN_CHECK(view->find_child("OwnedUpgrade_owned_16", true, false) != nullptr);
}

DEFN_TEST(mobile_score_rescue_and_endless_keep_their_original_actions_and_metrics) {
    const TreeMountedNode<ScoreScreenView> rescue_owner;
    auto rescue = mobile_score_fixture();
    rescue.victory = false;
    rescue.reward.title = "RESCUE UPGRADE";
    rescue.next_level_id.clear();
    rescue_owner.get()->configure(rescue, {});
    DEFN_CHECK(find_button_by_text(rescue_owner.get(), "Choose upgrade") != nullptr);
    DEFN_CHECK(find_button_by_text(rescue_owner.get(), "Retry")->is_disabled());
    const TreeMountedNode<ScoreScreenView> run_owner;
    ScoreScreenModel run;
    run.survival_bonus = 425;
    run.level_score = 1325;
    run.endless = {.available = true, .run = true, .wave_reached = 17, .best_wave = 17, .best_score = 1325, .record_wave = true, .record_score = true};
    run_owner.get()->configure(run, {});
    run_owner.get()->layout_in_rect({0, 0, 864, 230});
    check_mobile_content_bounds(run_owner.get(), {864, 230});
    DEFN_CHECK(has_all_labels(run_owner.get(), {"RUN OVER", "Wave Reached:", "Survival Bonus:", "Best Run:", "17  BEST", "1325  BEST"}));
    DEFN_CHECK(find_button_by_text(run_owner.get(), "Retry") != nullptr);
    DEFN_CHECK(find_button_by_text(run_owner.get(), "Endless") == nullptr);
    DEFN_CHECK(find_button_by_text(run_owner.get(), "Next Level") == nullptr);
}

DEFN_TEST(mobile_score_successful_reward_model_enables_all_four_navigation_actions) {
    const TreeMountedNode<ScoreScreenView> owner;
    auto model = mobile_score_fixture();
    model.reward.selected_upgrade = model.reward.available_upgrades.front();
    model.endless.available = true;
    auto *view = owner.get();
    view->configure(model, {});
    view->layout_in_rect({0, 0, 320, 480});
    check_mobile_content_bounds(view, {320, 480});
    DEFN_CHECK(find_button_by_text(view, "Choose upgrade") == nullptr);
    DEFN_CHECK(!find_button_by_text(view, "Next Level")->is_disabled());
    DEFN_CHECK(!find_button_by_text(view, "Endless")->is_disabled());
    DEFN_CHECK(!find_button_by_text(view, "Retry")->is_disabled());
    DEFN_CHECK(!find_button_by_text(view, "Campaign")->is_disabled());
}

namespace {
void check_desktop_score_control(Control *control, const godot::Vector2 &available, float footer_top) {
    DEFN_REQUIRE(control != nullptr);
    DEFN_CHECK(control->get_position().x >= -1);
    DEFN_CHECK(control->get_position().y >= -1);
    check_mobile_control_bounds(control, available);
    auto *button = Object::cast_to<Button>(control);
    if (button != nullptr) {
        DEFN_CHECK(button->get_size().y >= 44);
        if (button->has_meta("upgrade_id")) {
            check_mobile_card_bounds(button);
            DEFN_CHECK(button->get_rect().get_end().y <= footer_top);
        }
    }
}

void check_desktop_score_bounds(ScoreScreenView *view) {
    auto *panel = view->panel();
    const Ref<StyleBox> style = panel->get_theme_stylebox("panel");
    const godot::Vector2 available = panel->get_size() - godot::Vector2(style->get_content_margin(SIDE_LEFT) + style->get_content_margin(SIDE_RIGHT),
                                                                        style->get_content_margin(SIDE_TOP) + style->get_content_margin(SIDE_BOTTOM));
    auto *content = Object::cast_to<Control>(view->find_child("ScoreContent", true, false));
    DEFN_REQUIRE(content != nullptr);
    auto *footer = Object::cast_to<Control>(content->find_child("ScoreActions", true, false));
    DEFN_REQUIRE(footer != nullptr);
    for (int index = 0; index < content->get_child_count(); ++index) {
        auto *control = Object::cast_to<Control>(content->get_child(index));
        if (control->is_visible()) {
            check_desktop_score_control(control, available, footer->get_position().y);
        }
    }
    DEFN_CHECK(view->find_child("ScreenScroll", true, false) == nullptr);
}

void check_desktop_result_page(ScoreScreenView *view, const Rect2 &area) {
    view->layout_in_rect(area);
    check_desktop_score_bounds(view);
    DEFN_CHECK(has_all_labels(view, {"VICTORY", "328", "123457117", "Integrity Remaining:", "Completion Bonus:", "NEW UNLOCK: Level 02!"}));
    DEFN_CHECK(view->panel()->get_size().x <= 860);
    auto *choose = find_button_by_text(view, "Choose upgrade");
    DEFN_REQUIRE(choose != nullptr);
    check_mobile_primary_button(choose);
    DEFN_CHECK(find_button_by_text(view, "Retry")->is_disabled());
    choose->emit_signal("pressed");
}

void check_desktop_reward_page(ScoreScreenView *view, const Rect2 &area) {
    view->layout_in_rect(area);
    check_desktop_score_bounds(view);
    DEFN_CHECK(has_all_labels(
        view, {"FIRST CLEAR UPGRADE: Level 01", "Level 01 cleared for the first time.", "Reinforced Bulkhead", "Field Boots", "Demolition Permit"}));
    auto *choice = Object::cast_to<Button>(view->find_child("RewardUpgrade_boots", true, false));
    DEFN_REQUIRE(choice != nullptr);
    choice->emit_signal("pressed");
    DEFN_CHECK(view->has_meta("boots"));
    find_button_by_text(view, "Back")->emit_signal("pressed");
    view->layout_in_rect(area);
    DEFN_CHECK(find_button_by_text(view, "Campaign")->is_disabled());
}

void advance_desktop_collection_to_end(ScoreScreenView *view) {
    for (int page = 0; page < 20; ++page) {
        auto *next = find_button_by_text(view, "Next");
        DEFN_REQUIRE(next != nullptr);
        if (next->is_disabled()) {
            return;
        }
        next->emit_signal("pressed");
        view->layout_in_rect({0, 0, 480, 360});
        check_desktop_score_bounds(view);
    }
}
} // namespace

DEFN_TEST(desktop_score_pages_fit_without_scrolling_and_preserve_reward_gate) {
    const TreeMountedNode<ScoreScreenView> owner;
    auto *view = owner.get();
    view->configure(mobile_score_fixture(), {.on_select_upgrade = Callable(view, "set_meta").bind(true)}, ScoreScreenView::Layout::Desktop);
    for (const godot::Vector2 size : {godot::Vector2(1920, 1080), godot::Vector2(1280, 720), godot::Vector2(960, 540), godot::Vector2(640, 480)}) {
        const Rect2 area(7, 11, size.x, size.y);
        check_desktop_result_page(view, area);
        if (size.y == 1080) {
            DEFN_CHECK(view->panel()->get_size().y < 700);
            DEFN_CHECK(view->panel()->get_position().y > 100);
        }
        check_desktop_reward_page(view, area);
    }
}

DEFN_TEST(desktop_score_collection_keeps_full_details_and_item_anchor_after_resize) {
    const TreeMountedNode<ScoreScreenView> owner;
    auto *view = owner.get();
    view->configure(mobile_score_fixture(), {}, ScoreScreenView::Layout::Desktop);
    find_button_by_text(view, "Your upgrades (17)")->emit_signal("pressed");
    view->layout_in_rect({0, 0, 1920, 1080});
    check_desktop_score_bounds(view);
    DEFN_CHECK(has_all_labels(view, {"YOUR UPGRADES", "Sharpshooter Contract", "Unlock the Marksman deploy card.", "x1000"}));
    DEFN_CHECK(view->find_child("OwnedUpgrade_owned_5", true, false) != nullptr);
    DEFN_CHECK(view->find_child("OwnedUpgrade_owned_6", true, false) == nullptr);
    find_button_by_text(view, "Next")->emit_signal("pressed");
    view->layout_in_rect({0, 0, 1920, 1080});
    DEFN_CHECK(view->find_child("OwnedUpgrade_owned_6", true, false) != nullptr);
    view->layout_in_rect({0, 0, 480, 360});
    check_desktop_score_bounds(view);
    DEFN_CHECK(view->find_child("OwnedUpgrade_owned_6", true, false) != nullptr);
    advance_desktop_collection_to_end(view);
    DEFN_CHECK(view->find_child("OwnedUpgrade_owned_16", true, false) != nullptr);
    view->layout_in_rect({0, 0, 1920, 1080});
    DEFN_CHECK(view->find_child("OwnedUpgrade_owned_16", true, false) != nullptr);
}

DEFN_TEST(desktop_score_reward_completion_and_keyboard_focus_keep_navigation_accessible) {
    const TreeMountedNode<ScoreScreenView> owner;
    auto *view = owner.get();
    auto model = mobile_score_fixture();
    model.reward.selected_upgrade = model.reward.available_upgrades.front();
    model.endless.available = true;
    view->configure(model, {}, ScoreScreenView::Layout::Desktop);
    view->layout_in_rect({0, 0, 1920, 1080});
    DEFN_CHECK(find_button_by_text(view, "Choose upgrade") == nullptr);
    for (const char *text : {"Next Level", "Endless", "Retry", "Campaign"}) {
        auto *button = find_button_by_text(view, text);
        DEFN_REQUIRE(button != nullptr);
        DEFN_CHECK(!button->is_disabled());
    }
    auto *next = find_button_by_text(view, "Next Level");
    check_mobile_primary_button(next);
    next->grab_focus();
    view->layout_in_rect({0, 0, 320, 480});
    check_desktop_score_bounds(view);
    DEFN_CHECK(find_button_by_text(view, "Next Level")->has_focus());
}

DEFN_TEST(score_context_refresh_uses_translated_local_bounds_and_pointer_only_changes) {
    const TreeMountedNode<Control> parent;
    parent.get()->set_position({120, 70});
    auto *root = memnew(ResponsiveUiRoot);
    auto *window = scene_root();
    const auto previous_scale = window->get_content_scale_size();
    const auto restore = [previous_scale](Window *target) { target->set_content_scale_size(previous_scale); };
    const std::unique_ptr<Window, decltype(restore)> restore_scale(window, restore);
    parent.get()->add_child(root);
    DisplaySnapshot display{
        .content = {.width = 800, .height = 600}, .safe_area = {.left = 20, .top = 30, .right = 40, .bottom = 50}, .touch = true, .fine_pointer = true};
    root->apply_snapshot(display);
    const auto nodes = ScoreScreenView::show(root, {}, {}, root->ui_context());
    auto *view = Object::cast_to<ScoreScreenView>(nodes.overlay);
    view->set_size(root->get_size());
    view->layout_in_rect({{}, root->get_size()});
    const auto panel = nodes.panel->get_global_rect();
    const auto bounds = root->get_global_rect();
    DEFN_CHECK_EQ(root->get_size(), godot::Vector2(740, 520));
    DEFN_CHECK(panel.position.x >= bounds.position.x);
    DEFN_CHECK(panel.position.y >= bounds.position.y);
    DEFN_CHECK(panel.get_end().x <= bounds.get_end().x + 1);
    DEFN_CHECK(panel.get_end().y <= bounds.get_end().y + 1);
    DEFN_CHECK(!view->ui_context().mobile_score);
    display.primary_coarse_pointer = true;
    root->apply_snapshot(display);
    DEFN_CHECK(view->ui_context().mobile_score);
    DEFN_CHECK_EQ(root->get_size(), godot::Vector2(740, 520));
    view->_process(0);
    DEFN_CHECK_EQ(view->get_name(), StringName("MobileScoreScreen"));
}
} // namespace defn
