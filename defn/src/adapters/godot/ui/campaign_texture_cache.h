#ifndef CAMPAIGN_TEXTURE_CACHE_H
#define CAMPAIGN_TEXTURE_CACHE_H

#include "campaign_map_definition.h"

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/core/class_db.hpp>

#include <string>
#include <unordered_map>

namespace defn {

class CampaignTextureCache : public godot::RefCounted {
    GDCLASS(CampaignTextureCache, godot::RefCounted)

  public:
    [[nodiscard]] godot::Ref<godot::Texture2D> load(const CampaignTextureDefinition &definition);
    void clear();

  protected:
    static void _bind_methods();

  private:
    std::unordered_map<std::string, godot::Ref<godot::Texture2D>> textures_;
};

} // namespace defn

#endif
