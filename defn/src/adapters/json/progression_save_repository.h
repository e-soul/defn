#ifndef PROGRESSION_SAVE_REPOSITORY_H
#define PROGRESSION_SAVE_REPOSITORY_H

#include "player_profile.h"
#include "progression_ports.h"

#include <godot_cpp/variant/string.hpp>

#include <optional>

namespace defn {

using namespace godot;

class ProgressionSaveRepository : public ProfileRepository {
  public:
    explicit ProgressionSaveRepository(String path = {}) : path_(path) {}

    [[nodiscard]] std::optional<PlayerProfile> load_profile() const override;
    bool save_profile(const PlayerProfile &profile) const override;

    static std::optional<PlayerProfile> load(const String &path);
    static bool save(const String &path, const PlayerProfile &save_data);

  private:
    String path_;
};

} // namespace defn

#endif