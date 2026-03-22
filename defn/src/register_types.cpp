#include <gdextension_interface.h>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

#include "animation_controller.h"
#include "combat_component.h"
#include "detection_component.h"
#include "game_manager.h"
#include "grid_manager.h"
#include "health_bar_widget.h"
#include "health_component.h"
#include "hud.h"
#include "sound_controller.h"
#include "unit.h"
#include "wave_manager.h"

using namespace godot;

void initialize_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }

    ClassDB::register_class<defn::GridManager>();
    ClassDB::register_class<defn::HealthComponent>();
    ClassDB::register_class<defn::HealthBarWidget>();
    ClassDB::register_class<defn::AnimationController>();
    ClassDB::register_class<defn::SoundController>();
    ClassDB::register_class<defn::DetectionComponent>();
    ClassDB::register_class<defn::CombatComponent>();
    ClassDB::register_class<defn::Unit>();
    ClassDB::register_class<defn::WaveManager>();
    ClassDB::register_class<defn::GameManager>();
    ClassDB::register_class<defn::HUD>();
}

void uninitialize_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
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
