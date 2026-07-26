// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef MUSIC_PLAYLIST_H
#define MUSIC_PLAYLIST_H

#include <string>
#include <vector>

namespace defn {

struct MusicPlaylist {
    std::vector<std::string> tracks;
    double volume_linear = 1.0;
    double delay_seconds = 2.0;
};

} // namespace defn

#endif
