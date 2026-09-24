// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "unit_selection_controller.h"

#include "collision_layers.h"
#include "godot_color.h"
#include "reposition_destination_marker.h"
#include "selection_indicator.h"
#include "unit.h"

#include <algorithm>
#include <cstdint>

#include <godot_cpp/classes/area2d.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/physics_direct_space_state2d.hpp>
#include <godot_cpp/classes/physics_point_query_parameters2d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/world2d.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/typed_array.hpp>

namespace defn {

namespace {

SelectionIndicatorStyle to_indicator_style(const GroundMarkerConfig &config) {
    return {
        .radius_x = config.radius_x,
        .radius_y = config.radius_y,
        .border_width = config.border_width,
        .world_offset_y = config.ground_offset_y,
        .fill_color = to_godot_color(config.fill_color),
        .border_color = to_godot_color(config.border_color),
    };
}

RepositionDestinationMarkerStyle to_destination_marker_style(const DestinationMarkerConfig &config) {
    return {
        .radius_x = config.radius_x,
        .radius_y = config.radius_y,
        .border_width = config.border_width,
        .minimum_scale = config.minimum_scale,
        .maximum_scale = config.maximum_scale,
        .pulse_duration_seconds = config.pulse_duration_seconds,
        .pulse_count = config.pulse_count,
        .fill_color = to_godot_color(config.fill_color),
        .border_color = to_godot_color(config.border_color),
    };
}

Unit *unit_from_collider(Object *collider) {
    auto *area = Object::cast_to<Area2D>(collider);
    return area != nullptr ? Object::cast_to<Unit>(area->get_parent()) : nullptr;
}

bool candidate_precedes(const Unit *left, const Unit *right, const godot::Vector2 &world_position) {
    const real_t left_distance = left->get_global_position().distance_squared_to(world_position);
    const real_t right_distance = right->get_global_position().distance_squared_to(world_position);
    if (!Math::is_equal_approx(left_distance, right_distance)) {
        return left_distance < right_distance;
    }

    const real_t left_y = left->get_global_position().y;
    const real_t right_y = right->get_global_position().y;
    if (!Math::is_equal_approx(left_y, right_y)) {
        return left_y > right_y;
    }

    return left->get_instance_id() < right->get_instance_id();
}

} // namespace

void UnitSelectionController::_bind_methods() {}

void UnitSelectionController::configure(Node2D *entity_container, const UnitControlConfig &config) {
    entity_container_ = entity_container;
    config_ = config;
    set_process_unhandled_input(true);
    if (indicator_ == nullptr) {
        indicator_ = memnew(SelectionIndicator);
        indicator_->set_name("SelectionIndicator");
        add_child(indicator_);
    }
    indicator_->configure(to_indicator_style(config_.selection_marker));
    indicator_->set_visible(false);
    if (hover_indicator_ == nullptr) {
        hover_indicator_ = memnew(SelectionIndicator);
        hover_indicator_->set_name("HoverIndicator");
        add_child(hover_indicator_);
    }
    hover_indicator_->configure(to_indicator_style(config_.hover_marker));
    hover_indicator_->set_visible(false);
}

void UnitSelectionController::set_gameplay_available(bool available) {
    gameplay_available_ = available;
    if (!available) {
        clear_hover();
        clear_selection();
        clear_destination_marker();
        cancel_all_repositions_for_match_end();
    }
}

bool UnitSelectionController::has_selection() const { return resolve_selected_unit() != nullptr; }

void UnitSelectionController::_process(double /*delta*/) {
    if (!selected_unit_id_.is_null() && resolve_selected_unit() == nullptr) {
        clear_selection();
    }
    if (!hovered_unit_id_.is_null() && resolve_hovered_unit() == nullptr) {
        clear_hover();
    }
}

void UnitSelectionController::_unhandled_input(const Ref<InputEvent> &event) {
    SceneTree *tree = get_tree();
    if (!gameplay_available_ || tree == nullptr || tree->is_paused()) {
        clear_hover();
        return;
    }

    if (auto *mouse_motion = Object::cast_to<InputEventMouseMotion>(event.ptr()); mouse_motion != nullptr) {
        const godot::Vector2 world_position = make_canvas_position_local(mouse_motion->get_position());
        update_hover(pick_friendly(world_position));
        return;
    }

    auto *mouse_button = Object::cast_to<InputEventMouseButton>(event.ptr());
    if (mouse_button == nullptr || !mouse_button->is_pressed()) {
        return;
    }

    const godot::Vector2 world_position = make_canvas_position_local(mouse_button->get_position());
    if (mouse_button->get_button_index() == MOUSE_BUTTON_LEFT) {
        if (Unit *candidate = pick_friendly(world_position); candidate != nullptr) {
            select(candidate);
            get_viewport()->set_input_as_handled();
            return;
        }

        if (Unit *selected = resolve_selected_unit(); selected != nullptr) {
            if (selected->request_reposition(world_position.x, config_.reposition.arrival_epsilon)) {
                const real_t destination_y =
                    indicator_ != nullptr && indicator_->is_visible() ? indicator_->get_global_position().y : selected->get_global_position().y;
                show_destination_marker({world_position.x, destination_y});
            }
            get_viewport()->set_input_as_handled();
        }
        return;
    }

    if (mouse_button->get_button_index() == MOUSE_BUTTON_RIGHT && pick_friendly(world_position) == nullptr) {
        clear_selection();
        get_viewport()->set_input_as_handled();
    }
}

Unit *UnitSelectionController::resolve_selected_unit() const {
    if (selected_unit_id_.is_null()) {
        return nullptr;
    }

    auto *unit = Object::cast_to<Unit>(ObjectDB::get_instance(static_cast<uint64_t>(selected_unit_id_)));
    if (unit == nullptr || unit->is_queued_for_deletion() || !unit->is_commandable()) {
        return nullptr;
    }
    return unit;
}

Unit *UnitSelectionController::resolve_hovered_unit() const {
    if (hovered_unit_id_.is_null()) {
        return nullptr;
    }

    auto *unit = Object::cast_to<Unit>(ObjectDB::get_instance(static_cast<uint64_t>(hovered_unit_id_)));
    if (unit == nullptr || unit->is_queued_for_deletion() || !unit->is_commandable()) {
        return nullptr;
    }
    return unit;
}

std::vector<Unit *> UnitSelectionController::query_friendly_candidates(const godot::Vector2 &world_position) const {
    std::vector<Unit *> candidates;
    const Ref<World2D> world = get_world_2d();
    if (world.is_valid()) {
        if (PhysicsDirectSpaceState2D *space_state = world->get_direct_space_state(); space_state != nullptr) {
            Ref<PhysicsPointQueryParameters2D> query;
            query.instantiate();
            query->set_position(world_position);
            query->set_collision_mask(CollisionLayers::FRIENDLY_HITBOX);
            query->set_collide_with_areas(true);
            query->set_collide_with_bodies(false);

            const TypedArray<Dictionary> hits = space_state->intersect_point(query, config_.picking.max_candidates);
            candidates.reserve(static_cast<std::size_t>(hits.size()));
            for (const Variant &hit_variant : hits) {
                const Dictionary hit = hit_variant;
                Object *collider = hit.get("collider", Variant());
                Unit *unit = unit_from_collider(collider);
                if (unit != nullptr && unit->is_commandable() && std::ranges::find(candidates, unit) == candidates.end()) {
                    candidates.push_back(unit);
                }
            }
        }
    }

    if (entity_container_ == nullptr) {
        return candidates;
    }
    const int child_count = entity_container_->get_child_count();
    for (int child_index = 0; child_index < child_count; ++child_index) {
        auto *unit = Object::cast_to<Unit>(entity_container_->get_child(child_index));
        if (unit != nullptr && unit->contains_selection_point(world_position, config_.picking.fallback_radius) &&
            std::ranges::find(candidates, unit) == candidates.end()) {
            candidates.push_back(unit);
        }
    }
    return candidates;
}

Unit *UnitSelectionController::pick_friendly(const godot::Vector2 &world_position) const {
    std::vector<Unit *> candidates = query_friendly_candidates(world_position);
    if (candidates.empty()) {
        return nullptr;
    }

    std::ranges::sort(candidates, [&world_position](const Unit *left, const Unit *right) { return candidate_precedes(left, right, world_position); });
    return candidates.front();
}

void UnitSelectionController::select(Unit *unit) {
    if (unit == nullptr || !unit->is_commandable()) {
        return;
    }
    if (resolve_selected_unit() == unit) {
        return;
    }

    clear_hover();
    clear_selection();
    selected_unit_id_ = ObjectID(unit->get_instance_id());
    selected_died_connection_ = callable_mp(this, &UnitSelectionController::on_selected_unit_died).bind(static_cast<uint64_t>(selected_unit_id_));
    selected_tree_exit_connection_ = callable_mp(this, &UnitSelectionController::on_selected_unit_tree_exiting).bind(static_cast<uint64_t>(selected_unit_id_));
    unit->connect("unit_died", selected_died_connection_);
    unit->connect("tree_exiting", selected_tree_exit_connection_);
    attach_indicator(unit);
}

void UnitSelectionController::update_hover(Unit *unit) {
    if (unit == resolve_selected_unit()) {
        unit = nullptr;
    }
    if (unit == resolve_hovered_unit()) {
        return;
    }

    clear_hover();
    if (unit == nullptr) {
        return;
    }

    hovered_unit_id_ = ObjectID(unit->get_instance_id());
    hovered_tree_exit_connection_ = callable_mp(this, &UnitSelectionController::on_hovered_unit_tree_exiting).bind(static_cast<uint64_t>(hovered_unit_id_));
    unit->connect("tree_exiting", hovered_tree_exit_connection_);
    attach_ground_indicator(hover_indicator_, unit);
}

void UnitSelectionController::clear_hover() {
    if (!hovered_unit_id_.is_null()) {
        auto *unit = Object::cast_to<Unit>(ObjectDB::get_instance(static_cast<uint64_t>(hovered_unit_id_)));
        disconnect_hovered_signals(unit);
    }
    hovered_unit_id_ = ObjectID();
    hovered_tree_exit_connection_ = {};
    detach_ground_indicator(hover_indicator_);
}

void UnitSelectionController::clear_selection() {
    if (!selected_unit_id_.is_null()) {
        auto *unit = Object::cast_to<Unit>(ObjectDB::get_instance(static_cast<uint64_t>(selected_unit_id_)));
        disconnect_selected_signals(unit);
    }
    selected_unit_id_ = ObjectID();
    selected_died_connection_ = {};
    selected_tree_exit_connection_ = {};
    detach_indicator();
}

void UnitSelectionController::attach_indicator(Unit *unit) { attach_ground_indicator(indicator_, unit); }

void UnitSelectionController::detach_indicator() { detach_ground_indicator(indicator_); }

void UnitSelectionController::attach_ground_indicator(SelectionIndicator *indicator, Unit *unit) {
    if (indicator == nullptr || unit == nullptr) {
        return;
    }

    indicator->reparent(unit, false);
    const godot::Vector2 unit_scale = unit->get_scale();
    const real_t inverse_x = !Math::is_zero_approx(unit_scale.x) ? 1.0F / unit_scale.x : 1.0F;
    const real_t inverse_y = !Math::is_zero_approx(unit_scale.y) ? 1.0F / unit_scale.y : 1.0F;
    const real_t ground_offset_y = unit->get_selection_ground_offset_y(indicator->get_world_offset_y());
    indicator->set_scale({inverse_x, inverse_y});
    indicator->set_position({0.0F, ground_offset_y * inverse_y});
    indicator->set_visible(true);
}

void UnitSelectionController::detach_ground_indicator(SelectionIndicator *indicator) {
    if (indicator == nullptr) {
        return;
    }
    indicator->set_visible(false);
    if (indicator->get_parent() != this) {
        indicator->reparent(this, false);
    }
    indicator->set_position({});
    indicator->set_scale({1.0F, 1.0F});
}

void UnitSelectionController::show_destination_marker(const godot::Vector2 &world_position) {
    clear_destination_marker();
    if (entity_container_ == nullptr) {
        return;
    }

    auto *marker = memnew(RepositionDestinationMarker);
    marker->set_name("RepositionDestinationMarker");
    entity_container_->add_child(marker);
    marker->set_global_position(world_position);
    marker->configure(to_destination_marker_style(config_.destination_marker));
    destination_marker_id_ = ObjectID(marker->get_instance_id());
}

void UnitSelectionController::clear_destination_marker() {
    if (!destination_marker_id_.is_null()) {
        auto *marker = Object::cast_to<RepositionDestinationMarker>(ObjectDB::get_instance(static_cast<uint64_t>(destination_marker_id_)));
        if (marker != nullptr && !marker->is_queued_for_deletion()) {
            marker->queue_free();
        }
    }
    destination_marker_id_ = ObjectID();
}

void UnitSelectionController::disconnect_selected_signals(Unit *unit) {
    if (unit == nullptr) {
        return;
    }
    if (selected_died_connection_.is_valid() && unit->is_connected("unit_died", selected_died_connection_)) {
        unit->disconnect("unit_died", selected_died_connection_);
    }
    if (selected_tree_exit_connection_.is_valid() && unit->is_connected("tree_exiting", selected_tree_exit_connection_)) {
        unit->disconnect("tree_exiting", selected_tree_exit_connection_);
    }
}

void UnitSelectionController::disconnect_hovered_signals(Unit *unit) {
    if (unit != nullptr && hovered_tree_exit_connection_.is_valid() && unit->is_connected("tree_exiting", hovered_tree_exit_connection_)) {
        unit->disconnect("tree_exiting", hovered_tree_exit_connection_);
    }
}

void UnitSelectionController::cancel_all_repositions_for_match_end() {
    if (entity_container_ == nullptr) {
        return;
    }

    const int child_count = entity_container_->get_child_count();
    for (int child_index = 0; child_index < child_count; ++child_index) {
        if (auto *unit = Object::cast_to<Unit>(entity_container_->get_child(child_index)); unit != nullptr) {
            unit->cancel_reposition_for_match_end();
        }
    }
}

void UnitSelectionController::on_selected_unit_died(Node * /*unit*/, uint64_t expected_id) {
    if (static_cast<uint64_t>(selected_unit_id_) == expected_id) {
        clear_selection();
    }
}

void UnitSelectionController::on_selected_unit_tree_exiting(uint64_t expected_id) {
    if (static_cast<uint64_t>(selected_unit_id_) == expected_id) {
        clear_selection();
    }
}

void UnitSelectionController::on_hovered_unit_tree_exiting(uint64_t expected_id) {
    if (static_cast<uint64_t>(hovered_unit_id_) == expected_id) {
        clear_hover();
    }
}

} // namespace defn
