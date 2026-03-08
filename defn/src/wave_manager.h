#ifndef WAVE_MANAGER_H
#define WAVE_MANAGER_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <vector>

namespace defn {

using namespace godot;

struct SpawnEvent {
    double time = 0.0;
    int lane = 1;
    String type = "grunt";
};

struct WaveData {
    int wave_number = 1;
    std::vector<SpawnEvent> spawns;
};

class WaveManager : public Node {
    GDCLASS(WaveManager, Node)

public:
    WaveManager();

    void _ready() override;
    void _process(double delta) override;

    void load_level(const String &path);
    void start();
    void stop();

    int get_current_wave() const { return current_wave; }
    int get_total_waves() const { return static_cast<int>(waves.size()); }
    int get_starting_aether() const { return starting_aether; }
    int get_base_integrity() const { return base_integrity_max; }

    bool all_waves_spawned() const;

protected:
    static void _bind_methods();

private:
    std::vector<WaveData> waves;
    double level_timer = 0.0;
    int current_wave = 0; // 0 = not started
    int spawn_index = 0;  // index into current wave's spawns
    bool running = false;
    int starting_aether = 100;
    int base_integrity_max = 3;

    // Flattened list of all spawns with absolute times, for simpler processing
    struct FlatSpawn {
        double time;
        int lane;
        String type;
        int wave; // which wave this belongs to
    };
    std::vector<FlatSpawn> all_spawns;
    size_t next_spawn_idx = 0;
};

} // namespace defn

#endif
