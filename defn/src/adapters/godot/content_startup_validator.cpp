#include "content_startup_validator.h"

#include "content_repository.h"
#include "content_validator.h"

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
    const ContentValidationReport report =
        ContentValidator::validate_loaded_content(loaded_content.menu_data, loaded_content.progression_loaded ? &loaded_content.progression_catalog : nullptr,
                                                  loaded_content.upgrades_loaded ? &loaded_content.upgrade_catalog : nullptr,
                                                  loaded_content.units_loaded ? &loaded_content.unit_data : nullptr, loaded_content.levels);
    issues.insert(issues.end(), report.issues.begin(), report.issues.end());

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