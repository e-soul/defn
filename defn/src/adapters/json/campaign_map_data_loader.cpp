#include "campaign_map_data_loader.h"

#include "godot_string.h"
#include "json_file_loader.h"
#include "variant_tools.h"

#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

using namespace godot;

namespace {

float as_float(const Variant &value, float invalid_value) {
    if (value.get_type() != Variant::FLOAT && value.get_type() != Variant::INT) {
        return invalid_value;
    }
    return static_cast<float>(value);
}

Vector2 parse_vector2(const Variant &value, Vector2 invalid_value) {
    if (value.get_type() != Variant::ARRAY) {
        return invalid_value;
    }
    const Array values = value;
    if (values.size() < 2) {
        return invalid_value;
    }
    return {.x = as_float(values[0], invalid_value.x), .y = as_float(values[1], invalid_value.y)};
}

CampaignTextureDefinition parse_texture(const Dictionary &data) {
    return {
        .path = to_std_string(String(data.get("path", ""))),
        .texture_scale = as_float(data.get("texture_scale", 0.0F), 0.0F),
    };
}

CampaignPreviewDefinition parse_preview(const Dictionary &data) {
    const Vector2 focus = parse_vector2(data.get("focus", Array()), {.x = -1.0F, .y = -1.0F});
    return {
        .texture = parse_texture(data),
        .focus_x = focus.x,
        .focus_y = focus.y,
        .node_zoom = as_float(data.get("node_zoom", 0.0F), 0.0F),
        .dossier_zoom = as_float(data.get("dossier_zoom", 0.0F), 0.0F),
    };
}

} // namespace

CampaignMapAmbience parse_campaign_ambience(const String &value) {
    const String normalized = value.to_lower();
    if (normalized == "dust") {
        return CampaignMapAmbience::DUST;
    }
    if (normalized == "spores") {
        return CampaignMapAmbience::SPORES;
    }
    if (normalized == "mist") {
        return CampaignMapAmbience::MIST;
    }
    if (normalized == "snow") {
        return CampaignMapAmbience::SNOW;
    }
    if (normalized == "embers") {
        return CampaignMapAmbience::EMBERS;
    }
    return CampaignMapAmbience::UNKNOWN;
}

std::string campaign_ambience_id(CampaignMapAmbience ambience) {
    switch (ambience) {
    case CampaignMapAmbience::DUST:
        return "dust";
    case CampaignMapAmbience::SPORES:
        return "spores";
    case CampaignMapAmbience::MIST:
        return "mist";
    case CampaignMapAmbience::SNOW:
        return "snow";
    case CampaignMapAmbience::EMBERS:
        return "embers";
    case CampaignMapAmbience::UNKNOWN:
        return "unknown";
    }
    return "unknown";
}

std::optional<CampaignMapDefinition> CampaignMapDataLoader::load(const String &path) {
    const auto data = JsonFileLoader::load_dictionary(path, "CampaignMapDataLoader");
    if (!data.has_value()) {
        return std::nullopt;
    }
    return load_from_data(*data);
}

std::optional<CampaignMapDefinition> CampaignMapDataLoader::load_from_data(const Dictionary &data) {
    CampaignMapDefinition result;
    const Dictionary background = data.get("background", Dictionary());
    result.background = parse_texture(background);

    const Array missions = data.get("missions", Array());
    result.missions.reserve(missions.size());
    for (const Variant &mission_value : missions) {
        if (mission_value.get_type() != Variant::DICTIONARY) {
            continue;
        }
        const Dictionary mission_data = mission_value;
        CampaignMapMissionDefinition mission;
        mission.level_id = to_std_string(String(mission_data.get("level_id", "")));
        mission.position_normalized = parse_vector2(mission_data.get("position", Array()), {});
        mission.tagline = to_std_string(String(mission_data.get("tagline", "")));
        mission.threat_id = to_std_string(String(mission_data.get("threat", "")).to_lower());
        mission.ambience = parse_campaign_ambience(String(mission_data.get("ambience", "")));
        const Dictionary preview = mission_data.get("preview", Dictionary());
        mission.preview = parse_preview(preview);
        result.missions.push_back(std::move(mission));
    }

    return result;
}

} // namespace defn
