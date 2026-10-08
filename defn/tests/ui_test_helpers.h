// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause
#ifndef UI_TEST_HELPERS_H
#define UI_TEST_HELPERS_H
#include "campaign_map_view.h"
#include "menu_manager.h"
#include "test_harness.h"
#include "unit_factory.h"
#include <algorithm>
#include <cmath>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/classes/window.hpp>
#include <initializer_list>
#include <memory>
#include <vector>
namespace defn::ui_test {
using namespace godot;
template <typename ObjectType> struct GodotObjectDeleter {
    void operator()(ObjectType *object) const { memdelete(object); }
};

template <typename ObjectType> using GodotObjectOwner = std::unique_ptr<ObjectType, GodotObjectDeleter<ObjectType>>;

inline Window *scene_root() {
    auto *tree = Object::cast_to<SceneTree>(Engine::get_singleton()->get_main_loop());
    return tree == nullptr ? nullptr : tree->get_root();
}

// Mounts the node under the real scene root so `_ready()` paths that use `get_tree()` behave as they do in game.
template <typename NodeType> class TreeMountedNode {
  public:
    TreeMountedNode() : node_(memnew(NodeType)), root_(scene_root()) {
        if (root_ != nullptr) {
            root_->add_child(node_);
        }
    }

    TreeMountedNode(const TreeMountedNode &) = delete;
    TreeMountedNode &operator=(const TreeMountedNode &) = delete;
    TreeMountedNode(TreeMountedNode &&) = delete;
    TreeMountedNode &operator=(TreeMountedNode &&) = delete;

    ~TreeMountedNode() {
        if (root_ != nullptr) {
            root_->remove_child(node_);
        }
        memdelete(node_);
    }

    [[nodiscard]] NodeType *get() const { return node_; }

  private:
    NodeType *node_;
    Window *root_;
};

template <typename NodeType> void collect_nodes(Node *root, std::vector<NodeType *> &result) {
    if (root == nullptr) {
        return;
    }

    if (auto *typed_node = Object::cast_to<NodeType>(root); typed_node != nullptr) {
        result.push_back(typed_node);
    }

    const int child_count = root->get_child_count();
    for (int child_index = 0; child_index < child_count; ++child_index) {
        collect_nodes(root->get_child(child_index), result);
    }
}

inline std::vector<Label *> collect_labels(Node *root) {
    std::vector<Label *> labels;
    collect_nodes(root, labels);
    return labels;
}

inline std::vector<Button *> collect_buttons(Node *root) {
    std::vector<Button *> buttons;
    collect_nodes(root, buttons);
    return buttons;
}

inline bool has_label_text(Node *root, const String &text) {
    const std::vector<Label *> labels = collect_labels(root);
    return std::ranges::any_of(labels, [&text](const Label *label) { return label->get_text() == text; });
}

inline Label *find_label_by_text(Node *root, const String &text) {
    const std::vector<Label *> labels = collect_labels(root);
    const auto iter = std::ranges::find_if(labels, [&text](const Label *label) { return label->get_text() == text; });
    return iter == labels.end() ? nullptr : *iter;
}

inline bool has_label_containing(Node *root, const String &needle) {
    const std::vector<Label *> labels = collect_labels(root);
    return std::ranges::any_of(labels, [&needle](const Label *label) { return label->get_text().contains(needle); });
}

inline Button *find_button_by_text(Node *root, const String &text) {
    for (auto *button : collect_buttons(root)) {
        if (button->is_visible() && button->get_text() == text) {
            return button;
        }
    }

    return nullptr;
}

/// A card carries its title in a label inside the frame rather than as the button's own text, so it is found
/// by what it says rather than by a property Godot happens to draw.
inline Button *find_card_by_title(Node *root, const String &title) {
    for (auto *button : collect_buttons(root)) {
        if (has_label_text(button, title)) {
            return button;
        }
    }

    return nullptr;
}

inline Callable make_valid_callable(Object *receiver) { return {receiver, "queue_free"}; }

inline UnitConfig make_presenter_unit_config(const std::string &name, int cost) {
    UnitConfig config;
    config.name = name;
    config.cost = cost;
    config.hp = 120;
    config.melee_damage = 0;
    config.ranged_damage = 0;
    config.move_speed_pixels_per_second = 0.0F;
    config.melee_attack_range_variation = {.min = 1.0F, .max = 1.0F};
    config.ranged_attack_range_variation = {.min = 1.0F, .max = 1.0F};
    config.animations.push_back(
        {"walk", {.path_template = "res://assets/Spec_Ops_-_Game_Sprites/png/Soldier4/Climb__%03d.png", .frame_count = 1, .loop = true}});
    return config;
}

inline Node *find_node_named(Node *root, const String &name) {
    if (root == nullptr) {
        return nullptr;
    }
    if (String(root->get_name()) == name) {
        return root;
    }

    const int child_count = root->get_child_count();
    for (int child_index = 0; child_index < child_count; ++child_index) {
        if (Node *found = find_node_named(root->get_child(child_index), name); found != nullptr) {
            return found;
        }
    }

    return nullptr;
}

inline bool has_node_named(Node *root, const String &name) { return find_node_named(root, name) != nullptr; }

inline bool has_all_buttons(Node *root, std::initializer_list<const char *> labels) {
    return std::ranges::all_of(labels, [root](const char *label) { return find_button_by_text(root, String(label)) != nullptr; });
}

inline bool has_all_labels(Node *root, std::initializer_list<const char *> labels) {
    return std::ranges::all_of(labels, [root](const char *label) { return has_label_text(root, String(label)); });
}

inline bool has_all_named_nodes(Node *root, std::initializer_list<const char *> names) {
    return std::ranges::all_of(names, [root](const char *name) { return has_node_named(root, String(name)); });
}

inline bool nearly_equal(double left, double right) { return std::abs(left - right) <= 0.001; }

inline bool color_matches(const godot::Color &actual, const godot::Color &expected) {
    return nearly_equal(actual.r, expected.r) && nearly_equal(actual.g, expected.g) && nearly_equal(actual.b, expected.b) && nearly_equal(actual.a, expected.a);
}

inline bool label_font_color_matches(Node *root, const String &text, const godot::Color &expected_color) {
    Label *label = find_label_by_text(root, text);
    return label != nullptr && label->has_theme_color_override("font_color") && color_matches(label->get_theme_color("font_color"), expected_color);
}

inline bool button_minimum_size_is(Button *button, double width, double height) {
    if (button == nullptr) {
        return false;
    }

    const godot::Vector2 minimum_size = button->get_custom_minimum_size();
    return nearly_equal(minimum_size.x, width) && nearly_equal(minimum_size.y, height);
}

inline bool card_shows_a_tinted_mark(Button *button) {
    std::vector<TextureRect *> marks;
    collect_nodes(button, marks);
    const auto is_icon = [](TextureRect *mark) { return mark->get_name() == StringName("Icon") && mark->get_texture().is_valid(); };
    return std::ranges::any_of(marks, is_icon);
}

inline MenuManager *ready_menu_manager(const TreeMountedNode<MenuManager> &owner) {
    MenuManager *menu_manager = owner.get();
    if (menu_manager->get_node_or_null("UILayer") == nullptr) {
        menu_manager->_ready();
    }
    return menu_manager;
}

inline bool pump_campaign_map_loading(CampaignMapView *campaign_map) {
    if (campaign_map == nullptr) {
        return false;
    }
    for (int attempt = 0; attempt < 10000 && campaign_map->loading_state() != CampaignMapView::LoadingState::Ready &&
                          campaign_map->loading_state() != CampaignMapView::LoadingState::Failed;
         ++attempt) {
        campaign_map->_process(0.016);
        OS::get_singleton()->delay_usec(1000);
    }
    return campaign_map->loading_state() == CampaignMapView::LoadingState::Ready;
}

inline CampaignMapView *find_campaign_map(Node *root) {
    std::vector<CampaignMapView *> campaign_maps;
    collect_nodes(root, campaign_maps);
    return campaign_maps.size() == static_cast<std::size_t>(1) ? campaign_maps.front() : nullptr;
}

inline CampaignMapView *show_campaign_map(const TreeMountedNode<MenuManager> &owner) {
    MenuManager *menu_manager = ready_menu_manager(owner);
    menu_manager->on_button_pressed(static_cast<int>(MenuIntentType::ShowLevelSelect), {});
    CampaignMapView *campaign_map = find_campaign_map(menu_manager);
    (void)pump_campaign_map_loading(campaign_map);
    return campaign_map;
}

inline void check_mobile_control_bounds(Control *control, const godot::Vector2 &available) {
    DEFN_REQUIRE(control != nullptr);
    const godot::Vector2 end = control->get_position() + control->get_size();
    if (end.x > available.x + 1) {
        tests::fail(__FILE__, __LINE__,
                    std::string(String(control->get_name()).utf8().get_data()) + " ends at x=" + std::to_string(end.x) + " beyond " +
                        std::to_string(available.x));
    }
    if (end.y > available.y + 1) {
        tests::fail(__FILE__, __LINE__,
                    std::string(String(control->get_name()).utf8().get_data()) + " ends at " + std::to_string(end.y) + " beyond " +
                        std::to_string(available.y));
    }
}

} // namespace defn::ui_test
#endif
