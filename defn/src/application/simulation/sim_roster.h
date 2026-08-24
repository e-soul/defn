// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef SIM_ROSTER_H
#define SIM_ROSTER_H

#include "unit_definition.h"

#include <string>
#include <utility>
#include <vector>

namespace defn {

// An in-memory UnitCatalog. Native tests fill it from fixtures; a Godot-hosted sweep can fill it from UnitDataLoader
// so the kernel reads the shipped content through the same port the game does.
class SimRoster final : public UnitCatalog {
  public:
    void add(UnitConfig config) { units_.push_back(std::move(config)); }

    [[nodiscard]] std::optional<UnitConfig> get_unit(const std::string &name) const override {
        for (const UnitConfig &unit : units_) {
            if (unit.name == name) {
                return unit;
            }
        }

        return std::nullopt;
    }

    [[nodiscard]] std::vector<UnitConfig> get_friendly_units() const override {
        std::vector<UnitConfig> friendly_units;
        for (const UnitConfig &unit : units_) {
            if (unit.side == UnitSide::FRIENDLY) {
                friendly_units.push_back(unit);
            }
        }

        return friendly_units;
    }

  private:
    std::vector<UnitConfig> units_;
};

} // namespace defn

#endif
