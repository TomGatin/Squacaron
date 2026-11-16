#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <string>
#include <vector>

#include "audio/AudioEvents.hpp"
#include "audio/IAudioBackend.hpp"
#include "audio/NullAudioBackend.hpp"
#include "audio/components/AudioEmitter.hpp"
#include "audio/components/AudioListener.hpp"
#include "audio/settings/AudioSettings.hpp"
#include "audio/spatial/AudioSpatial.hpp"
#include "audio/spatial/SpatialConfig.hpp"
#include "audio/types/AudioConstants.hpp"
#include "audio/types/AudioTypes.hpp"
#include "audio/types/SoundParams.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/Transform.hpp"
#include "events/EventBus.hpp"
#include "systems/audio/AudioSystem.hpp"

namespace {

/** @brief Backend recording every call, so the tests can check what the audio system asked for. */
class FakeAudioBackend final : public sq::audio::IAudioBackend {
  public:
    struct PlayedSound {
        sq::audio::SoundId id = sq::audio::NO_SOUND;
        sq::audio::SoundParams params;
    };

    [[nodiscard]] bool isAvailable() const noexcept override { return true; }
    [[nodiscard]] sq::audio::SoundId loadSound(const std::string& /*path*/) override { return sq::audio::NO_SOUND; }
    [[nodiscard]] sq::audio::MusicId loadMusic(const std::string& /*path*/) override { return sq::audio::NO_MUSIC; }
    void unloadSound(sq::audio::SoundId /*id*/) override {}
    void unloadMusic(sq::audio::MusicId /*id*/) override {}

    sq::audio::VoiceId playSound(sq::audio::SoundId id, const sq::audio::SoundParams& params) override {
        if (_playing.size() >= _voiceLimit) {
            // Like the real backend: the oldest instance loses its voice.
            _stolen.push_back(_playing.front());
            _playing.erase(_playing.begin());
        }
        _sounds.push_back({.id = id, .params = params});
        _calls.emplace_back("playSound");
        _playing.push_back(++_lastVoice);
        return _lastVoice;
    }
    sq::audio::VoiceId playSoundWithoutStealing(sq::audio::SoundId id, const sq::audio::SoundParams& params) override {
        return _playing.size() < _voiceLimit ? playSound(id, params) : sq::audio::NO_VOICE;
    }
    sq::audio::VoiceState updateVoice(sq::audio::VoiceId voice, const sq::audio::SoundParams& params) override {
        if (std::ranges::find(_stolen, voice) != _stolen.end()) {
            return sq::audio::VoiceState::Stolen;
        }
        if (std::ranges::find(_playing, voice) == _playing.end()) {
            return sq::audio::VoiceState::Ended;
        }
        _lastUpdate = params;
        return sq::audio::VoiceState::Playing;
    }
    void stopVoice(sq::audio::VoiceId voice) override {
        if (std::erase(_playing, voice) > 0) {
            _calls.emplace_back("stopVoice");
        }
    }
    void stopAllSounds() override {
        _calls.emplace_back("stopAllSounds");
        _playing.clear();
    }
    void playMusic(sq::audio::MusicId id, bool loop) override {
        _calls.push_back("playMusic " + std::to_string(id) + (loop ? " loop" : " once"));
    }
    void stopMusic() override { _calls.emplace_back("stopMusic"); }
    void setMusicVolume(float volume) override { _musicVolume = volume; }
    void setMasterVolume(float volume) override { _masterVolume = volume; }
    void update() override { ++_updates; }
    [[nodiscard]] std::size_t activeVoiceCount() const override { return 0; }

    [[nodiscard]] const std::vector<PlayedSound>& sounds() const { return _sounds; }
    [[nodiscard]] const std::vector<std::string>& calls() const { return _calls; }
    [[nodiscard]] float masterVolume() const { return _masterVolume; }
    [[nodiscard]] float musicVolume() const { return _musicVolume; }
    [[nodiscard]] int updates() const { return _updates; }
    [[nodiscard]] std::size_t playingCount() const { return _playing.size(); }
    [[nodiscard]] const sq::audio::SoundParams& lastUpdate() const { return _lastUpdate; }

