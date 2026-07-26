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

} // namespace

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
