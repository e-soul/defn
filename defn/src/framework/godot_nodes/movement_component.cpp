#include "movement_component.h"

#include "grid_manager.h"

namespace defn {

void MovementComponent::_bind_methods() {}

void MovementComponent::configure(Node2D *owner_node, UnitSide side, real_t speed_pixels_per_second) {
    owner_node_ = owner_node;
    side_ = side;
    speed_pixels_per_second_ = speed_pixels_per_second;
}

void MovementComponent::move(double delta) {
    if (owner_node_ == nullptr || speed_pixels_per_second_ <= 0.0F || delta <= 0.0) {
        stop();
        return;
    }

    auto *grid = GridManager::get_singleton();
    if (grid == nullptr) {
        stop();
        return;
    }

    const real_t displacement = speed_pixels_per_second_ * static_cast<real_t>(delta);
    Vector2 position = owner_node_->get_position();

    if (side_ == UnitSide::FRIENDLY) {
        const auto &rules = grid->get_rules();
        const real_t max_x = grid->get_world_width() - rules.friendly_world_margin;
        if (position.x < max_x) {
            position.x = std::min(position.x + displacement, max_x);
            owner_node_->set_position(position);
            return;
        }

        stop();
        return;
    }

    position.x -= displacement;
    owner_node_->set_position(position);
}

void MovementComponent::stop() {}

} // namespace defn