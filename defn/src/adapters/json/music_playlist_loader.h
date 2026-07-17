#ifndef MUSIC_PLAYLIST_LOADER_H
#define MUSIC_PLAYLIST_LOADER_H

#include "music_playlist.h"

#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

#include <optional>

namespace defn {

class MusicPlaylistLoader {
  public:
    MusicPlaylistLoader() = delete;

    static std::optional<MusicPlaylist> load(const godot::String &path);
    static std::optional<MusicPlaylist> load_from_data(const godot::Dictionary &data);
};

} // namespace defn

#endif
