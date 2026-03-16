#ifndef DEFENDER_H
#define DEFENDER_H

#include "entity.h"

namespace defn {

class Hostile;

class Defender : public Entity {
    GDCLASS(Defender, Entity)

  public:
    Defender() = default;

    void _ready() override;

  private:
    void setup_sprite_frames();
    void find_new_target() override;
    void do_movement(double delta) override;
    double get_forward_distance(Entity *other) const override;

    int cost = 25;

  protected:
    static void _bind_methods();
};

} // namespace defn

#endif