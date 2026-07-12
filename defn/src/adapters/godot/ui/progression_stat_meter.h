#ifndef PROGRESSION_STAT_METER_H
#define PROGRESSION_STAT_METER_H

#include "progression_stat_visualization.h"

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/string.hpp>

namespace defn {

class ProgressionStatMeter : public godot::Control {
    GDCLASS(ProgressionStatMeter, godot::Control)

  public:
    ProgressionStatMeter();

    void configure(const ProgressionStatVisualViewModel &model);
    void _draw() override;
    void _gui_input(const godot::Ref<godot::InputEvent> &event) override;

    [[nodiscard]] int get_segment_count() const;
    [[nodiscard]] godot::String get_stat_id() const;

  protected:
    static void _bind_methods();

  private:
    void show_detail();
    void hide_detail();
    void on_focus_entered();
    void on_focus_exited();

    ProgressionStatVisualViewModel model_;
    bool pointer_active_ = false;
};

} // namespace defn

#endif
