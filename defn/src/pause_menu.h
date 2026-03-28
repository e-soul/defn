#ifndef PAUSE_MENU_H
#define PAUSE_MENU_H

#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/dictionary.hpp>

namespace defn {

using namespace godot;

class PauseMenu : public CanvasLayer {
    GDCLASS(PauseMenu, CanvasLayer)

  public:
    void _ready() override;
    void _input(const Ref<InputEvent> &event) override;

    void toggle_pause();

  protected:
    static void _bind_methods();

  private:
    bool load_config();
    void build_ui();
    void set_paused(bool paused);
    void on_resume();
    void on_main_menu();

    Dictionary menu_data_;
    bool paused_ = false;

    ColorRect *overlay_ = nullptr;
    VBoxContainer *button_container_ = nullptr;
};

} // namespace defn

#endif