    /** @brief Limits how many instances can play at once; beyond it, the oldest one is stolen. */
    void setVoiceLimit(std::size_t limit) { _voiceLimit = limit; }

    /** @brief Simulates every playing instance reaching its end. */
    void endAllVoices() { _playing.clear(); }

  private:
    std::vector<PlayedSound> _sounds;
    std::vector<std::string> _calls;
    float _masterVolume = -1.0F;
    float _musicVolume = -1.0F;
    int _updates = 0;
    std::vector<sq::audio::VoiceId> _playing;
    std::vector<sq::audio::VoiceId> _stolen;
    std::size_t _voiceLimit = std::numeric_limits<std::size_t>::max();
    sq::audio::VoiceId _lastVoice = sq::audio::NO_VOICE;
    sq::audio::SoundParams _lastUpdate;
};

constexpr sq::audio::SoundId LASER = 1;
constexpr sq::audio::SoundId EXPLOSION = 2;
constexpr sq::audio::MusicId THEME = 1;

/** @brief Audio system wired to a recording backend, with an empty world. */
class AudioSystemTest : public ::testing::Test {
  protected:
    /** @brief Adds a listener entity at the given world position. */
    sq::ecs::Entity spawnListener(float x, float y, bool active = true) {
        const sq::ecs::Entity entity = registry.spawnEntity();
        registry.addComponent<sq::ecs::Transform>(entity, sq::ecs::Transform{.x = x, .y = y});
        registry.addComponent<sq::ecs::AudioListener>(entity, sq::ecs::AudioListener{.active = active});
        return entity;
    }

    /** @brief Adds an entity emitting `sound` at the given world position. */
    sq::ecs::Entity spawnEmitter(float x, float y, sq::audio::SoundId sound, bool loop = true) {
        const sq::ecs::Entity entity = registry.spawnEntity();
        registry.addComponent<sq::ecs::Transform>(entity, sq::ecs::Transform{.x = x, .y = y});
        registry.addComponent<sq::ecs::AudioEmitter>(entity, sq::ecs::AudioEmitter{.sound = sound, .loop = loop});
        return entity;
    }

    FakeAudioBackend backend;
    sq::events::EventBus bus;
    sq::ecs::Registry registry;
    sq::systems::AudioSystem audioSystem{backend, bus};
};

}  // namespace

// ---------------------------------------------------------------------------------------------------------------------
// Spatial computations
// ---------------------------------------------------------------------------------------------------------------------

TEST(AudioSpatial, PanFollowsHorizontalOffset) {
    const sq::audio::SpatialConfig config;

    EXPECT_FLOAT_EQ(sq::audio::computePan(0.0F, config), sq::audio::PAN_CENTER);
    EXPECT_FLOAT_EQ(sq::audio::computePan(config.panRange / 2.0F, config), config.maxPan / 2.0F);
    EXPECT_FLOAT_EQ(sq::audio::computePan(-config.panRange, config), -config.maxPan);
    EXPECT_FLOAT_EQ(sq::audio::computePan(config.panRange * 10.0F, config), config.maxPan) << "pan is capped";
}

TEST(AudioSpatial, FullStrengthPanReachesTheSpeakers) {
    const sq::audio::SpatialConfig config{.maxPan = 1.0F};

    EXPECT_FLOAT_EQ(sq::audio::computePan(config.panRange, config), sq::audio::PAN_RIGHT);
    EXPECT_FLOAT_EQ(sq::audio::computePan(-config.panRange, config), sq::audio::PAN_LEFT);
}

TEST(AudioSpatial, AttenuationFallsLinearlyBetweenMinAndMaxDistance) {
    const sq::audio::SpatialConfig config;
    const float middle = (config.minDistance + config.maxDistance) / 2.0F;

    EXPECT_FLOAT_EQ(sq::audio::computeAttenuation(0.0F, config), sq::audio::MAX_VOLUME);
    EXPECT_FLOAT_EQ(sq::audio::computeAttenuation(config.minDistance, config), sq::audio::MAX_VOLUME);
    EXPECT_FLOAT_EQ(sq::audio::computeAttenuation(middle, config), 0.5F);
    EXPECT_FLOAT_EQ(sq::audio::computeAttenuation(config.maxDistance, config), sq::audio::MIN_VOLUME);
    EXPECT_FLOAT_EQ(sq::audio::computeAttenuation(config.maxDistance * 2.0F, config), sq::audio::MIN_VOLUME);
}

