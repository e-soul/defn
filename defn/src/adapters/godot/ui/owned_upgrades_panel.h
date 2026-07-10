#ifndef OWNED_UPGRADES_PANEL_H
#define OWNED_UPGRADES_PANEL_H

#include "score_screen_models.h"

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/variant/vector2.hpp>

#include <vector>

namespace defn {

using namespace godot;

// Builds a reusable, self-contained view of the upgrades the player currently
// owns. Used by both the end-of-level score screen and the main-menu
// progression screen so the two stay visually consistent.
class OwnedUpgradesPanel {
  public:
    OwnedUpgradesPanel() = delete;

    enum class Layout {
        HorizontalStrip,
        VerticalGrid,
    };

    struct Options {
        godot::Vector2 min_size;
        Layout layout = Layout::HorizontalStrip;
        int grid_columns = 4;
        int card_separation = 12;
    };

    static Control *build(const std::vector<UpgradeCardViewModel> &owned_upgrades, const Options &options);
};

} // namespace defn

#endif
