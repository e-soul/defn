// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef CAMPAIGN_MAP_DATA_LOADER_H
#define CAMPAIGN_MAP_DATA_LOADER_H

#include "campaign_map_definition.h"

#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

#include <optional>

namespace defn {

class CampaignMapDataLoader {
  public:
    CampaignMapDataLoader() = delete;

    [[nodiscard]] static std::optional<CampaignMapDefinition> load(const godot::String &path);
    [[nodiscard]] static std::optional<CampaignMapDefinition> load_from_data(const godot::Dictionary &data);
};

[[nodiscard]] CampaignMapAmbience parse_campaign_ambience(const godot::String &value);
[[nodiscard]] std::string campaign_ambience_id(CampaignMapAmbience ambience);

} // namespace defn

#endif
