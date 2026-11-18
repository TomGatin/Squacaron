#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "assets/AssetKinds.hpp"
#include "assets/AssetManager.hpp"
#include "assets/AssetPlugin.hpp"
#include "assets/audio/AudioAssetsPlugin.hpp"
#include "audio/AudioPlugin.hpp"
#include "audio/IAudioBackend.hpp"
#include "audio/NullAudioBackend.hpp"
#include "audio/types/AudioConstants.hpp"
#include "audio/types/AudioTypes.hpp"
#include "audio/types/SoundParams.hpp"
#include "core/Engine.hpp"
#include "ecs/Registry.hpp"

namespace {

/** @brief Backend that hands out handles (giving `preferred` back) and records what it loads and releases. */
class RecordingBackend final : public sq::audio::IAudioBackend {
  public:
    /** @brief What the backend did, shared with the test after the plugin owns the backend. */
    struct Log {
        std::vector<std::string> loads;
        std::vector<sq::audio::SoundId> unloadedSounds;
        std::vector<sq::audio::MusicId> unloadedMusics;
        std::vector<sq::audio::SoundId> preferredSounds;
    };

    explicit RecordingBackend(std::shared_ptr<Log> log) : _log(std::move(log)) {}

    [[nodiscard]] bool isAvailable() const noexcept override { return true; }
    [[nodiscard]] sq::audio::SoundId loadSound(const std::string& path) override { return loadSoundAs(path, 0); }
    [[nodiscard]] sq::audio::MusicId loadMusic(const std::string& path) override {
        _log->loads.push_back(path);
        return static_cast<sq::audio::MusicId>(_log->loads.size());
    }
    [[nodiscard]] sq::audio::SoundId loadSoundAs(const std::string& path, sq::audio::SoundId preferred) override {
        _log->loads.push_back(path);
        _log->preferredSounds.push_back(preferred);
        return preferred != sq::audio::NO_SOUND ? preferred : static_cast<sq::audio::SoundId>(_log->loads.size());
    }
    void unloadSound(sq::audio::SoundId id) override { _log->unloadedSounds.push_back(id); }
    void unloadMusic(sq::audio::MusicId id) override { _log->unloadedMusics.push_back(id); }
    sq::audio::VoiceId playSound(sq::audio::SoundId /*id*/, const sq::audio::SoundParams& /*params*/) override {
        return sq::audio::NO_VOICE;
    }
    sq::audio::VoiceId playSoundWithoutStealing(sq::audio::SoundId /*id*/,
                                                const sq::audio::SoundParams& /*params*/) override {
        return sq::audio::NO_VOICE;
    }
    sq::audio::VoiceState updateVoice(sq::audio::VoiceId /*voice*/, const sq::audio::SoundParams& /*params*/) override {
        return sq::audio::VoiceState::Ended;
    }
    void stopVoice(sq::audio::VoiceId /*voice*/) override {}
    void stopAllSounds() override {}
    void playMusic(sq::audio::MusicId /*id*/, bool /*loop*/) override {}
    void stopMusic() override {}
    void setMusicVolume(float /*volume*/) override {}
    void setMasterVolume(float /*volume*/) override {}
    void update() override {}
    [[nodiscard]] std::size_t activeVoiceCount() const override { return 0; }

  private:
    std::shared_ptr<Log> _log;
};

}  // namespace

TEST(AudioAssetsPlugin, PlugsTheAudioLoadersIntoTheAssetManagerAndReleasesThemAtShutdown) {
    auto log = std::make_shared<RecordingBackend::Log>();
    sq::engine::Engine engine;
    engine.addPlugin<sq::audio::AudioPlugin>(std::make_unique<RecordingBackend>(log))
        .addPlugin<sq::assets::AssetPlugin>()
        .addPlugin<sq::assets::AudioAssetsPlugin>();
    engine.init();

    auto& manager = engine.registry().getResource<sq::assets::AssetManager>();
    ASSERT_TRUE(manager.hasLoader<sq::assets::Sound>());
    ASSERT_TRUE(manager.hasLoader<sq::assets::Music>());
    const sq::audio::SoundId jump = manager.load<sq::assets::Sound>("sounds/jump.wav");
    EXPECT_EQ(manager.load<sq::assets::Sound>("sounds/jump.wav"), jump);
    const sq::audio::MusicId theme = manager.load<sq::assets::Music>("music/theme.ogg");
    EXPECT_EQ(log->loads, (std::vector<std::string>{"sounds/jump.wav", "music/theme.ogg"}));

    engine.shutdown();
    EXPECT_EQ(log->unloadedSounds, std::vector<sq::audio::SoundId>{jump});
    EXPECT_EQ(log->unloadedMusics, std::vector<sq::audio::MusicId>{theme});
}

TEST(AudioAssetsPlugin, ASoundLoadedAgainKeepsItsHandle) {
    auto log = std::make_shared<RecordingBackend::Log>();
    sq::engine::Engine engine;
    engine.addPlugin<sq::audio::AudioPlugin>(std::make_unique<RecordingBackend>(log))
        .addPlugin<sq::assets::AssetPlugin>()
        .addPlugin<sq::assets::AudioAssetsPlugin>();
    engine.init();

    auto& manager = engine.registry().getResource<sq::assets::AssetManager>();
    manager.declare<sq::assets::Sound>("boom", "sounds/boom.wav", "level1");
    manager.loadGroup("level1");
    const sq::audio::SoundId boom = manager.get<sq::assets::Sound>("boom");
    manager.unloadGroup("level1");
    manager.loadGroup("level1");
    EXPECT_EQ(manager.get<sq::assets::Sound>("boom"), boom);
    EXPECT_EQ(log->preferredSounds.back(), boom);
    engine.shutdown();
}

TEST(AudioAssetsPlugin, AMutedBackendInstallsNoLoader) {
    sq::engine::Engine engine;
    engine.addPlugin<sq::audio::AudioPlugin>(std::make_unique<sq::audio::NullAudioBackend>())
        .addPlugin<sq::assets::AssetPlugin>()
        .addPlugin<sq::assets::AudioAssetsPlugin>();
    engine.init();

    auto& manager = engine.registry().getResource<sq::assets::AssetManager>();
    EXPECT_FALSE(engine.getPlugin<sq::assets::AudioAssetsPlugin>().attached());
    EXPECT_FALSE(manager.hasLoader<sq::assets::Sound>());
    manager.declare<sq::assets::Sound>("jump", "sounds/jump.wav");
    manager.loadGroup("global");
    // Waiting for a loader, not failed: no warning per sound.
    EXPECT_EQ(manager.status<sq::assets::Sound>("jump"), sq::assets::AssetStatus::Declared);
    EXPECT_TRUE(manager.warnings().empty());
    engine.shutdown();
}

TEST(AudioAssetsPlugin, TheAudioPluginRunsWithoutTheAssetManager) {
    sq::engine::Engine engine;
    engine.addPlugin<sq::audio::AudioPlugin>(std::make_unique<sq::audio::NullAudioBackend>());
    engine.init();
    EXPECT_FALSE(engine.registry().hasResource<sq::assets::AssetManager>());
    engine.shutdown();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
