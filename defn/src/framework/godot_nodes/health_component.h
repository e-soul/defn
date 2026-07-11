#ifndef HEALTH_COMPONENT_H
#define HEALTH_COMPONENT_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace defn {

using namespace godot;

class HealthComponent : public Node {
    GDCLASS(HealthComponent, Node)

  public:
    void configure(int max_hp);
    void set_max_hp_and_heal(int new_max_hp);
    [[nodiscard]] int take_damage(int amount);
    bool is_dead() const { return current_hp <= 0; }
    int get_current_hp() const { return current_hp; }
    int get_max_hp() const { return max_hp; }

  protected:
    static void _bind_methods();

  private:
    int max_hp = 100;
    int current_hp = 100;
};

} // namespace defn

#endif
