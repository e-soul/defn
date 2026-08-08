// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef UNIT_SELECTION_CONTROLLER_H
#define UNIT_SELECTION_CONTROLLER_H

#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/object_id.hpp>
#include <godot_cpp/variant/callable.hpp>

#include <vector>

namespace defn {

using namespace godot;

class SelectionIndicator;
class Unit;

class UnitSelectionController : public Node2D {
    GDCLASS(UnitSelectionController, Node2D)

  public:
    void configure(Node2D *entity_container);
    void set_gameplay_available(bool available);
    void select_unit(Unit *unit) { select(unit); }
    void clear_selection();
    [[nodiscard]] bool has_selection() const;

    void _process(double delta) override;
    void _unhandled_input(const Ref<InputEvent> &event) override;

  protected:
    static void _bind_methods();

  private:
    [[nodiscard]] Unit *resolve_selected_unit() const;
    [[nodiscard]] Unit *resolve_hovered_unit() const;
    [[nodiscard]] std::vector<Unit *> query_friendly_candidates(const godot::Vector2 &world_position) const;
    [[nodiscard]] Unit *pick_friendly(const godot::Vector2 &world_position) const;
    void select(Unit *unit);
    void update_hover(Unit *unit);
    void clear_hover();
    void attach_indicator(Unit *unit);
    void detach_indicator();
    static void attach_ground_indicator(SelectionIndicator *indicator, Unit *unit);
    void detach_ground_indicator(SelectionIndicator *indicator);
    void show_destination_marker(const godot::Vector2 &world_position);
    void clear_destination_marker();
    void disconnect_selected_signals(Unit *unit);
    void disconnect_hovered_signals(Unit *unit);
    void cancel_all_repositions_for_match_end();
    void on_selected_unit_died(Node *unit, uint64_t expected_id);
    void on_selected_unit_tree_exiting(uint64_t expected_id);
    void on_hovered_unit_tree_exiting(uint64_t expected_id);

    Node2D *entity_container_ = nullptr;
    SelectionIndicator *indicator_ = nullptr;
    SelectionIndicator *hover_indicator_ = nullptr;
    ObjectID selected_unit_id_{};
    ObjectID hovered_unit_id_{};
    ObjectID destination_marker_id_{};
    Callable selected_died_connection_{};
    Callable selected_tree_exit_connection_{};
    Callable hovered_tree_exit_connection_{};
    bool gameplay_available_ = true;
};

} // namespace defn

#endif
