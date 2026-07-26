// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef MENU_INTENTS_H
#define MENU_INTENTS_H

#include <string>

namespace defn {

enum class MenuIntentType { None, GotoMenu, ShowLevelSelect, ShowProgression, StartGame, Quit, Resume, MainMenu };

struct MenuIntent {
    MenuIntentType type = MenuIntentType::None;
    std::string target;
};

} // namespace defn

#endif