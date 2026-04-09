#ifndef COLLISION_LAYERS_H
#define COLLISION_LAYERS_H

#include "unit_data.h"

#include <cstdint>

namespace defn {

using CollisionMask = uint32_t;

struct DetectionChannels {
    CollisionMask hitbox_layer = 0U;
    CollisionMask sensor_mask = 0U;
};

namespace CollisionLayers {

inline constexpr CollisionMask NONE = 0U;
inline constexpr CollisionMask FRIENDLY_HITBOX = 1U;
inline constexpr CollisionMask HOSTILE_HITBOX = 2U;
inline constexpr CollisionMask FRIENDLY_SENSOR_MASK = HOSTILE_HITBOX;
inline constexpr CollisionMask HOSTILE_SENSOR_MASK = FRIENDLY_HITBOX;
inline constexpr CollisionMask RIGHT_SCROLL_TRIGGER_MASK = FRIENDLY_HITBOX;
inline constexpr CollisionMask LEFT_SCROLL_TRIGGER_MASK = HOSTILE_HITBOX;
inline constexpr CollisionMask SCROLL_TRIGGER_MASK = RIGHT_SCROLL_TRIGGER_MASK;

} // namespace CollisionLayers

inline constexpr DetectionChannels get_detection_channels(UnitSide side) {
    if (side == UnitSide::FRIENDLY) {
        return {
            .hitbox_layer = CollisionLayers::FRIENDLY_HITBOX,
            .sensor_mask = CollisionLayers::FRIENDLY_SENSOR_MASK,
        };
    }

    return {
        .hitbox_layer = CollisionLayers::HOSTILE_HITBOX,
        .sensor_mask = CollisionLayers::HOSTILE_SENSOR_MASK,
    };
}

} // namespace defn

#endif