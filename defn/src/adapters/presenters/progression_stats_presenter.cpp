#include "progression_stats_presenter.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <ranges>
#include <sstream>

namespace defn {

namespace {

std::string humanize(std::string value) {
    bool capitalize = true;
    for (char &character : value) {
        if (character == '_') {
            character = ' ';
            capitalize = true;
        } else if (capitalize) {
            character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
            capitalize = false;
        }
    }
    return value;
}

std::string concise_number(double value) {
    if (std::fabs(value - std::round(value)) < 0.0001) {
        return std::to_string(std::llround(value));
    }
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(1) << value;
    return stream.str();
}

std::string signed_number(double value) { return (value >= 0.0 ? "+" : "") + concise_number(value); }

std::string label_for_stat(const std::string &stat_id) {
    if (stat_id == "health") {
        return "Health";
    }
    if (stat_id == "firepower") {
        return "Firepower";
    }
    if (stat_id == "mobility") {
        return "Mobility";
    }
    if (stat_id == "deploy_cost") {
        return "Energy cost";
    }
    if (stat_id == "fire_rate") {
        return "Fire rate";
    }
    if (stat_id == "integrity_bonus") {
        return "Integrity bonus";
    }
    if (stat_id == "starting_energy_bonus") {
        return "Starting energy bonus";
    }
    if (stat_id == "energy_generation") {
        return "Energy generation";
    }
    if (stat_id == "bounty_bonus") {
        return "Bounty bonus";
    }
    return humanize(stat_id);
}

std::string with_detail_unit(const std::string &stat_id, const std::string &value) {
    if (stat_id == "health") {
        return value + " HP";
    }
    if (stat_id == "firepower") {
        return value + " damage";
    }
    if (stat_id == "mobility") {
        return value + " px/s";
    }
    if (stat_id == "deploy_cost" || stat_id == "starting_energy_bonus") {
        return value + " energy";
    }
    if (stat_id == "integrity_bonus") {
        return value + " integrity";
    }
    return value;
}

ProgressionStatRowViewModel present_stat(const ProgressionStatValue &stat) {
    ProgressionStatRowViewModel result{.id = stat.id, .label = label_for_stat(stat.id)};
    if (stat.contribution_only) {
        result.value = stat.id == "bounty_bonus" ? signed_number(stat.contribution * 100.0) + "%" : signed_number(stat.contribution);
        result.visual = make_progression_stat_visual(stat.id, 0.0, stat.contribution, result.value, true);
        result.visual.exact_detail = result.label + ": " + with_detail_unit(stat.id, result.value) + " from owned upgrades";
        return result;
    }

    result.value = concise_number(stat.effective_value);
    if (stat.id == "energy_generation" || stat.id == "fire_rate") {
        result.value += "/s";
    }
    const double delta = stat.effective_value - stat.base_value;
    if (std::fabs(delta) >= 0.0001) {
        result.detail = concise_number(stat.base_value) + " " + signed_number(delta);
    }
    result.visual = make_progression_stat_visual(stat.id, stat.base_value, stat.effective_value, result.value, false);
    result.visual.exact_detail = result.label + ": " + with_detail_unit(stat.id, result.value);
    if (std::fabs(delta) >= 0.0001) {
        result.visual.exact_detail += " (base " + concise_number(stat.base_value) + ", " + signed_number(delta) + " upgrade)";
    }
    if (stat.id == "deploy_cost") {
        result.visual.exact_detail += ". Fewer cells means a cheaper deployment";
    }
    return result;
}

const ProgressionEntitySnapshot *find_entity(const ProgressionOverviewSnapshot &snapshot, const std::string &entity_id) {
    const auto found = std::ranges::find(snapshot.entities, entity_id, &ProgressionEntitySnapshot::id);
    return found == snapshot.entities.end() ? nullptr : &*found;
}

} // namespace

std::string ProgressionStatsPresenter::default_selection(const ProgressionOverviewSnapshot &snapshot) {
    const auto deployable =
        std::ranges::find_if(snapshot.entities, [](const auto &entity) { return entity.kind == ProgressionEntityKind::UNIT && entity.unlocked; });
    if (deployable != snapshot.entities.end()) {
        return deployable->id;
    }
    const auto base = std::ranges::find_if(snapshot.entities, [](const auto &entity) { return entity.kind == ProgressionEntityKind::BASE && entity.unlocked; });
    if (base != snapshot.entities.end()) {
        return base->id;
    }
    const auto operations =
        std::ranges::find_if(snapshot.entities, [](const auto &entity) { return entity.kind == ProgressionEntityKind::OPERATIONS && entity.unlocked; });
    return operations == snapshot.entities.end() ? std::string() : operations->id;
}

ProgressionStatsScreenViewModel ProgressionStatsPresenter::present(const ProgressionOverviewSnapshot &snapshot, const std::string &selected_entity_id) {
    ProgressionStatsScreenViewModel result;
    std::string selection = selected_entity_id;
    const ProgressionEntitySnapshot *selected = find_entity(snapshot, selection);
    if (selected == nullptr || !selected->unlocked) {
        selection = default_selection(snapshot);
        selected = find_entity(snapshot, selection);
    }

    for (const auto &entity : snapshot.entities) {
        result.selectors.push_back(
            {.id = entity.id,
             .label = humanize(entity.id) + (entity.unlocked ? "" : " [Locked]"),
             .portrait_path_template = entity.portrait_path_template,
             .locked_message = entity.unlocked || entity.unlock_upgrade_name.empty() ? std::string() : "Unlock with " + entity.unlock_upgrade_name,
             .unlocked = entity.unlocked,
             .selected = entity.id == selection});
    }
    result.selected_entity_id = selection;
    if (selected == nullptr) {
        result.title = "Command Roster";
        result.empty_upgrade_message = "No progression entities are available.";
        return result;
    }

    result.title = humanize(selected->id);
    result.description = selected->description;
    result.portrait_path_template = selected->portrait_path_template;
    result.locked = !selected->unlocked;
    if (result.locked) {
        result.locked_message = selected->unlock_upgrade_name.empty() ? "Locked" : "Unlock with " + selected->unlock_upgrade_name;
    }
    for (size_t index = 0; index < std::min<size_t>(selected->stats.size(), 4); ++index) {
        result.stats.push_back(present_stat(selected->stats[index]));
    }
    for (const auto &source : selected->contributing_upgrades) {
        const auto &presentation = source.presentation;
        std::string label = presentation.name.empty() ? humanize(presentation.id) : presentation.name;
        if (source.owned_count > 1) {
            label += " x" + std::to_string(source.owned_count);
        }
        result.upgrades.push_back({.id = presentation.id, .label = label, .description = presentation.description, .emoji = presentation.emoji});
    }
    if (result.upgrades.empty()) {
        result.empty_upgrade_message = "No owned upgrades affect this selection.";
    }
    return result;
}

} // namespace defn
