// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause
#ifndef RESPONSIVE_UI_ROOT_H
#define RESPONSIVE_UI_ROOT_H
#include "responsive_layout.h"
#include <godot_cpp/classes/control.hpp>
namespace defn {
class ResponsiveUiRoot : public godot::Control {
    GDCLASS(ResponsiveUiRoot, godot::Control)
  public:
    void _ready() override;
    void _process(double delta) override;
    void refresh_display();
    void apply_snapshot(const DisplaySnapshot &next);
    void invalidate_layout() { dirty_ = true; }
    [[nodiscard]] const UiContext &ui_context() const { return context_; }
    void notify_content(godot::Node *node);

  protected:
    static void _bind_methods() {}
    virtual void apply_layout() {}
    DisplaySnapshot display_;

  private:
    void request_layout();
    UiContext context_;
    std::size_t theme_revision_ = 0;
    double poll_seconds_ = 0;
    bool dirty_ = true;
    bool initial_ = true;
};
} // namespace defn
#endif
