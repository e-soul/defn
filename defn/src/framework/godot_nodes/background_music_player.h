#ifndef BACKGROUND_MUSIC_PLAYER_H
#define BACKGROUND_MUSIC_PLAYER_H

#include "music_playlist.h"
#include "random_source.h"

#include <cstddef>
#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/timer.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/string.hpp>
#include <vector>

namespace defn {

class BackgroundMusicPlayer : public godot::Node {
    GDCLASS(BackgroundMusicPlayer, godot::Node)

  public:
    void configure(const MusicPlaylist &playlist);
    void _ready() override;

  protected:
    static void _bind_methods();

  private:
    void shuffle_tracks();
    void play_next_track();
    void on_track_finished();

    StdRandomSource random_;
    std::vector<godot::String> tracks_;
    std::size_t next_track_index_ = 0;
    godot::String last_played_track_;
    double volume_linear_ = 1.0;
    double delay_seconds_ = 2.0;
    godot::AudioStreamPlayer *player_ = nullptr;
    godot::Timer *gap_timer_ = nullptr;
};

} // namespace defn

#endif
