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

  private:
    void setup_sprite_frames();
    void find_new_target() override;
    void do_movement(double delta) override;
    double get_forward_distance(Entity *t) const override;

    int cost = 25;

  protected:
    static void _bind_methods();
};

} // namespace defn

#endif