TEST(AudioSpatial, VerticalOffsetAttenuatesWithoutPanning) {
    const sq::audio::SpatialConfig config{.minDistance = 100.0F, .maxDistance = 300.0F};

    const sq::audio::SpatialGains gains =
        sq::audio::computeSpatial({.x = 0.0F, .y = 200.0F}, {.x = 0.0F, .y = 0.0F}, config);

    EXPECT_FLOAT_EQ(gains.pan, sq::audio::PAN_CENTER);
    EXPECT_FLOAT_EQ(gains.volume, 0.5F);
}

TEST(AudioSpatial, InvalidInputsStayFinite) {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const sq::audio::SpatialConfig config;

    const sq::audio::SpatialGains gains =
        sq::audio::computeSpatial({.x = nan, .y = 0.0F}, {.x = 0.0F, .y = 0.0F}, config);
    EXPECT_FLOAT_EQ(gains.volume, sq::audio::MAX_VOLUME) << "a broken position is heard like an unpositioned sound";
    EXPECT_FLOAT_EQ(gains.pan, sq::audio::PAN_CENTER);

    EXPECT_FLOAT_EQ(sq::audio::computePan(100.0F, sq::audio::SpatialConfig{.panRange = 0.0F}), sq::audio::PAN_CENTER);
    EXPECT_FLOAT_EQ(sq::audio::computePan(100.0F, sq::audio::SpatialConfig{.maxPan = nan}),
                    100.0F / sq::audio::DEFAULT_PAN_RANGE * sq::audio::DEFAULT_MAX_PAN);
    EXPECT_FLOAT_EQ(sq::audio::computeAttenuation(500.0F, sq::audio::SpatialConfig{.minDistance = nan}),
                    sq::audio::MAX_VOLUME);

    const sq::audio::SpatialConfig collapsed{.minDistance = 100.0F, .maxDistance = 100.0F};
    EXPECT_FLOAT_EQ(sq::audio::computeAttenuation(100.0F, collapsed), sq::audio::MAX_VOLUME);
    EXPECT_FLOAT_EQ(sq::audio::computeAttenuation(101.0F, collapsed), sq::audio::MIN_VOLUME);
}

// ---------------------------------------------------------------------------------------------------------------------
// Event handling
// ---------------------------------------------------------------------------------------------------------------------

TEST_F(AudioSystemTest, PublishedSoundIsPlayedOnUpdate) {
    bus.publish(sq::events::PlaySoundEvent{.sound = LASER, .volume = 0.8F, .pitch = 1.5F});
    EXPECT_TRUE(backend.sounds().empty()) << "publishing must only queue the sound";

    audioSystem.update(registry);
    ASSERT_EQ(backend.sounds().size(), 1U);
    EXPECT_EQ(backend.sounds()[0].id, LASER);
    EXPECT_FLOAT_EQ(backend.sounds()[0].params.volume, 0.8F);
    EXPECT_FLOAT_EQ(backend.sounds()[0].params.pitch, 1.5F);
    EXPECT_FLOAT_EQ(backend.sounds()[0].params.pan, sq::audio::PAN_CENTER);
}

TEST_F(AudioSystemTest, QueueIsEmptiedByUpdate) {
    bus.publish(sq::events::PlaySoundEvent{.sound = LASER});
    audioSystem.update(registry);
    audioSystem.update(registry);

    EXPECT_EQ(backend.sounds().size(), 1U);
    EXPECT_EQ(backend.updates(), 2);
}

TEST_F(AudioSystemTest, SfxVolumeScalesSounds) {
    audioSystem.setVolume(sq::audio::AudioBus::Sfx, 0.5F);
    bus.publish(sq::events::PlaySoundEvent{.sound = LASER, .volume = 0.8F});
    audioSystem.update(registry);

    ASSERT_EQ(backend.sounds().size(), 1U);
    EXPECT_FLOAT_EQ(backend.sounds()[0].params.volume, 0.4F);
}

