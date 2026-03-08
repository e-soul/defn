#include <godot_cpp/godot.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <gdextension_interface.h>

#include "grid_manager.h"
#include "entity.h"
#include "defender.h"
#include "hostile.h"
#include "wave_manager.h"
#include "game_manager.h"
#include "hud.h"

using namespace godot;

void initialize_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }

    ClassDB::register_class<defn::GridManager>();
    ClassDB::register_class<defn::Entity>();
    ClassDB::register_class<defn::Defender>();
    ClassDB::register_class<defn::Hostile>();
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

GDExtensionBool GDE_EXPORT
defn_core_library_init(GDExtensionInterfaceGetProcAddress p_get_proc_address,
                       const GDExtensionClassLibraryPtr p_library,
                       GDExtensionInitialization *r_initialization) {

    GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

    init_obj.register_initializer(initialize_module);
    init_obj.register_terminator(uninitialize_module);
    init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

    return init_obj.init();
}
}
