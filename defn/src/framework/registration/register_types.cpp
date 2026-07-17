#include <gdextension_interface.h>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/godot.hpp>

#include "animation_controller.h"
#include "background_music_player.h"
#include "base_objective.h"
#include "battle_entity.h"
#include "combat_component.h"
#include "detection_component.h"
#include "field_promotion_effect.h"
#include "field_promotion_view.h"
#include "game_manager.h"
#include "grid_manager.h"
#include "health_bar_widget.h"
#include "health_component.h"
#include "hitbox_component.h"
#include "hud.h"
#include "menu_manager.h"
#include "movement_component.h"
#include "pause_menu.h"
#include "progression_manager.h"
#include "progression_stat_meter.h"
#include "progression_stats_screen_view.h"
#include "projectile_attack.h"
#include "sound_controller.h"
#include "unit.h"

#ifdef DEFN_HOSTED_TESTS_ENABLED
#include "defn_hosted_test_runner.h"
#endif

using namespace godot;

void initialize_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }

    ClassDB::register_class<defn::GridManager>();
    defn::GridManager::register_singleton();
    ClassDB::register_class<defn::CampaignService>();
    defn::CampaignService::register_singleton();
    GDREGISTER_ABSTRACT_CLASS(defn::BattleEntity);
    ClassDB::register_internal_class<defn::BackgroundMusicPlayer>();
    ClassDB::register_class<defn::HealthComponent>();
    ClassDB::register_class<defn::HealthBarWidget>();
    ClassDB::register_class<defn::HitboxComponent>();
    ClassDB::register_class<defn::AnimationController>();
    ClassDB::register_class<defn::SoundController>();
    ClassDB::register_class<defn::MovementComponent>();
    ClassDB::register_class<defn::BaseObjective>();
    ClassDB::register_class<defn::DetectionComponent>();
    ClassDB::register_class<defn::CombatComponent>();
    ClassDB::register_class<defn::ProjectileAttack>();
    ClassDB::register_internal_class<defn::FieldPromotionView>();
    ClassDB::register_internal_class<defn::vfx::FieldPromotionEffect>();
    ClassDB::register_class<defn::Unit>();
    ClassDB::register_class<defn::GameManager>();
    ClassDB::register_class<defn::HUD>();
    ClassDB::register_class<defn::MenuManager>();
    ClassDB::register_internal_class<defn::ProgressionStatsScreenView>();
    ClassDB::register_internal_class<defn::ProgressionStatMeter>();
    ClassDB::register_class<defn::PauseMenu>();
#ifdef DEFN_HOSTED_TESTS_ENABLED
    ClassDB::register_class<defn::DefnHostedTestRunner>();
#endif
}

void uninitialize_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }

    defn::CampaignService::unregister_singleton();
    defn::GridManager::unregister_singleton();
}

extern "C" {

GDExtensionBool GDE_EXPORT defn_core_library_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, const GDExtensionClassLibraryPtr p_library,
                                                  GDExtensionInitialization *r_initialization) {

    GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

    init_obj.register_initializer(initialize_module);
    init_obj.register_terminator(uninitialize_module);
    init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

    return init_obj.init();
}
}