TEST_F(AudioSystemTest, MutedSfxBusPlaysNothing) {
    audioSystem.setVolume(sq::audio::AudioBus::Sfx, 0.0F);
    bus.publish(sq::events::PlaySoundEvent{.sound = LASER});
    audioSystem.update(registry);

    EXPECT_TRUE(backend.sounds().empty());
}

TEST_F(AudioSystemTest, VolumeEventsReachTheBackend) {
    bus.publish(sq::events::SetAudioVolumeEvent{.bus = sq::audio::AudioBus::Master, .volume = 0.3F});
    bus.publish(sq::events::SetAudioVolumeEvent{.bus = sq::audio::AudioBus::Music, .volume = 0.6F});
    bus.publish(sq::events::SetAudioVolumeEvent{.bus = sq::audio::AudioBus::Sfx, .volume = 0.9F});
    audioSystem.update(registry);

    EXPECT_FLOAT_EQ(backend.masterVolume(), 0.3F);
    EXPECT_FLOAT_EQ(backend.musicVolume(), 0.6F);
    EXPECT_FLOAT_EQ(audioSystem.volume(sq::audio::AudioBus::Master), 0.3F);
    EXPECT_FLOAT_EQ(audioSystem.volume(sq::audio::AudioBus::Music), 0.6F);
    EXPECT_FLOAT_EQ(audioSystem.volume(sq::audio::AudioBus::Sfx), 0.9F);
}

TEST_F(AudioSystemTest, InvalidVolumesAreClamped) {
    audioSystem.setVolume(sq::audio::AudioBus::Master, std::numeric_limits<float>::quiet_NaN());
    audioSystem.setVolume(sq::audio::AudioBus::Music, -1.0F);
    audioSystem.setVolume(sq::audio::AudioBus::Sfx, 3.0F);

    EXPECT_FLOAT_EQ(audioSystem.volume(sq::audio::AudioBus::Master), sq::audio::DEFAULT_VOLUME);
    EXPECT_FLOAT_EQ(audioSystem.volume(sq::audio::AudioBus::Music), sq::audio::MIN_VOLUME);
    EXPECT_FLOAT_EQ(audioSystem.volume(sq::audio::AudioBus::Sfx), sq::audio::MAX_VOLUME);
}

TEST_F(AudioSystemTest, EventsAreExecutedInArrivalOrderAndSoundsLast) {
    bus.publish(sq::events::PlayMusicEvent{.music = THEME, .loop = false});
    bus.publish(sq::events::PlaySoundEvent{.sound = EXPLOSION});
    bus.publish(sq::events::StopMusicEvent{});
    audioSystem.update(registry);

    const std::vector<std::string> expected{"playMusic 1 once", "stopMusic", "playSound"};
    EXPECT_EQ(backend.calls(), expected);
}

TEST_F(AudioSystemTest, StopAllSoundsDropsEarlierRequestsOfTheSameUpdate) {
    bus.publish(sq::events::PlaySoundEvent{.sound = LASER});
    bus.publish(sq::events::StopAllSoundsEvent{});
    bus.publish(sq::events::PlaySoundEvent{.sound = EXPLOSION});
    audioSystem.update(registry);

    const std::vector<std::string> expected{"stopAllSounds", "playSound"};
    EXPECT_EQ(backend.calls(), expected);
    ASSERT_EQ(backend.sounds().size(), 1U);
    EXPECT_EQ(backend.sounds()[0].id, EXPLOSION);
}

TEST_F(AudioSystemTest, UnpositionedRequestsOfTheSameSoundPlayOnce) {
    bus.publish(sq::events::PlaySoundEvent{.sound = LASER});
    bus.publish(sq::events::PlaySoundEvent{.sound = LASER});
    bus.publish(sq::events::PlaySoundEvent{.sound = EXPLOSION});
    bus.publish(sq::events::PlaySoundEvent{.sound = LASER});
    audioSystem.update(registry);
    EXPECT_EQ(backend.sounds().size(), 2U);

    bus.publish(sq::events::PlaySoundEvent{.sound = LASER});
    audioSystem.update(registry);
    EXPECT_EQ(backend.sounds().size(), 3U);
}

