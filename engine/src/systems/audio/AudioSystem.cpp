#include "systems/audio/AudioSystem.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>
#include <variant>

#include "audio/AudioEvents.hpp"
#include "audio/AudioMiddleware.hpp"
#include "audio/IAudioBackend.hpp"
#include "audio/components/AudioEmitter.hpp"
#include "audio/components/AudioListener.hpp"
#include "audio/settings/AudioSettings.hpp"
#include "audio/spatial/AudioSpatial.hpp"
#include "audio/types/AudioConstants.hpp"
#include "audio/types/AudioTypes.hpp"
#include "audio/types/SoundParams.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/Transform.hpp"
#include "events/EventBus.hpp"
#include "utils/MoveOnlyFunction.hpp"

namespace sq::systems {

namespace {

/** @brief Subscribes `callback` to `EventType` and returns the function that undoes it. */
template <typename EventType, typename Callback>
sq::ecs::MoveOnlyFunction<void()> subscribe(sq::events::EventBus& eventBus, Callback callback) {
    const sq::events::EventBus::EventToken token = eventBus.subscribe<EventType>(std::move(callback));
    return [&eventBus, token] { eventBus.unsubscribe<EventType>(token); };
}

/** @brief Returns the position of the first active `AudioListener`, or nothing if there is none. */
std::optional<sq::audio::SoundPosition> findListener(sq::ecs::Registry& registry) {
    std::optional<sq::audio::SoundPosition> listener;
    registry.runSystem<sq::ecs::AudioListener, sq::ecs::Transform>(
        [&listener](sq::ecs::Entity /*entity*/, const sq::ecs::AudioListener& audioListener,
                    const sq::ecs::Transform& transform) {
            if (!listener && audioListener.active) {
                listener = sq::audio::SoundPosition{.x = transform.x, .y = transform.y};
            }
        });
    return listener;
}

}  // namespace

AudioSystem::AudioSystem(sq::audio::IAudioBackend& backend, sq::events::EventBus& eventBus) : _backend(&backend) {
    // Listeners only queue: playback happens in update(), never inside the publisher's call.
    const auto queue = [this](const auto& event) { enqueue(event); };
    _unsubscribers.push_back(subscribe<sq::events::PlaySoundEvent>(eventBus, queue));
    _unsubscribers.push_back(subscribe<sq::events::PlayMusicEvent>(eventBus, queue));
    _unsubscribers.push_back(subscribe<sq::events::StopMusicEvent>(eventBus, queue));
    _unsubscribers.push_back(subscribe<sq::events::StopAllSoundsEvent>(eventBus, queue));
    _unsubscribers.push_back(subscribe<sq::events::SetAudioVolumeEvent>(eventBus, queue));

    _backend->setMasterVolume(_masterVolume);
    _backend->setMusicVolume(_musicVolume);
}

AudioSystem::~AudioSystem() {
    for (auto& unsubscribe : _unsubscribers) {
        unsubscribe();
    }
    for (const auto& [entity, state] : _emitters) {
        _backend->stopVoice(state.voice);
    }
}

void AudioSystem::update(sq::ecs::Registry& registry) {
    _listener = findListener(registry);
    for (const AudioCommand& command : _queue) {
        std::visit([this](const auto& event) { execute(event); }, command);
    }
    _queue.clear();
    _queuedSounds = 0;

    refreshPlayingSounds();
    for (const PendingSound& sound : _pendingSounds) {
        const sq::audio::VoiceId voice = _backend->playSound(sound.id, applySfxBus(sound.base));
        if (voice != sq::audio::NO_VOICE) {
            _playingSounds.push_back({.voice = voice, .base = sound.base});
        }
    }
    _pendingSounds.clear();

    updateEmitters(registry);
    _backend->update();
}

void AudioSystem::setVolume(sq::audio::AudioBus bus, float volume) {
    const float gain =
        sq::audio::clampFinite(volume, sq::audio::DEFAULT_VOLUME, sq::audio::MIN_VOLUME, sq::audio::MAX_VOLUME);
    switch (bus) {
        case sq::audio::AudioBus::Master:
            _masterVolume = gain;
            _backend->setMasterVolume(gain);
            break;
        case sq::audio::AudioBus::Music:
            _musicVolume = gain;
            _backend->setMusicVolume(gain);
            break;
        case sq::audio::AudioBus::Sfx:
            // Emitters pick the new gain up on the next update; one-shot sounds already playing are updated now.
            _sfxVolume = gain;
            refreshPlayingSounds();
            break;
    }
}

void AudioSystem::applySettings(const sq::audio::AudioSettings& settings) {
    setVolume(sq::audio::AudioBus::Master, settings.masterVolume);
    setVolume(sq::audio::AudioBus::Music, settings.musicVolume);
    setVolume(sq::audio::AudioBus::Sfx, settings.sfxVolume);
    setSpatialConfig(settings.spatial);
}

float AudioSystem::volume(sq::audio::AudioBus bus) const noexcept {
    switch (bus) {
        case sq::audio::AudioBus::Master:
            return _masterVolume;
        case sq::audio::AudioBus::Music:
            return _musicVolume;
        case sq::audio::AudioBus::Sfx:
            return _sfxVolume;
    }
    return sq::audio::DEFAULT_VOLUME;
}

void AudioSystem::enqueue(const sq::events::PlaySoundEvent& event) {
    if (event.sound == sq::audio::NO_SOUND || _queuedSounds >= sq::audio::MAX_QUEUED_SOUNDS) {
        return;
    }
    _queue.emplace_back(event);
    ++_queuedSounds;
}

void AudioSystem::enqueue(const AudioCommand& command) { _queue.push_back(command); }

sq::audio::SoundParams AudioSystem::resolveParams(float volume, float pitch,
                                                  const std::optional<sq::audio::SoundPosition>& position) const {
    const float gain =
        sq::audio::clampFinite(volume, sq::audio::DEFAULT_VOLUME, sq::audio::MIN_VOLUME, sq::audio::MAX_VOLUME);
    sq::audio::SoundParams params{.volume = gain, .pitch = pitch};
    if (const std::optional<sq::audio::SoundPosition> where = heardFrom(position); where && _listener) {
        const sq::audio::SpatialGains gains = sq::audio::computeSpatial(*where, *_listener, _spatial);
        params.volume *= gains.volume;
        params.pan = gains.pan;
    }
    return params;
}

sq::audio::SoundParams AudioSystem::applySfxBus(sq::audio::SoundParams base) const noexcept {
    base.volume *= _sfxVolume;
    return base;
}

std::optional<sq::audio::SoundPosition> AudioSystem::heardFrom(
    const std::optional<sq::audio::SoundPosition>& position) const {
    return position && _listener && _spatial.enabled ? position : std::nullopt;
}

void AudioSystem::refreshPlayingSounds() {
    std::erase_if(_playingSounds, [this](const PlayingSound& sound) {
        return _backend->updateVoice(sound.voice, applySfxBus(sound.base)) != sq::audio::VoiceState::Playing;
    });
}

void AudioSystem::updateEmitters(sq::ecs::Registry& registry) {
    for (auto& [entity, state] : _emitters) {
        state.seen = false;
    }

    registry.runSystem<sq::ecs::AudioEmitter, sq::ecs::Transform>(
        [this](sq::ecs::Entity entity, const sq::ecs::AudioEmitter& emitter, const sq::ecs::Transform& transform) {
            EmitterState& state = _emitters[entity];
            state.seen = true;
            const bool active = emitter.playing && emitter.sound != sq::audio::NO_SOUND;
            if (!active || emitter.sound != state.sound) {
                _backend->stopVoice(state.voice);
                state = EmitterState{.sound = emitter.sound, .seen = true};
                if (!active) {
                    return;
                }
            }

            const sq::audio::SoundParams params = applySfxBus(resolveParams(
                emitter.volume, emitter.pitch, sq::audio::SoundPosition{.x = transform.x, .y = transform.y}));
            // Still playing: move it with the entity.
            const sq::audio::VoiceState voice = _backend->updateVoice(state.voice, params);
            if (voice == sq::audio::VoiceState::Playing) {
                return;
            }
            if (voice == sq::audio::VoiceState::Stolen) {
                state.stolen = true;
            }
            // Ended, stolen by another sound, or not started yet. An inaudible emitter waits to come closer.
            if ((state.started && !emitter.loop) || params.volume <= sq::audio::MIN_VOLUME) {
                return;
            }
            // A stolen emitter must not steal back, or emitters competing for too few voices would restart each
            // other on every update. It waits for a free voice instead.
            if (state.stolen) {
                state.voice = _backend->playSoundWithoutStealing(emitter.sound, params);
                state.stolen = state.voice == sq::audio::NO_VOICE;
            } else {
                state.voice = _backend->playSound(emitter.sound, params);
            }
            state.started = true;
        });

    // The entity was destroyed or lost its emitter: silence it.
    std::erase_if(_emitters, [this](const auto& entry) {
        if (entry.second.seen) {
            return false;
        }
        _backend->stopVoice(entry.second.voice);
        return true;
    });
}

void AudioSystem::execute(const sq::events::PlaySoundEvent& event) {
    const sq::audio::SoundParams base = resolveParams(event.volume, event.pitch, event.position);
    // An inaudible sound (too far, or muted bus) would only take a voice from an audible one.
    if (applySfxBus(base).volume <= sq::audio::MIN_VOLUME) {
        return;
    }

    // The same sound started at the same place in one frame would only sound louder and waste a voice: keep the
    // loudest. Requests from different places (two players firing) are all played, each from its own side.
    const std::optional<sq::audio::SoundPosition> where = heardFrom(event.position);
    const auto same = std::ranges::find_if(_pendingSounds, [&](const PendingSound& pending) {
        if (pending.id != event.sound || pending.position.has_value() != where.has_value()) {
            return false;
        }
        return !where ||
               std::hypot(pending.position->x - where->x, pending.position->y - where->y) <= _spatial.mergeDistance;
    });
    if (same == _pendingSounds.end()) {
        _pendingSounds.push_back({.id = event.sound, .base = base, .position = where});
    } else if (base.volume > same->base.volume) {
        same->base = base;
        same->position = where;
    }
}

void AudioSystem::execute(const sq::events::PlayMusicEvent& event) {
    if (event.music != sq::audio::NO_MUSIC) {
        _backend->playMusic(event.music, event.loop);
    }
}

void AudioSystem::execute(const sq::events::StopMusicEvent& /*event*/) { _backend->stopMusic(); }

void AudioSystem::execute(const sq::events::StopAllSoundsEvent& /*event*/) {
    _backend->stopAllSounds();
    _pendingSounds.clear();
    _playingSounds.clear();
}

void AudioSystem::execute(const sq::events::SetAudioVolumeEvent& event) { setVolume(event.bus, event.volume); }

}  // namespace sq::systems
