// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause
#ifndef MATCH_RESOURCES_H
#define MATCH_RESOURCES_H
#include "endless_definition.h"
#include "music_playlist.h"
#include "unit_control_config.h"
#include "unit_data.h"
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/sprite_frames.hpp>
#include <map>
#include <tuple>

namespace defn {
struct PreparedMatch {
    UnitDataLoader units;
    LevelDefinition level;
    std::string level_id;
    std::optional<EndlessSchedule> endless;
    UnitControlConfig controls;
    std::optional<MusicPlaylist> music;
    std::vector<UnitConfig> selected_units;
};

struct UnitVisualFrames {
    godot::Ref<godot::SpriteFrames> body;
    godot::Ref<godot::SpriteFrames> muzzle;
};

// Owned by the match, including periods with no living units. All resources stay referenced until scene exit.
class MatchResources {
  public:
    void retain(const std::string &path, const godot::Ref<godot::Resource> &resource);
    [[nodiscard]] godot::Ref<godot::Resource> get(const std::string &path) const;
    void prepare_unit(const UnitConfig &config);
    [[nodiscard]] UnitVisualFrames unit_frames(const std::string &unit_id) const;
    [[nodiscard]] godot::Ref<godot::SpriteFrames> animation_frames(const AnimConfig &animation) const;

  private:
    using AnimationKey = std::tuple<std::string, int, double, bool>;
    [[nodiscard]] static AnimationKey key(const AnimConfig &animation);
    godot::Ref<godot::SpriteFrames> build_animation(const AnimConfig &animation);
    std::map<std::string, godot::Ref<godot::Resource>, std::less<>> resources_;
    std::map<std::string, UnitVisualFrames, std::less<>> units_;
    std::map<AnimationKey, godot::Ref<godot::SpriteFrames>> animations_;
};
} // namespace defn
#endif
