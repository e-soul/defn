#include "battle_entity.h"

#include "health_component.h"
#include "hitbox_component.h"

namespace defn {

void BattleEntity::_bind_methods() {}

void BattleEntity::take_damage(int amount) {
    if (health_component_ != nullptr) {
        health_component_->take_damage(amount);
    }
}

bool BattleEntity::is_dead() const { return health_component_ == nullptr || health_component_->is_dead(); }

int BattleEntity::get_current_hp() const { return health_component_ != nullptr ? health_component_->get_current_hp() : 0; }

int BattleEntity::get_max_hp() const { return health_component_ != nullptr ? health_component_->get_max_hp() : 0; }

Area2D *BattleEntity::get_hitbox() const { return hitbox_component_ != nullptr ? hitbox_component_->get_hitbox() : nullptr; }

Node2D *BattleEntity::ensure_target_anchor() {
    if (target_anchor_ != nullptr) {
        return target_anchor_;
    }

    target_anchor_ = memnew(Node2D);
    target_anchor_->set_name("TargetAnchor");
    add_child(target_anchor_);
    return target_anchor_;
}

} // namespace defn