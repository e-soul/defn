"""Explicit source inventories. A source belongs to exactly one group."""

from pathlib import PurePosixPath

CORE = """
src/application/ports/random_source.cpp
src/application/menu/menu_flow_use_case.cpp
src/application/progression/progression_use_cases.cpp
src/application/progression/unit_progression_mapper.cpp
src/application/settings/settings_session.cpp
src/application/settings/settings_use_case.cpp
src/domain/progression/progression_rules.cpp
src/domain/progression/upgrade_draft_builder.cpp
src/domain/match/match_session.cpp
src/application/match/match_director.cpp
src/application/match/endless_director.cpp
src/application/content/content_validator.cpp
src/domain/match/camera_scroll_controller.cpp
src/domain/content/ui_theme_models.cpp
src/application/match/deployment_service.cpp
src/domain/match/spawn_timeline.cpp
src/domain/content/hostile_scaling.cpp
src/domain/match/endless_wave_generator.cpp
src/application/match/spawn_scheduler.cpp
src/domain/combat/combat_logic.cpp
src/domain/combat/projectile_rules.cpp
src/domain/combat/field_promotion.cpp
src/domain/unit_control/reposition_logic.cpp
src/application/combat/field_promotion_runtime.cpp
src/application/combat/combat_use_cases.cpp
src/domain/content/force_mix.cpp
src/domain/content/unit_runtime_profile.cpp
src/domain/content/unit_runtime_config_resolver.cpp
src/domain/content/unit_animation_state.cpp
""".split()

PRESENTERS = """
src/adapters/presenters/progression_presentation.cpp
src/adapters/presenters/progression_stats_presenter.cpp
src/adapters/presenters/progression_stat_visualization.cpp
src/adapters/presenters/deploy_card_view_model.cpp
src/adapters/presenters/hud_presenter.cpp
src/adapters/presenters/match_result_cutscene_view_model.cpp
src/adapters/presenters/menu_view_model.cpp
src/adapters/presenters/campaign_map_view_model.cpp
src/adapters/presenters/score_screen_view_model.cpp
""".split()

ENGINE = """
src/framework/registration/register_types.cpp
src/adapters/json/json_file_loader.cpp
src/adapters/json/campaign_map_data_loader.cpp
src/adapters/json/endless_schedule_loader.cpp
src/adapters/json/music_playlist_loader.cpp
src/adapters/json/unit_control_config_loader.cpp
src/framework/godot_nodes/attack_target_resolver.cpp
src/framework/godot_nodes/battle_entity.cpp
src/framework/godot_nodes/background_music_player.cpp
src/framework/godot_nodes/base_objective.cpp
src/framework/godot_nodes/base_objective_factory.cpp
src/framework/godot_nodes/grid_manager.cpp
src/adapters/godot/progression/progression_manager.cpp
src/adapters/json/progression_catalog.cpp
src/adapters/json/upgrade_catalog.cpp
src/adapters/json/progression_save_repository.cpp
src/adapters/godot/ui/owned_upgrades_panel.cpp
src/adapters/godot/ui/progression_stats_screen_view.cpp
src/adapters/godot/ui/progression_stat_meter.cpp
src/adapters/godot/ui/meter_geometry.cpp
src/adapters/godot/ui/icon_medallion.cpp
src/adapters/godot/ui/hud_meters.cpp
src/adapters/godot/ui/ui_sfx_player.cpp
src/framework/godot_nodes/vfx/bounty_energy_effect.cpp
src/framework/godot_nodes/vfx/field_promotion_effect.cpp
src/framework/godot_nodes/field_promotion_view.cpp
src/adapters/json/content_repository.cpp
src/adapters/godot/content_startup_validator.cpp
src/framework/godot_nodes/hitbox_component.cpp
src/adapters/json/level_loader.cpp
src/adapters/godot/game_background_builder.cpp
src/adapters/json/menu_data_loader.cpp
src/adapters/godot/ui/menu_backdrop.cpp
src/adapters/json/ui_theme_loader.cpp
src/adapters/godot/ui/ui_theme_provider.cpp
src/adapters/godot/ui/ui_widgets.cpp
src/adapters/godot/ui/ui_screen_scaffold.cpp
src/adapters/godot/settings_adapters.cpp
src/adapters/godot/scene_navigator.cpp
src/framework/godot_nodes/health_component.cpp
src/framework/godot_nodes/health_bar_widget.cpp
src/framework/godot_nodes/animation_controller.cpp
src/framework/godot_nodes/sound_controller.cpp
src/framework/godot_nodes/movement_component.cpp
src/framework/godot_nodes/unit_control_component.cpp
src/framework/godot_nodes/selection_indicator.cpp
src/framework/godot_nodes/reposition_destination_marker.cpp
src/framework/godot_nodes/unit_selection_controller.cpp
src/adapters/godot/ui/campaign_preview_view.cpp
src/adapters/godot/ui/campaign_map_node_view.cpp
src/adapters/godot/ui/operation_dossier_view.cpp
src/adapters/godot/ui/campaign_map_view.cpp
src/adapters/godot/ui/deploy_card_presenter.cpp
src/adapters/godot/ui/upgrade_card_presenter.cpp
src/framework/godot_nodes/detection_component.cpp
src/framework/godot_nodes/projectile_attack.cpp
src/framework/godot_nodes/projectile_factory.cpp
src/adapters/godot/ui/score_screen_view.cpp
src/adapters/godot/combat/combat_runtime.cpp
src/adapters/godot/combat/combat_target_selector.cpp
src/adapters/godot/combat/combat_attack_executor.cpp
src/adapters/godot/combat/damage_dispatcher.cpp
src/framework/godot_nodes/combat_component.cpp
src/framework/godot_nodes/unit.cpp
src/framework/godot_nodes/unit_factory.cpp
src/adapters/json/unit_data.cpp
src/framework/godot_nodes/game_manager.cpp
src/framework/godot_nodes/hud.cpp
src/framework/godot_nodes/menu_manager.cpp
src/framework/godot_nodes/pause_menu.cpp
src/framework/godot_nodes/settings_runtime.cpp
""".split()

