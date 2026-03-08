#include "hello.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/texture2d.hpp>

Hello::Hello() {
    speed = 2.0;
}

void Hello::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_speed"), &Hello::get_speed);
    ClassDB::bind_method(D_METHOD("set_speed", "p_speed"), &Hello::set_speed);
    ClassDB::add_property("Hello", PropertyInfo(Variant::FLOAT, "speed"), "set_speed", "get_speed");
}

void Hello::_ready() {
    UtilityFunctions::print("Hello World from C++! And rotating the Godot logo.");
    
    // Enable processing
    set_process(true);
    
    // Load and set the texture
    Ref<Texture2D> texture = ResourceLoader::get_singleton()->load("res://icon.svg");
    if (texture.is_valid()) {
        set_texture(texture);
    }
    
    // Center the sprite on a default 1152x648 screen
    set_position(Vector2(576, 324));
}

void Hello::_process(double delta) {
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }
    set_rotation(get_rotation() + speed * delta);
}

void Hello::set_speed(const double p_speed) {
    speed = p_speed;
}

double Hello::get_speed() const {
    return speed;
}