TEST_F(AudioSystemTest, InvalidHandlesAreIgnored) {
    bus.publish(sq::events::PlaySoundEvent{});
    bus.publish(sq::events::PlayMusicEvent{});
    audioSystem.update(registry);

    EXPECT_TRUE(backend.calls().empty());
}

TEST_F(AudioSystemTest, SoundQueueIsBounded) {
    for (std::size_t i = 1; i <= sq::audio::MAX_QUEUED_SOUNDS + 50; ++i) {
        bus.publish(sq::events::PlaySoundEvent{.sound = static_cast<sq::audio::SoundId>(i)});
    }
    bus.publish(sq::events::PlayMusicEvent{.music = THEME});
    audioSystem.update(registry);

    EXPECT_EQ(backend.sounds().size(), sq::audio::MAX_QUEUED_SOUNDS);
    EXPECT_EQ(backend.calls().front(), "playMusic 1 loop") << "non-sound events are never dropped";
}

TEST(AudioSystem, StopsListeningWhenDestroyed) {
    FakeAudioBackend backend;
    sq::events::EventBus bus;
    {
        const sq::systems::AudioSystem audioSystem(backend, bus);
    }

    bus.publish(sq::events::PlaySoundEvent{.sound = LASER});
    bus.publish(sq::events::StopMusicEvent{});
    EXPECT_TRUE(backend.calls().empty());
}

TEST(AudioSystem, WorksWithAMutedBackend) {
    sq::audio::NullAudioBackend backend;
    sq::events::EventBus bus;
    sq::ecs::Registry registry;
    sq::systems::AudioSystem audioSystem(backend, bus);

    bus.publish(
        sq::events::PlaySoundEvent{.sound = LASER, .position = sq::audio::SoundPosition{.x = 10.0F, .y = 0.0F}});
    bus.publish(sq::events::PlayMusicEvent{.music = THEME});
    bus.publish(sq::events::SetAudioVolumeEvent{.bus = sq::audio::AudioBus::Sfx, .volume = 0.5F});
    EXPECT_NO_THROW(audioSystem.update(registry));
}

// ---------------------------------------------------------------------------------------------------------------------
// Positioned sounds
// ---------------------------------------------------------------------------------------------------------------------

TEST_F(AudioSystemTest, PositionedSoundIsPannedTowardsItsSide) {
    spawnListener(640.0F, 360.0F);
    const sq::audio::SpatialConfig& config = audioSystem.spatialConfig();

    bus.publish(
        sq::events::PlaySoundEvent{.sound = LASER, .position = sq::audio::SoundPosition{.x = 960.0F, .y = 360.0F}});
    bus.publish(
        sq::events::PlaySoundEvent{.sound = EXPLOSION, .position = sq::audio::SoundPosition{.x = 0.0F, .y = 360.0F}});
    audioSystem.update(registry);

    ASSERT_EQ(backend.sounds().size(), 2U);
    EXPECT_FLOAT_EQ(backend.sounds()[0].params.pan, 320.0F / config.panRange * config.maxPan);
    EXPECT_FLOAT_EQ(backend.sounds()[0].params.volume, sq::audio::MAX_VOLUME) << "visible sounds keep their volume";
    EXPECT_FLOAT_EQ(backend.sounds()[1].params.pan, -config.maxPan);
}

TEST_F(AudioSystemTest, DistantSoundsFadeAndSilentOnesAreSkipped) {
    spawnListener(0.0F, 0.0F);
    audioSystem.setSpatialConfig({.minDistance = 100.0F, .maxDistance = 300.0F});

    bus.publish(
        sq::events::PlaySoundEvent{.sound = LASER, .volume = 0.8F, .position = sq::audio::SoundPosition{.x = 200.0F}});
    bus.publish(sq::events::PlaySoundEvent{.sound = EXPLOSION, .position = sq::audio::SoundPosition{.x = 400.0F}});
    audioSystem.update(registry);

    ASSERT_EQ(backend.sounds().size(), 1U) << "a silent sound must not take a voice";
    EXPECT_EQ(backend.sounds()[0].id, LASER);
    EXPECT_FLOAT_EQ(backend.sounds()[0].params.volume, 0.4F);
}

