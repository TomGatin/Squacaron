#pragma once

#include <optional>

#include "audio/types/AudioConstants.hpp"
#include "audio/types/AudioTypes.hpp"

namespace sq::events {

/**
 * @brief Requests a sound effect.
 *
 * Published by gameplay code, which never touches the audio library: the audio system plays it on its next update.
 * Requesting the same sound several times before that update plays only the loudest request.
 */
struct PlaySoundEvent {
    /** @brief Sound to play; `NO_SOUND` is ignored. */
    sq::audio::SoundId sound = sq::audio::NO_SOUND;

    /** @brief Gain of this instance, multiplied by the SFX bus volume. */
    float volume = sq::audio::DEFAULT_VOLUME;

    /** @brief Playback speed factor (1.0F keeps the original pitch). */
    float pitch = sq::audio::DEFAULT_PITCH;

    /**
     * @brief World position of the emitter, to pan and attenuate the sound around the `AudioListener`.
     *
     * Leave it empty for sounds that have no place in the world (UI, the player's own feedback).
     */
    std::optional<sq::audio::SoundPosition> position;
};

/** @brief Requests a music stream, replacing the current one. */
struct PlayMusicEvent {
    /** @brief Music to play; `NO_MUSIC` is ignored. */
    sq::audio::MusicId music = sq::audio::NO_MUSIC;

    /** @brief Whether the music restarts when it reaches its end. */
    bool loop = true;
};

/** @brief Requests the current music stream to stop. */
struct StopMusicEvent {};

/** @brief Requests every playing sound effect to stop (pause menu, scene change, ...). */
struct StopAllSoundsEvent {};

/** @brief Changes the gain of a volume bus (options menu, console, ...). */
struct SetAudioVolumeEvent {
    /** @brief Bus to change. */
    sq::audio::AudioBus bus = sq::audio::AudioBus::Master;

    /** @brief New linear gain in [`MIN_VOLUME`, `MAX_VOLUME`]. */
    float volume = sq::audio::DEFAULT_VOLUME;
};

}  // namespace sq::events
