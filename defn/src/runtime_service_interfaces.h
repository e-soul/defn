#ifndef RUNTIME_SERVICE_INTERFACES_H
#define RUNTIME_SERVICE_INTERFACES_H

#include "progression_catalog.h"
#include "score_screen_models.h"
#include "unit_data.h"

#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>

#include <vector>

namespace defn {

using namespace godot;

class GridQueryService {
  public:
    virtual ~GridQueryService() = default;

    [[nodiscard]] virtual real_t deploy_x() const = 0;
    [[nodiscard]] virtual real_t spawn_x() const = 0;
    [[nodiscard]] virtual real_t sample_belt_y() const = 0;
};

class ProgressionService {
  public:
    virtual ~ProgressionService() = default;

    [[nodiscard]] virtual int get_total_score() const = 0;
    [[nodiscard]] virtual PackedStringArray get_unlocked_units() const = 0;
    [[nodiscard]] virtual bool is_level_completed(const String &level_id) const = 0;
    [[nodiscard]] virtual bool is_level_unlocked(const String &level_id) const = 0;
    [[nodiscard]] virtual bool can_claim_level_upgrade(const String &level_id) const = 0;
    [[nodiscard]] virtual bool can_claim_rescue_draft(const String &level_id) const = 0;
    [[nodiscard]] virtual String get_frontier_level_id() const = 0;
    [[nodiscard]] virtual int get_highest_level_score(const String &level_id) const = 0;
    [[nodiscard]] virtual UnitConfig get_effective_friendly_unit_config(const UnitConfig &base_config) const = 0;
    [[nodiscard]] virtual int get_effective_starting_energy(int base) const = 0;
    [[nodiscard]] virtual int get_effective_energy_regen() const = 0;
    [[nodiscard]] virtual real_t get_effective_bounty_multiplier() const = 0;
    [[nodiscard]] virtual int get_effective_base_integrity(int base) const = 0;
    [[nodiscard]] virtual std::vector<UpgradeCardViewModel> build_upgrade_draft_for_level(const String &level_id) const = 0;
    [[nodiscard]] virtual std::vector<UpgradeCardViewModel> build_rescue_draft_for_level(const String &level_id) const = 0;
    [[nodiscard]] virtual String get_current_level_id() const = 0;
    [[nodiscard]] virtual const std::vector<LevelUnlock> &get_level_unlock_data() const = 0;

    virtual void add_score(int amount) = 0;
    virtual void mark_level_completed(const String &level_id, int level_score) = 0;
    virtual bool claim_level_upgrade(const String &level_id, const String &upgrade_id) = 0;
    virtual bool claim_rescue_draft(const String &level_id, const String &upgrade_id) = 0;
    virtual void save() = 0;
};

} // namespace defn

#endif