TEST_F(AudioSystemTest, CloseRequestsOfTheSameSoundPlayOnceAtTheLoudest) {
    spawnListener(0.0F, 0.0F);
    const float close = audioSystem.spatialConfig().mergeDistance / 2.0F;

    bus.publish(
        sq::events::PlaySoundEvent{.sound = LASER, .volume = 0.4F, .position = sq::audio::SoundPosition{.x = 100.0F}});
    bus.publish(sq::events::PlaySoundEvent{
        .sound = LASER, .volume = 0.8F, .position = sq::audio::SoundPosition{.x = 100.0F + close}});
    audioSystem.update(registry);

    ASSERT_EQ(backend.sounds().size(), 1U);
    EXPECT_FLOAT_EQ(backend.sounds()[0].params.volume, 0.8F);
}

TEST_F(AudioSystemTest, DistantRequestsOfTheSameSoundAreAllHeard) {
    spawnListener(640.0F, 360.0F);

    // Two players firing on the same frame, one on each side of the screen.
    bus.publish(
        sq::events::PlaySoundEvent{.sound = LASER, .position = sq::audio::SoundPosition{.x = 100.0F, .y = 360.0F}});
    bus.publish(
        sq::events::PlaySoundEvent{.sound = LASER, .position = sq::audio::SoundPosition{.x = 1180.0F, .y = 360.0F}});
    audioSystem.update(registry);

    ASSERT_EQ(backend.sounds().size(), 2U);
    EXPECT_LT(backend.sounds()[0].params.pan, sq::audio::PAN_CENTER);
    EXPECT_GT(backend.sounds()[1].params.pan, sq::audio::PAN_CENTER);
}

TEST_F(AudioSystemTest, PositionedAndUnpositionedRequestsAreNotMerged) {
    spawnListener(0.0F, 0.0F);

    bus.publish(sq::events::PlaySoundEvent{.sound = LASER});
    bus.publish(sq::events::PlaySoundEvent{.sound = LASER, .position = sq::audio::SoundPosition{.x = 0.0F}});
    audioSystem.update(registry);

    EXPECT_EQ(backend.sounds().size(), 2U);
}

TEST_F(AudioSystemTest, SfxVolumeChangeAppliesToPlayingSounds) {
    bus.publish(sq::events::PlaySoundEvent{.sound = LASER, .volume = 0.8F});
    audioSystem.update(registry);
    ASSERT_EQ(backend.sounds().size(), 1U);

    audioSystem.setVolume(sq::audio::AudioBus::Sfx, 0.5F);
    EXPECT_FLOAT_EQ(backend.lastUpdate().volume, 0.4F) << "the shot already playing is turned down at once";

    bus.publish(sq::events::SetAudioVolumeEvent{.bus = sq::audio::AudioBus::Sfx, .volume = 0.25F});
    audioSystem.update(registry);
    EXPECT_FLOAT_EQ(backend.lastUpdate().volume, 0.2F);
}

TEST_F(AudioSystemTest, FinishedSoundsAreNoLongerUpdated) {
    bus.publish(sq::events::PlaySoundEvent{.sound = LASER, .volume = 0.8F});
    audioSystem.update(registry);
    audioSystem.setVolume(sq::audio::AudioBus::Sfx, 0.5F);
    backend.endAllVoices();

    audioSystem.setVolume(sq::audio::AudioBus::Sfx, 0.25F);
    EXPECT_FLOAT_EQ(backend.lastUpdate().volume, 0.4F) << "the finished shot was forgotten";
}

