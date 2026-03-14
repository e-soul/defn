#ifndef HOSTILE_H
#define HOSTILE_H

#include "entity.h"

namespace defn {

class Defender;

class Hostile : public Entity {
    GDCLASS(Hostile, Entity)

public:
    Hostile();

    void _ready() override;
    void _process(double delta) override;

    int get_bounty() const { return bounty; }
    void set_bounty(int b) { bounty = b; }
    bool is_engaged() const { return engaged; }

private:
    void setup_sprite_frames();
    void setup_muzzle_flash();
    void find_target();
    void on_attack_timeout();
    void on_ranged_timeout();
    void on_muzzle_flash_finished();
    void do_movement(double delta);
    void check_breach();
    void on_animation_finished();
    void start_death_fade();

    int bounty = 5;
    bool engaged = false;
    Defender *target = nullptr;

protected:
    static void _bind_methods();
};

} // namespace defn

#endif