DEBUG_RENDERING = ["src/framework/godot_nodes/belt_debug_overlay.cpp"]

SIMULATION = """
src/application/simulation/sim_world.cpp
src/application/simulation/sim_engagement_lab.cpp
src/application/simulation/sim_camera.cpp
src/application/simulation/sim_progression.cpp
src/application/simulation/sim_scenario.cpp
src/application/simulation/sim_report.cpp
src/application/simulation/sim_match.cpp
src/application/simulation/policies/sim_policies.cpp
""".split()

HOSTED_RUNNERS = """
src/framework/testing/defn_sim_runner.cpp
src/framework/testing/defn_conformance_runner.cpp
src/framework/testing/defn_balance_runner.cpp
src/framework/testing/defn_matrix_runner.cpp
src/framework/testing/defn_endless_runner.cpp
src/framework/testing/defn_hosted_test_runner.cpp
""".split()

SHARED_TESTS = """
tests/test_combat_logic.cpp
tests/test_match_session_core.cpp
tests/test_progression_presentation.cpp
""".split()

HOSTED_TESTS = """
tests/test_match_and_services.cpp
tests/test_models.cpp
tests/test_parsing.cpp
tests/test_settings_adapters.cpp
tests/test_shipped_content.cpp
tests/test_ui_presenters.cpp
tests/test_ui_theme_provider.cpp
tests/test_unit_profiles.cpp
""".split()

NATIVE_TESTS = """
tests/test_main.cpp
tests/test_animation_clock.cpp
tests/test_projectile_flight.cpp
tests/test_unit_animation_state.cpp
tests/test_sim_world.cpp
tests/test_sim_match.cpp
tests/test_engagement_lab.cpp
tests/test_field_promotion.cpp
tests/test_reposition_logic.cpp
tests/test_progression_rules.cpp
tests/test_progression_use_cases.cpp
tests/test_upgrade_draft_builder.cpp
tests/test_progression_stats_presenter.cpp
tests/test_progression_stat_visualization.cpp
tests/test_unit_runtime_config_resolver.cpp
tests/test_spawn_timeline.cpp
tests/test_hostile_scaling.cpp
tests/test_endless_wave_generator.cpp
tests/test_endless_director.cpp
tests/test_menu_flow_use_case.cpp
tests/test_ui_theme.cpp
tests/test_presentation_view_models.cpp
tests/test_campaign_map_presenter.cpp
tests/test_settings_session.cpp
tests/test_settings_use_case.cpp
tests/test_random_source.cpp
tests/test_content_validator.cpp
""".split()

NATIVE_SUPPORT = CORE + PRESENTERS + SIMULATION
NATIVE = NATIVE_SUPPORT + SHARED_TESTS + NATIVE_TESTS


def extension_sources(debug=False, hosted=False):
    return (CORE + PRESENTERS + ENGINE
            + (DEBUG_RENDERING if debug else [])
            + (SIMULATION + HOSTED_RUNNERS + SHARED_TESTS + HOSTED_TESTS if hosted else []))


def include_paths(native=False):
    groups = CORE + PRESENTERS + SIMULATION
    if not native:
        groups += ENGINE + HOSTED_RUNNERS
    return [".", "src", *sorted({str(PurePosixPath(s).parent) for s in groups})]
