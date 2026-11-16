#include "assets/audio/AudioAssetsPlugin.hpp"

#include <string>

#include "assets/AssetKinds.hpp"
#include "assets/AssetManager.hpp"
#include "assets/AssetPlugin.hpp"
#include "audio/AudioPlugin.hpp"
#include "audio/IAudioBackend.hpp"
#include "core/AudioHandles.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Registry.hpp"

namespace sq::assets {

void AudioAssetsPlugin::build(sq::engine::PluginBuilder& builder) {
    builder.require<sq::audio::AudioPlugin>();
    builder.require<AssetPlugin>();
}

void AudioAssetsPlugin::init(sq::engine::Engine& engine) {
    sq::audio::IAudioBackend* backend = engine.getPlugin<sq::audio::AudioPlugin>().backend();
    if (backend == nullptr || !backend->isAvailable()) {
        return;
    }
    auto& manager = engine.registry().getResource<AssetManager>();
    manager.setLoader<Sound>(AssetLoader<Sound>{
        .load = [backend](const std::string& path, const LoadOptions& /*options*/,
                          const sq::audio::SoundId preferred) { return backend->loadSoundAs(path, preferred); },
        .unload = [backend](const sq::audio::SoundId id) { backend->unloadSound(id); },
        .fallback = {},
        .reserve = [backend](const sq::audio::SoundId last) { backend->reserveSounds(last); },
    });
    manager.setLoader<Music>(AssetLoader<Music>{
        .load = [backend](const std::string& path, const LoadOptions& /*options*/,
                          const sq::audio::MusicId preferred) { return backend->loadMusicAs(path, preferred); },
        .unload = [backend](const sq::audio::MusicId id) { backend->unloadMusic(id); },
        .fallback = {},
        .reserve = [backend](const sq::audio::MusicId last) { backend->reserveMusics(last); },
    });
    _attached = true;
}

void AudioAssetsPlugin::shutdown(sq::engine::Engine& engine) {
    if (!_attached) {
        return;
    }
    _attached = false;
    if (!engine.registry().hasResource<AssetManager>()) {
        return;
    }
    auto& manager = engine.registry().getResource<AssetManager>();
    manager.detachLoader<Sound>();
    manager.detachLoader<Music>();
}

}  // namespace sq::assets
