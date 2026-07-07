#ifndef WAVE_MANAGER_H
#define WAVE_MANAGER_H

#include "level_definition.h"
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>
#include <optional>
#include <string>
#include <vector>

namespace defn {
class UnitDataLoader;
}

namespace defn {

using namespace godot;

class WaveManager : public Node {
    GDCLASS(WaveManager, Node)

  public:
    WaveManager();

    void _ready() override;
    void _process(double delta) override;

    void set_unit_data(const UnitDataLoader *loader) { unit_data_ = loader; }
    void load_level(const String &path);
    void start();
    void stop();

    int get_current_wave() const { return current_wave; }
    int get_total_waves() const { return level_definition_ ? static_cast<int>(level_definition_->waves.size()) : 0; }
    int get_starting_core_resource() const { return level_definition_ ? level_definition_->starting_core_resource : 100; }
    int get_base_integrity() const { return level_definition_ ? level_definition_->base_integrity : 3; }
    String get_background_path() const { return level_definition_ ? String(level_definition_->background_path.c_str()) : String(); }

    bool all_waves_spawned() const;

  protected:
    static void _bind_methods();

  private:
    std::optional<LevelDefinition> level_definition_;
    double level_timer = 0.0;
    int current_wave = 0; // 0 = not started
    bool running = false;
    bool completion_emitted_ = false;

    // Flattened list of all spawns with absolute times, for simpler processing
    struct FlatSpawn {
        double time;
        std::string type;
        int wave; // which wave this belongs to
    };
    std::vector<FlatSpawn> all_spawns;
    size_t next_spawn_idx = 0;

    const UnitDataLoader *unit_data_ = nullptr;
};

} // namespace defn

#endif
