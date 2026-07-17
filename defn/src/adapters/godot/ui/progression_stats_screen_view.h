#ifndef PROGRESSION_STATS_SCREEN_VIEW_H
#define PROGRESSION_STATS_SCREEN_VIEW_H

#include "progression_models.h"
#include "score_screen_models.h"

#include <godot_cpp/classes/base_button.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/callable.hpp>

#include <string>
#include <vector>

namespace defn {

class UiSfxPlayer;

class ProgressionStatsScreenView : public godot::VBoxContainer {
    GDCLASS(ProgressionStatsScreenView, godot::VBoxContainer)

  public:
    void configure(ProgressionOverviewSnapshot snapshot, std::vector<UpgradeCardViewModel> owned_upgrades, const godot::Callable &back_action,
                   UiSfxPlayer *ui_sfx_player = nullptr);
    void select_entity(const godot::String &entity_id);
    void show_owned_upgrades();
    void show_dossier();
    void go_back();

  protected:
    static void _bind_methods();

  private:
    void rebuild();
    void clear_content();
    void connect_menu_sfx(godot::BaseButton *button) const;
    void on_stat_detail_changed(const godot::String &stat_id, const godot::String &detail, bool active);

    ProgressionOverviewSnapshot snapshot_;
    std::vector<UpgradeCardViewModel> owned_upgrades_;
    std::string selected_entity_id_;
    godot::Callable back_action_;
    UiSfxPlayer *ui_sfx_player_ = nullptr;
    bool showing_all_upgrades_ = false;
    godot::Label *exact_detail_label_ = nullptr;
    godot::String active_stat_id_;
};

} // namespace defn

#endif
