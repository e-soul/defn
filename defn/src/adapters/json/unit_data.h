#ifndef UNIT_DATA_H
#define UNIT_DATA_H

#include "unit_definition.h"

#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/variant.hpp>

namespace defn {

using namespace godot;

UnitSide parse_unit_side(const Dictionary &unit_dict);
SplashTargetRoundingMode parse_splash_target_rounding_mode(const Variant &value, SplashTargetRoundingMode fallback);

class UnitDataLoader : public UnitCatalog {
  public:
    bool load(const String &unit_path, const String &global_path);
    bool load_from_data(const Dictionary &unit_data, const Dictionary &global_data);

    [[nodiscard]] std::optional<UnitConfig> get_unit(const std::string &name) const override;
    [[nodiscard]] std::vector<UnitConfig> get_friendly_units() const override;

    const GlobalUnitConfig &get_globals() const { return globals_; }

  private:
    GlobalUnitConfig globals_;
    std::vector<UnitConfig> units_;
};

} // namespace defn

#endif
