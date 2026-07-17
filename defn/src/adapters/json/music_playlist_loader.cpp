#include "music_playlist_loader.h"

#include "godot_string.h"
#include "json_file_loader.h"
#include "variant_tools.h"

#include <cmath>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

std::optional<MusicPlaylist> MusicPlaylistLoader::load(const godot::String &path) {
    const auto data = JsonFileLoader::load_dictionary(path, "MusicPlaylistLoader");
    return data ? load_from_data(*data) : std::nullopt;
}

std::optional<MusicPlaylist> MusicPlaylistLoader::load_from_data(const godot::Dictionary &data) {
    MusicPlaylist playlist;
    playlist.volume_linear = VariantTools::as_double(data.get("volume_linear", playlist.volume_linear));
    playlist.delay_seconds = VariantTools::as_double(data.get("delay_seconds", playlist.delay_seconds));

    if (!std::isfinite(playlist.volume_linear) || playlist.volume_linear < 0.0 || playlist.volume_linear > 1.0) {
        godot::UtilityFunctions::printerr("MusicPlaylistLoader: volume_linear must be between 0 and 1");
        return std::nullopt;
    }
    if (!std::isfinite(playlist.delay_seconds) || playlist.delay_seconds < 0.0) {
        godot::UtilityFunctions::printerr("MusicPlaylistLoader: delay_seconds must be zero or greater");
        return std::nullopt;
    }

    const godot::Array tracks = data.get("tracks", godot::Array());
    playlist.tracks.reserve(tracks.size());
    for (const godot::Variant &track_value : tracks) {
        if (track_value.get_type() != godot::Variant::STRING) {
            godot::UtilityFunctions::printerr("MusicPlaylistLoader: every track must be a resource path string");
            return std::nullopt;
        }
        const godot::String track_path = track_value;
        if (track_path.is_empty()) {
            godot::UtilityFunctions::printerr("MusicPlaylistLoader: track paths must not be empty");
            return std::nullopt;
        }
        playlist.tracks.push_back(to_std_string(track_path));
    }

    if (playlist.tracks.empty()) {
        godot::UtilityFunctions::printerr("MusicPlaylistLoader: tracks must contain at least one path");
        return std::nullopt;
    }

    return playlist;
}

} // namespace defn
