// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "content_startup_validator.h"

#include "content_repository.h"
#include "content_validator.h"
#include "godot_string.h"

#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <optional>

namespace defn {

bool ContentStartupValidator::report_startup_validation() {
    static std::optional<bool> cached_result;
    if (cached_result.has_value()) {
        return *cached_result;
    }

    const JsonContentRepository repository(default_json_content_paths());
    const JsonLoadedContent loaded_content = repository.load_for_validation();

    std::vector<String> issues = loaded_content.load_issues;
    const auto &rules = loaded_content.unit_data.get_globals().gameplay_rules;
    auto *settings = godot::ProjectSettings::get_singleton();
    if (static_cast<float>(settings->get_setting("display/window/size/viewport_width")) != rules.viewport_width ||
        static_cast<float>(settings->get_setting("display/window/size/viewport_height")) != rules.viewport_height) {
        issues.emplace_back("Project reference viewport must match unit_globals gameplay viewport dimensions");
    }
    const ContentValidationReport report = ContentValidator::validate_loaded_content(make_content_validation_input(loaded_content));
    for (const std::string &issue : report.issues) {
        issues.push_back(to_godot_string(issue));
    }

    if (issues.empty()) {
        UtilityFunctions::print("ContentValidator: content validation passed");
        cached_result = true;
        return true;
    }

    UtilityFunctions::printerr("ContentValidator: content validation found ", static_cast<int>(issues.size()), " issue(s)");
    for (const String &issue : issues) {
        UtilityFunctions::printerr("  - ", issue);
    }

    cached_result = false;
    return false;
}

} // namespace defn
