#include "campaign_texture_cache.h"

#include "godot_string.h"

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#include <cmath>

namespace defn {

using namespace godot;

void CampaignTextureCache::_bind_methods() {}

Ref<Texture2D> CampaignTextureCache::load(const CampaignTextureDefinition &definition) {
    if (definition.path.empty() || definition.texture_scale <= 0.0F || definition.texture_scale > 1.0F) {
        return {};
    }

    Ref<Texture2D> imported = ResourceLoader::get_singleton()->load(to_godot_string(definition.path), "Texture2D", ResourceLoader::CACHE_MODE_IGNORE);
    if (!imported.is_valid()) {
        UtilityFunctions::printerr("CampaignTextureCache: Failed to load ", to_godot_string(definition.path));
        return {};
    }

    const int source_width = imported->get_width();
    const int source_height = imported->get_height();
    const int target_width = std::max(1, static_cast<int>(std::lround(static_cast<double>(source_width) * definition.texture_scale)));
    const int target_height = std::max(1, static_cast<int>(std::lround(static_cast<double>(source_height) * definition.texture_scale)));
    const std::string cache_key = definition.path + "@" + std::to_string(target_width) + "x" + std::to_string(target_height);
    if (const auto found = textures_.find(cache_key); found != textures_.end()) {
        return found->second;
    }

    if (target_width == source_width && target_height == source_height) {
        textures_.emplace(cache_key, imported);
        return imported;
    }

    Ref<Image> image = imported->get_image();
    if (!image.is_valid() || image->is_empty()) {
        UtilityFunctions::printerr("CampaignTextureCache: Texture has no readable image: ", to_godot_string(definition.path));
        return {};
    }
    image->resize(target_width, target_height, Image::INTERPOLATE_LANCZOS);
    image->generate_mipmaps();
    Ref<ImageTexture> prepared = ImageTexture::create_from_image(image);
    if (!prepared.is_valid()) {
        return {};
    }
    textures_.emplace(cache_key, prepared);
    return prepared;
}

void CampaignTextureCache::clear() { textures_.clear(); }

} // namespace defn