TEST_F(AudioSystemTest, SoundsAreNotSpatializedWithoutListener) {
    bus.publish(sq::events::PlaySoundEvent{.sound = LASER, .position = sq::audio::SoundPosition{.x = 5000.0F}});
    audioSystem.update(registry);

    ASSERT_EQ(backend.sounds().size(), 1U);
    EXPECT_FLOAT_EQ(backend.sounds()[0].params.volume, sq::audio::MAX_VOLUME);
    EXPECT_FLOAT_EQ(backend.sounds()[0].params.pan, sq::audio::PAN_CENTER);
}

TEST_F(AudioSystemTest, InactiveListenerIsIgnored) {
    spawnListener(5000.0F, 0.0F, false);
    spawnListener(0.0F, 0.0F);

    bus.publish(sq::events::PlaySoundEvent{.sound = LASER, .position = sq::audio::SoundPosition{.x = 100.0F}});
    audioSystem.update(registry);

    ASSERT_EQ(backend.sounds().size(), 1U);
    EXPECT_FLOAT_EQ(backend.sounds()[0].params.volume, sq::audio::MAX_VOLUME);
    EXPECT_GT(backend.sounds()[0].params.pan, sq::audio::PAN_CENTER);
}

TEST_F(AudioSystemTest, DisabledSpatializationPlaysPositionedSoundsCentered) {
    spawnListener(0.0F, 0.0F);
    audioSystem.setSpatialConfig({.enabled = false});

    bus.publish(sq::events::PlaySoundEvent{.sound = LASER, .position = sq::audio::SoundPosition{.x = 5000.0F}});
    audioSystem.update(registry);

    ASSERT_EQ(backend.sounds().size(), 1U) << "a far sound is not skipped when spatialization is off";
    EXPECT_FLOAT_EQ(backend.sounds()[0].params.volume, sq::audio::MAX_VOLUME);
    EXPECT_FLOAT_EQ(backend.sounds()[0].params.pan, sq::audio::PAN_CENTER);
}

TEST_F(AudioSystemTest, SettingsApplyVolumesAndSpatialization) {
    audioSystem.applySettings(sq::audio::AudioSettings{
        .spatial = {.enabled = false}, .masterVolume = 0.2F, .musicVolume = 0.4F, .sfxVolume = 0.6F});

    EXPECT_FLOAT_EQ(backend.masterVolume(), 0.2F);
    EXPECT_FLOAT_EQ(backend.musicVolume(), 0.4F);
    EXPECT_FLOAT_EQ(audioSystem.volume(sq::audio::AudioBus::Sfx), 0.6F);
    EXPECT_FALSE(audioSystem.spatialConfig().enabled);
}

TEST_F(AudioSystemTest, UnpositionedSoundIgnoresTheListener) {
    spawnListener(5000.0F, 0.0F);

    bus.publish(sq::events::PlaySoundEvent{.sound = LASER});
    audioSystem.update(registry);

    ASSERT_EQ(backend.sounds().size(), 1U);
    EXPECT_FLOAT_EQ(backend.sounds()[0].params.volume, sq::audio::MAX_VOLUME);
    EXPECT_FLOAT_EQ(backend.sounds()[0].params.pan, sq::audio::PAN_CENTER);
}

// ---------------------------------------------------------------------------------------------------------------------
// Emitters
// ---------------------------------------------------------------------------------------------------------------------

TEST_F(AudioSystemTest, EmitterSoundFollowsItsEntity) {
    spawnListener(640.0F, 360.0F);
    const sq::ecs::Entity ship = spawnEmitter(0.0F, 360.0F, LASER);
    const sq::audio::SpatialConfig& config = audioSystem.spatialConfig();

    audioSystem.update(registry);
    ASSERT_EQ(backend.sounds().size(), 1U);
    EXPECT_FLOAT_EQ(backend.sounds()[0].params.pan, -config.maxPan);

    registry.getComponents<sq::ecs::Transform>().get(ship).x = 1280.0F;
    audioSystem.update(registry);
    EXPECT_EQ(backend.sounds().size(), 1U) << "the playing instance is moved, not restarted";
    EXPECT_FLOAT_EQ(backend.lastUpdate().pan, config.maxPan);
}

