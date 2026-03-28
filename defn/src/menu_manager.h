#ifndef MENU_MANAGER_H
#define MENU_MANAGER_H

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

namespace defn {

using namespace godot;

class MenuManager : public Node2D {
    GDCLASS(MenuManager, Node2D)

  public:
    void _ready() override;

  protected:
    static void _bind_methods();

  private:
    bool load_menu_data();
    void setup_background();
    void show_menu(const String &menu_name);
    void clear_buttons();
    void on_button_pressed(const String &action, const String &target);

    Dictionary menu_data_;
    String current_menu_;

    CanvasLayer *ui_layer_ = nullptr;
    TextureRect *background_ = nullptr;
    VBoxContainer *button_container_ = nullptr;
};

} // namespace defn

#endif
