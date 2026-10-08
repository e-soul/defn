// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause
#ifndef SCORE_SCREEN_VIEW_H
#define SCORE_SCREEN_VIEW_H

#include "score_pagination.h"
#include "score_screen_view_model.h"
#include "ui_widgets.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/rect2.hpp>

#include <array>
#include <vector>

namespace defn {
using namespace godot;

struct ScoreScreenActions {
    Callable on_next_level;
    Callable on_endless;
    Callable on_retry;
    Callable on_campaign;
    Callable on_select_upgrade;
};

struct ScoreScreenViewNodes {
    Control *overlay = nullptr;
    PanelContainer *panel = nullptr;
};

/// Desktop and mobile pages render the existing score presentation. Choosing an upgrade still goes through
/// the supplied application action; only a successful model update returns the player to Results.
class ScoreScreenView : public UiContextControl {
    GDCLASS(ScoreScreenView, UiContextControl)

  public:
    enum class Layout { Mobile, Desktop };
    static ScoreScreenViewNodes show(godot::Node *parent, const ScoreScreenModel &model, const ScoreScreenActions &actions, const UiContext &context = {});
    void configure(const ScoreScreenModel &model, const ScoreScreenActions &actions, Layout layout = Layout::Mobile);
    void _process(double delta) override;
    void layout_in_rect(const godot::Rect2 &usable);
    [[nodiscard]] godot::PanelContainer *panel() const { return panel_; }

  protected:
    static void _bind_methods() {}
    void context_changed() override;

  private:
    enum class Page { Results, Rewards, Owned };
    struct Action {
        std::string id;
        godot::String text;
        godot::Callable pressed;
        bool enabled = true;
        bool primary = false;
    };
    struct ResultRow {
        godot::String name;
        godot::String value;
        bool notice = false;
        bool hint = false;
    };
    using ResultLine = MeasuredScoreLine;

    void open_page(int page);
    void turn_page(int direction);
    void select_upgrade(const godot::String &upgrade_id);
    void clear_content();
    void reset_footer_buttons();
    void restore_focus(const godot::String &identity);
    void layout_desktop(const godot::Rect2 &usable);
    [[nodiscard]] float desktop_body_height(float width, bool wide) const;
    [[nodiscard]] float metric(std::string_view suffix, int fallback) const;
    [[nodiscard]] float result_gap() const;
    [[nodiscard]] godot::String upgrade_name(const UpgradeCardViewModel &card) const;
    float build_header(float width, bool landscape);
    float build_result_header(float width, bool landscape);
    float build_totals(const std::vector<std::pair<godot::String, godot::String>> &totals, const godot::Rect2 &rect, bool inline_totals);
    float build_actions(float width, float bottom, const std::vector<Action> &actions);
    void build_results(float width, float top, float bottom, bool landscape);
    [[nodiscard]] std::vector<ResultRow> result_rows() const;
    [[nodiscard]] std::vector<ResultLine> result_lines(const std::vector<ResultRow> &rows, float width, size_t columns) const;
    void paginate_lines(const std::vector<ResultLine> &lines, float available);
    void draw_result_row(const ResultRow &row, const godot::Rect2 &rect);
    void build_upgrades(float width, float top, float bottom, bool landscape);
    float build_pager(float width, float bottom);
    [[nodiscard]] std::vector<Action> result_actions();
    [[nodiscard]] float text_width(const godot::String &text, int size) const;
    [[nodiscard]] float text_height(const godot::String &text, float width, int size) const;
    static godot::Label *add_text(godot::Node *parent, const godot::String &text, const godot::Rect2 &rect, int size, std::string_view color = "text_primary",
                                  bool right = false);
    godot::Button *add_button(godot::Node *parent, const Action &action, std::string_view variant = "secondary");
    [[nodiscard]] float card_height(const UpgradeCardViewModel &card, float width, bool vertical, bool owned = false) const;
    void add_upgrade_card(const UpgradeCardViewModel &card, const godot::Rect2 &rect, bool vertical, bool owned);

    ScoreScreenModel model_;
    ScoreScreenViewModel presentation_;
    ScoreScreenActions actions_;
    godot::PanelContainer *panel_ = nullptr;
    godot::Control *content_ = nullptr;
    godot::Control *footer_ = nullptr;
    godot::Control *pager_ = nullptr;
    godot::ColorRect *footer_rule_ = nullptr;
    std::map<std::string, godot::Button *, std::less<>> action_buttons_;
    std::map<std::string, godot::Callable, std::less<>> action_callbacks_;
    godot::Rect2 last_rect_;
    Page page_ = Page::Results;
    // Keep the viewed item, rather than a page number that changes when orientation changes.
    std::array<size_t, 3> anchors_{};
    std::vector<size_t> page_starts_;
    size_t current_page_ = 0;
    int body_size_ = 16;
    int supporting_size_ = 14;
    float gap_ = 8;
    float target_ = 48;
    bool dirty_ = true;
    Layout layout_ = Layout::Mobile;
    bool compact_desktop_ = false;
    std::map<std::string, int, std::less<>> style_;
    void resolve_style();
    void resized();
    float desktop_footer_bottom_ = 0;
};

} // namespace defn
#endif
