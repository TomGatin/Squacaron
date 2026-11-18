#include <gtest/gtest.h>

#include <cstddef>
#include <memory>

#include "audio/AudioPlugin.hpp"
#include "audio/IAudioService.hpp"
#include "audio/NullAudioBackend.hpp"
#include "audio/settings/AudioSettings.hpp"
#include "audio/types/AudioTypes.hpp"
#include "core/Engine.hpp"

namespace {

/** @brief Settings with distinct bus volumes, so the tests can check they were applied. */
sq::audio::AudioSettings customSettings() {
    sq::audio::AudioSettings settings;
    settings.masterVolume = 0.5F;
    settings.musicVolume = 0.25F;
    settings.sfxVolume = 0.75F;
    return settings;
}

}  // namespace

TEST(AudioPlugin, RegistersTheAudioServiceAtInit) {
    sq::engine::Engine engine;
    engine.addPlugin<sq::audio::AudioPlugin>(std::make_unique<sq::audio::NullAudioBackend>());

    EXPECT_FALSE(engine.mediator().hasService<sq::engine::IAudioService>());
    engine.init();

    EXPECT_TRUE(engine.mediator().hasService<sq::engine::IAudioService>());
    EXPECT_NE(engine.getPlugin<sq::audio::AudioPlugin>().audioSystem(), nullptr);
}

TEST(AudioPlugin, AppliesSettingsAtInit) {
    sq::engine::Engine engine;
    engine.addPlugin<sq::audio::AudioPlugin>(std::make_unique<sq::audio::NullAudioBackend>(), customSettings());
    engine.init();

    const auto& service = engine.service<sq::engine::IAudioService>();
    EXPECT_FLOAT_EQ(service.volume(sq::audio::AudioBus::Master), 0.5F);
    EXPECT_FLOAT_EQ(service.volume(sq::audio::AudioBus::Music), 0.25F);
    EXPECT_FLOAT_EQ(service.volume(sq::audio::AudioBus::Sfx), 0.75F);
}

TEST(AudioPlugin, ServiceReportsAMutedBackend) {
    sq::engine::Engine engine;
    engine.addPlugin<sq::audio::AudioPlugin>(std::make_unique<sq::audio::NullAudioBackend>());
    engine.init();

    auto& service = engine.service<sq::engine::IAudioService>();
    EXPECT_FALSE(service.isAvailable());
    EXPECT_EQ(service.loadSound("missing.wav"), sq::audio::NO_SOUND);
    EXPECT_EQ(service.loadMusic("missing.ogg"), sq::audio::NO_MUSIC);
    EXPECT_EQ(service.activeVoiceCount(), 0U);
}

TEST(AudioPlugin, NullBackendIsReplacedByAMutedOne) {
    sq::engine::Engine engine;
    engine.addPlugin<sq::audio::AudioPlugin>(std::unique_ptr<sq::audio::IAudioBackend>{});

    EXPECT_NE(engine.getPlugin<sq::audio::AudioPlugin>().backend(), nullptr);
    EXPECT_NO_THROW(engine.init());
}

TEST(AudioPlugin, StepsWithoutThrowing) {
    double now = 0.0;
    sq::engine::Engine engine([&now]() -> double { return now; });
    engine.addPlugin<sq::audio::AudioPlugin>(std::make_unique<sq::audio::NullAudioBackend>());
    engine.init();

    now = static_cast<double>(engine.fixedDeltaTime()) * 1.5;
    std::size_t steps = 0;
    EXPECT_NO_THROW(steps = engine.advance());
    EXPECT_EQ(steps, 1U);
}

TEST(AudioPlugin, ShutdownUnregistersTheServiceAndKeepsTheInjectedBackend) {
    sq::engine::Engine engine;
    engine.addPlugin<sq::audio::AudioPlugin>(std::make_unique<sq::audio::NullAudioBackend>());
    engine.init();
    engine.shutdown();

    auto& plugin = engine.getPlugin<sq::audio::AudioPlugin>();
    EXPECT_FALSE(engine.mediator().hasService<sq::engine::IAudioService>());
    EXPECT_EQ(plugin.audioSystem(), nullptr);
    EXPECT_NE(plugin.backend(), nullptr);
}

TEST(AudioPlugin, CanBeInitializedAgainAfterShutdown) {
    sq::engine::Engine engine;
    engine.addPlugin<sq::audio::AudioPlugin>(std::make_unique<sq::audio::NullAudioBackend>());
    engine.init();
    engine.shutdown();

    // An engine does not restart after shutdown(), but the plugin itself can be initialized again.
    auto& plugin = engine.getPlugin<sq::audio::AudioPlugin>();
    EXPECT_NO_THROW(plugin.init(engine));
    EXPECT_TRUE(engine.mediator().hasService<sq::engine::IAudioService>());
    plugin.shutdown(engine);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
