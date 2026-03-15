#ifndef DEFENDER_H
#define DEFENDER_H

#include "entity.h"

namespace defn {

class Hostile;

class Defender : public Entity {
    GDCLASS(Defender, Entity)

public:
    Defender();

    void _ready() override;
    void _process(double delta) override;

    int get_cost() const { return cost; }
    void set_cost(int c) { cost = c; }

    bool is_engaged() const { return engaged; }

private:
    void setup_sprite_frames();
    void setup_muzzle_flash();
    void find_target();
    bool try_keep_target();
    void find_new_target();
    void on_attack_timeout();
    void on_ranged_timeout();
    void on_muzzle_flash_finished();
    void do_movement(double delta);
    void on_animation_finished();
    void start_death_fade();

    int cost = 25;
    bool engaged = false;
    Hostile *target = nullptr;

protected:
    static void _bind_methods();
};

} // namespace defn

#endif
