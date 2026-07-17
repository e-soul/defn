#include "background_music_player.h"

#include "godot_string.h"

#include <algorithm>
#include <cmath>
#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/audio_stream_mp3.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

void BackgroundMusicPlayer::_bind_methods() {}

void BackgroundMusicPlayer::configure(const MusicPlaylist &playlist) {
    tracks_.clear();
    tracks_.reserve(playlist.tracks.size());
    for (const std::string &track_path : playlist.tracks) {
        tracks_.push_back(to_godot_string(track_path));
    }
    volume_linear_ = playlist.volume_linear;
    delay_seconds_ = playlist.delay_seconds;
}

void BackgroundMusicPlayer::_ready() {
    player_ = memnew(godot::AudioStreamPlayer);
    player_->set_name("MusicStreamPlayer");
    const double audible_volume = std::max(volume_linear_, 0.0001);
    player_->set_volume_db(static_cast<float>(20.0 * std::log10(audible_volume)));
    player_->connect("finished", callable_mp(this, &BackgroundMusicPlayer::on_track_finished));
    add_child(player_);

    gap_timer_ = memnew(godot::Timer);
    gap_timer_->set_name("MusicGapTimer");
    gap_timer_->set_one_shot(true);
    gap_timer_->set_wait_time(std::max(delay_seconds_, 0.001));
    gap_timer_->connect("timeout", callable_mp(this, &BackgroundMusicPlayer::play_next_track));
    add_child(gap_timer_);

    shuffle_tracks();
    play_next_track();
}

void BackgroundMusicPlayer::shuffle_tracks() {
    for (std::size_t index = tracks_.size(); index > 1; --index) {
        const auto swap_index = static_cast<std::size_t>(random_.range_int(0, static_cast<int>(index - 1)));
        std::swap(tracks_[index - 1], tracks_[swap_index]);
    }

    if (tracks_.size() > 1 && tracks_.front() == last_played_track_) {
        std::swap(tracks_.front(), tracks_[1]);
    }
    next_track_index_ = 0;
}

void BackgroundMusicPlayer::play_next_track() {
    auto *loader = godot::ResourceLoader::get_singleton();
    if (player_ == nullptr || loader == nullptr || tracks_.empty()) {
        return;
    }

    const std::size_t attempt_count = tracks_.size();
    for (std::size_t attempt = 0; attempt < attempt_count; ++attempt) {
        if (next_track_index_ >= tracks_.size()) {
            shuffle_tracks();
        }

        const godot::String track_path = tracks_[next_track_index_++];
        godot::Ref<godot::AudioStream> stream = loader->load(track_path);
        if (!stream.is_valid()) {
            godot::UtilityFunctions::printerr("BackgroundMusicPlayer: Failed to load track: ", track_path);
            continue;
        }

        godot::Ref<godot::AudioStreamMP3> mp3_stream = stream;
        if (mp3_stream.is_valid()) {
            mp3_stream->set_loop(false);
        }

        last_played_track_ = track_path;
        player_->set_stream(stream);
        player_->play();
        return;
    }

    godot::UtilityFunctions::printerr("BackgroundMusicPlayer: No playable music tracks were found");
}

void BackgroundMusicPlayer::on_track_finished() {
    if (delay_seconds_ <= 0.0) {
        play_next_track();
        return;
    }
    if (gap_timer_ != nullptr) {
        gap_timer_->start();
    }
}

} // namespace defn
