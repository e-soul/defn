// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "test_harness.h"

#include "campaign_map_view_model.h"

namespace defn {

namespace {

CampaignMapDefinition make_map() {
    return {
        .background = {.path = "res://map.jpg"},
        .missions = {{.level_id = "level_01",
                      .position_normalized = {.x = 0.2F, .y = 0.3F},
                      .tagline = "First.",
                      .threat_id = "low",
                      .preview = {.texture = {.path = "res://preview_01.jpg"}, .focus_x = 0.38F, .focus_y = 0.5F, .node_zoom = 1.0F, .dossier_zoom = 1.1F}},
                     {.level_id = "level_02",
                      .position_normalized = {.x = 0.4F, .y = 0.5F},
                      .tagline = "Second.",
                      .threat_id = "high",
                      .preview = {.texture = {.path = "res://preview_02.jpg"}, .focus_x = 0.5F, .focus_y = 0.5F, .node_zoom = 1.0F, .dossier_zoom = 1.0F}},
                     {.level_id = "level_03",
                      .position_normalized = {.x = 0.6F, .y = 0.7F},
                      .tagline = "Third.",
                      .threat_id = "severe",
                      .preview = {.texture = {.path = "res://preview_03.jpg"}, .focus_x = 0.5F, .focus_y = 0.5F, .node_zoom = 1.0F, .dossier_zoom = 1.0F}}},
    };
}

CampaignMapDefinition make_map_with_beacon() {
    CampaignMapDefinition map = make_map();
    map.endless = CampaignEndlessDefinition{
        .position_normalized = {.x = 0.685F, .y = 0.5F},
        .title = "Standing Engagement",
        .tagline = "Hold the line.",
        .requires_completed = "level_02",
        .preview = {.texture = {.path = "res://preview_endless.jpg"}, .focus_x = 0.38F, .focus_y = 0.5F, .node_zoom = 1.0F, .dossier_zoom = 1.0F},
    };
    return map;
}

LevelDefinition make_level(const std::string &name, double last_spawn) {
    LevelDefinition definition;
    definition.name = name;
    definition.starting_core_resource = 50;
    definition.base_integrity = 3;
    definition.waves = {
        {.wave_number = 1,
         .spawns = {{.time = 2.0, .type = "heavy_grime"}, {.time = last_spawn, .type = "jackal"}, {.time = last_spawn, .type = "heavy_grime"}}}};
    return definition;
}

std::vector<CampaignLevelPresentationSource> make_completed_campaign() {
    return {{.level_id = "level_01", .definition = make_level("Desert", 50.0), .unlocked = true, .completed = true},
            {.level_id = "level_02", .definition = make_level("Jungle", 70.0), .requires_completed = "level_01", .unlocked = true, .completed = true},
            {.level_id = "level_03", .definition = make_level("Coast", 90.0), .requires_completed = "level_02", .unlocked = true, .completed = true}};
}

} // namespace

DEFN_TEST(campaign_map_presenter_hides_the_beacon_until_endless_is_unlocked) {
    const CampaignMapViewModel view_model = CampaignMapPresenter::present(make_map_with_beacon(), make_completed_campaign(), {.unlocked = false});

    // Hidden rather than locked: nothing on the map hints at a mode the player cannot reach yet.
    DEFN_CHECK(!view_model.endless.has_value());
    // And nothing else moved because a beacon was declared in the data.
    DEFN_CHECK_EQ(static_cast<int>(view_model.missions.size()), 3);
    DEFN_CHECK_EQ(view_model.completed_count, 3);
    DEFN_CHECK_EQ(static_cast<int>(view_model.routes.size()), 2);
}

DEFN_TEST(campaign_map_presenter_shows_the_beacon_without_counting_it_as_a_mission) {
    const CampaignMapViewModel view_model = CampaignMapPresenter::present(make_map_with_beacon(), make_completed_campaign(),
                                                                          {.unlocked = true,
                                                                           .best_wave = 17,
                                                                           .best_score = 4820,
                                                                           .base_starting_energy = 105,
                                                                           .effective_starting_energy = 125,
                                                                           .base_integrity = 4,
                                                                           .effective_base_integrity = 5});

    DEFN_REQUIRE(view_model.endless.has_value());
    DEFN_CHECK_EQ(view_model.endless->title, std::string("Standing Engagement"));
    DEFN_CHECK_CLOSE(view_model.endless->position_x, 0.685, 1e-6);
    DEFN_CHECK_EQ(view_model.endless->preview.texture.path, std::string("res://preview_endless.jpg"));

    // The header reads "3 / 3 SECURED" whether or not a beacon is on the map, and the mission chain is unchanged:
    // a beacon appended to `missions` would have moved both.
    DEFN_CHECK_EQ(static_cast<int>(view_model.missions.size()), 3);
    DEFN_CHECK_EQ(view_model.completed_count, 3);
    DEFN_CHECK_EQ(static_cast<int>(view_model.routes.size()), 2);
    DEFN_CHECK_EQ(view_model.initial_selected_level_id, std::string("level_01"));

    // The standing route leaves from the mission that unlocks it, not from wherever the list happens to end.
    DEFN_CHECK_EQ(static_cast<int>(view_model.endless->route_from_index), 1);
}

DEFN_TEST(campaign_map_presenter_carries_the_endless_record_and_conditions) {
    const CampaignMapViewModel played = CampaignMapPresenter::present(make_map_with_beacon(), make_completed_campaign(),
                                                                      {.unlocked = true,
                                                                       .best_wave = 17,
                                                                       .best_score = 4820,
                                                                       .base_starting_energy = 105,
                                                                       .effective_starting_energy = 125,
                                                                       .base_integrity = 4,
                                                                       .effective_base_integrity = 5});
    DEFN_REQUIRE(played.endless.has_value());
    DEFN_CHECK_EQ(played.endless->best_wave, 17);
    DEFN_CHECK_EQ(played.endless->best_score, 4820);
    DEFN_CHECK_EQ(played.endless->effective_starting_energy, 125);
    DEFN_CHECK_EQ(played.endless->effective_base_integrity, 5);
    DEFN_CHECK(played.endless->record_label.find("17") != std::string::npos);
    DEFN_CHECK(played.endless->record_label.find("4820") != std::string::npos);

    const CampaignMapViewModel unplayed = CampaignMapPresenter::present(make_map_with_beacon(), make_completed_campaign(), {.unlocked = true});
    DEFN_REQUIRE(unplayed.endless.has_value());
    DEFN_CHECK_EQ(unplayed.endless->record_label, std::string("NO WATCH STOOD"));
}

DEFN_TEST(campaign_map_presenter_assigns_states_routes_and_frontier_selection) {
    const CampaignMapViewModel view_model = CampaignMapPresenter::present(make_map(), {{.level_id = "level_01",
                                                                                        .definition = make_level("Desert", 50.0),
                                                                                        .unlocked = true,
                                                                                        .completed = true,
                                                                                        .best_score = 420,
                                                                                        .effective_starting_energy = 60,
                                                                                        .effective_base_integrity = 4},
                                                                                       {.level_id = "level_02",
                                                                                        .definition = make_level("Jungle", 70.0),
                                                                                        .requires_completed = "level_01",
                                                                                        .unlocked = true,
                                                                                        .frontier = true,
                                                                                        .effective_starting_energy = 60,
                                                                                        .effective_base_integrity = 4},
                                                                                       {.level_id = "level_03",
                                                                                        .definition = make_level("Coast", 200.0),
                                                                                        .requires_completed = "level_02",
                                                                                        .effective_starting_energy = 60,
                                                                                        .effective_base_integrity = 4}});

    DEFN_REQUIRE(view_model.missions.size() == static_cast<std::size_t>(3));
    DEFN_CHECK_EQ(view_model.missions[0].state, CampaignNodeState::COMPLETED);
    DEFN_CHECK_EQ(view_model.missions[1].state, CampaignNodeState::FRONTIER);
    DEFN_CHECK_EQ(view_model.missions[2].state, CampaignNodeState::LOCKED);
    DEFN_CHECK_EQ(view_model.initial_selected_level_id, std::string("level_02"));
    DEFN_CHECK_EQ(view_model.completed_count, 1);
    DEFN_REQUIRE(view_model.routes.size() == static_cast<std::size_t>(2));
    DEFN_CHECK_EQ(view_model.routes[0].state, CampaignRouteState::FRONTIER);
    DEFN_CHECK_EQ(view_model.routes[1].state, CampaignRouteState::LOCKED);
    DEFN_CHECK(view_model.missions[2].unlock_requirement.contains("Level 02"));
}

DEFN_TEST(campaign_map_presenter_derives_intel_and_uses_concrete_mission_previews) {
    const CampaignMapViewModel view_model = CampaignMapPresenter::present(
        make_map(),
        {{.level_id = "level_01", .definition = make_level("Desert", 55.0), .unlocked = true, .effective_starting_energy = 70, .effective_base_integrity = 5},
         {.level_id = "level_02", .definition = make_level("Jungle", 105.0), .unlocked = true, .effective_starting_energy = 70, .effective_base_integrity = 5},
         {.level_id = "level_03", .definition = make_level("Coast", 166.0), .unlocked = true, .effective_starting_energy = 70, .effective_base_integrity = 5}});

    DEFN_CHECK_EQ(view_model.missions[0].duration_label, std::string("~2 MIN"));
    DEFN_CHECK_EQ(view_model.missions[2].duration_label, std::string("3+ MIN"));
    DEFN_REQUIRE(view_model.missions[0].enemy_labels.size() == static_cast<std::size_t>(2));
    DEFN_CHECK_EQ(view_model.missions[0].enemy_labels[0], std::string("Heavy Grime"));
    DEFN_CHECK_EQ(view_model.missions[0].preview.texture.path, std::string("res://preview_01.jpg"));
    DEFN_CHECK_EQ(view_model.missions[1].preview.texture.path, std::string("res://preview_02.jpg"));
    DEFN_CHECK_CLOSE(view_model.missions[0].preview.focus_x, 0.38F, 0.0001F);
    DEFN_CHECK_EQ(view_model.missions[0].effective_starting_energy, 70);
}

DEFN_TEST(campaign_map_presenter_duration_boundaries_are_exact) {
    DEFN_CHECK_EQ(CampaignMapPresenter::format_duration(54.999), std::string("~1 MIN"));
    DEFN_CHECK_EQ(CampaignMapPresenter::format_duration(55.0), std::string("~2 MIN"));
    DEFN_CHECK_EQ(CampaignMapPresenter::format_duration(105.0), std::string("~2 MIN"));
    DEFN_CHECK_EQ(CampaignMapPresenter::format_duration(105.001), std::string("~3 MIN"));
    DEFN_CHECK_EQ(CampaignMapPresenter::format_duration(165.0), std::string("~3 MIN"));
    DEFN_CHECK_EQ(CampaignMapPresenter::format_duration(165.001), std::string("3+ MIN"));
}

DEFN_TEST(campaign_preview_framing_covers_and_clamps_focus) {
    const CampaignPreviewFrame frame = CampaignMapPresenter::frame_preview(960.0F, 540.0F, 164.0F, 116.0F, 0.38F, 0.5F, 1.0F);
    DEFN_CHECK_CLOSE(frame.draw_height, 116.0F, 0.001F);
    DEFN_CHECK(frame.draw_width >= 164.0F);
    DEFN_CHECK(frame.origin_x <= 0.0F);
    DEFN_CHECK(frame.origin_x >= 164.0F - frame.draw_width);
    DEFN_CHECK_CLOSE(frame.origin_y, 0.0F, 0.001F);
}

} // namespace defn