TEST_F(AudioSystemTest, LoopingEmitterRestartsWhenItsSoundEnds) {
    spawnEmitter(0.0F, 0.0F, LASER);

    audioSystem.update(registry);
    backend.endAllVoices();
    audioSystem.update(registry);

    EXPECT_EQ(backend.sounds().size(), 2U);
}

TEST_F(AudioSystemTest, NonLoopingEmitterPlaysOnce) {
    spawnEmitter(0.0F, 0.0F, LASER, false);

    audioSystem.update(registry);
    backend.endAllVoices();
    audioSystem.update(registry);

    EXPECT_EQ(backend.sounds().size(), 1U);
}

TEST_F(AudioSystemTest, EmitterStopsWithItsEntity) {
    const sq::ecs::Entity ship = spawnEmitter(0.0F, 0.0F, LASER);
    audioSystem.update(registry);
    ASSERT_EQ(backend.playingCount(), 1U);

    registry.killEntity(ship);
    audioSystem.update(registry);

    EXPECT_EQ(backend.playingCount(), 0U);
    EXPECT_EQ(backend.calls().back(), "stopVoice");
}

TEST_F(AudioSystemTest, EmitterCanBePausedAndResumed) {
    const sq::ecs::Entity ship = spawnEmitter(0.0F, 0.0F, LASER, false);
    audioSystem.update(registry);

    registry.getComponents<sq::ecs::AudioEmitter>().get(ship).playing = false;
    audioSystem.update(registry);
    EXPECT_EQ(backend.playingCount(), 0U);

    registry.getComponents<sq::ecs::AudioEmitter>().get(ship).playing = true;
    audioSystem.update(registry);
    EXPECT_EQ(backend.playingCount(), 1U);
    EXPECT_EQ(backend.sounds().size(), 2U);
}

TEST_F(AudioSystemTest, InaudibleEmitterStartsWhenItComesCloser) {
    spawnListener(0.0F, 0.0F);
    audioSystem.setSpatialConfig({.minDistance = 100.0F, .maxDistance = 300.0F});
    const sq::ecs::Entity ship = spawnEmitter(1000.0F, 0.0F, LASER);

    audioSystem.update(registry);
    EXPECT_TRUE(backend.sounds().empty());

    registry.getComponents<sq::ecs::Transform>().get(ship).x = 50.0F;
    audioSystem.update(registry);
    EXPECT_EQ(backend.sounds().size(), 1U);
}

TEST_F(AudioSystemTest, LoopingEmittersDoNotStealEachOtherBack) {
    backend.setVoiceLimit(4);
    std::vector<sq::ecs::Entity> ships;
    for (int i = 0; i < 5; ++i) {
        ships.push_back(spawnEmitter(0.0F, 0.0F, LASER));
    }

    audioSystem.update(registry);
    ASSERT_EQ(backend.sounds().size(), 5U) << "the fifth emitter took the voice of the first";

    for (int frame = 0; frame < 10; ++frame) {
        audioSystem.update(registry);
    }
    EXPECT_EQ(backend.sounds().size(), 5U) << "no emitter may restart while every voice is in use";
    EXPECT_EQ(backend.playingCount(), 4U);

    // The destroyed emitter frees its voice at the end of this update; the stolen one takes it on the next.
    registry.killEntity(ships.back());
    audioSystem.update(registry);
    audioSystem.update(registry);
    EXPECT_EQ(backend.sounds().size(), 6U) << "the stolen emitter resumes once a voice is free";
    EXPECT_EQ(backend.playingCount(), 4U);
}

TEST(AudioSystem, StopsEmittersWhenDestroyed) {
    FakeAudioBackend backend;
    sq::events::EventBus bus;
    sq::ecs::Registry registry;
    const sq::ecs::Entity ship = registry.spawnEntity();
    registry.addComponent<sq::ecs::Transform>(ship, sq::ecs::Transform{});
    registry.addComponent<sq::ecs::AudioEmitter>(ship, sq::ecs::AudioEmitter{.sound = LASER});
    {
        sq::systems::AudioSystem audioSystem(backend, bus);
        audioSystem.update(registry);
        ASSERT_EQ(backend.playingCount(), 1U);
    }
    EXPECT_EQ(backend.playingCount(), 0U);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
