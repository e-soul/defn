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

  private:
    void setup_sprite_frames();
    void find_new_target() override;
    void do_movement(double delta) override;
    void check_breach();
    double get_forward_distance(Entity *other) const override;

    int bounty = 5;

  protected:
    static void _bind_methods();
};

} // namespace defn

#endif