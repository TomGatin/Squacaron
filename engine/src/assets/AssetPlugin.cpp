#include "assets/AssetPlugin.hpp"

#include "assets/AssetManager.hpp"
#include "core/Engine.hpp"
#include "ecs/Registry.hpp"

namespace sq::assets {

void AssetPlugin::init(sq::engine::Engine& engine) {
    sq::ecs::Registry& registry = engine.registry();
    if (!registry.hasResource<AssetManager>()) {
        registry.emplaceResource<AssetManager>(_root);
    }
}

void AssetPlugin::shutdown(sq::engine::Engine& engine) {
    sq::ecs::Registry& registry = engine.registry();
    if (registry.hasResource<AssetManager>()) {
        registry.getResource<AssetManager>().unloadAll();
        registry.removeResource<AssetManager>();
    }
}

}  // namespace sq::assets
