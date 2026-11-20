#include <gtest/gtest.h>

#include <chrono>
#include <csignal>
#include <ctime>
#include <string_view>

#include "core/Engine.hpp"
#include "core/FixedRateLoop.hpp"
#include "core/IPlugin.hpp"
#include "core/platform/StopSignals.hpp"
#include "ecs/time/Time.hpp"

namespace {

using Clock = std::chrono::steady_clock;

/** @brief Counts its updates; stops the engine after `limit` of them, or raises SIGINT when asked to. */
class FrameCounterPlugin final : public engine::IPlugin {
  public:
    FrameCounterPlugin(int& frames, const int limit, const bool raiseSignal)
        : _frames(&frames), _limit(limit), _raiseSignal(raiseSignal) {}
    [[nodiscard]] std::string_view name() const noexcept override { return "FrameCounter"; }
    void update(engine::Engine& engine, const ecs::time::Time& /*time*/) override {
        if (++*_frames < _limit) {
            return;
        }
        if (_raiseSignal) {
            std::raise(SIGINT);
        } else {
            engine.stop();
        }
    }

  private:
    int* _frames;
    int _limit;
    bool _raiseSignal;
};

/** @brief Clears the stop flag around a test, so a raised signal does not leak into the next one. */
class RunLoopTest : public ::testing::Test {
  protected:
    void SetUp() override { engine::platform::clearStopSignal(); }
    void TearDown() override { engine::platform::clearStopSignal(); }
};

}  // namespace

TEST_F(RunLoopTest, PacedLoopWaitsForEachTick) {
    constexpr double RATE = 100.0;
    constexpr int TICKS = 20;
    engine::FixedRateLoop loop(RATE);
    int ticks = 0;

    const auto start = Clock::now();
    loop.run([&] {
        if (++ticks == TICKS) {
            loop.stop();
        }
    });
    const auto elapsed = Clock::now() - start;

    EXPECT_EQ(ticks, TICKS);
    // The first tick starts at once, each next one a period later.
    EXPECT_GE(elapsed, (TICKS - 1) * loop.period() - std::chrono::milliseconds(1));
}

TEST_F(RunLoopTest, UnpacedLoopDoesNotSleep) {
    engine::FixedRateLoop loop;
    int ticks = 0;

    const auto start = Clock::now();
    loop.run([&] {
        if (++ticks == 10000) {
            loop.stop();
        }
    });

    EXPECT_EQ(loop.period(), std::chrono::nanoseconds::zero());
    EXPECT_LT(Clock::now() - start, std::chrono::seconds(1));
}

TEST_F(RunLoopTest, StopBeforeRunIsKept) {
    engine::FixedRateLoop loop(1000.0);
    loop.stop();
    int ticks = 0;

    loop.run([&] { ++ticks; });

    EXPECT_EQ(ticks, 0) << "a stop() made before run() (e.g. from another thread) must not be lost";
}

TEST_F(RunLoopTest, StopSignalEndsTheLoopWhenAsked) {
    engine::FixedRateLoop loop(1000.0);
    loop.stopOnSignals(true);
    int ticks = 0;

    loop.run([&] {
        if (++ticks == 3) {
            std::raise(SIGINT);  // the handler only sets a flag: the process goes on
        }
    });

    EXPECT_EQ(ticks, 3);
    EXPECT_TRUE(engine::platform::stopSignalReceived());
}

TEST_F(RunLoopTest, StopSignalIsIgnoredByDefault) {
    engine::platform::installStopSignalHandlers();
    std::raise(SIGTERM);
    engine::FixedRateLoop loop(1000.0);
    int ticks = 0;

    loop.run([&] {
        if (++ticks == 5) {
            loop.stop();
        }
    });

    EXPECT_EQ(ticks, 5);
}

TEST_F(RunLoopTest, HeadlessEngineRunsOneFramePerFixedStep) {
    constexpr int FRAMES = 10;
    engine::Engine engine;
    int frames = 0;
    engine.addPlugin<FrameCounterPlugin>(frames, FRAMES, false);

    const auto start = Clock::now();
    engine.run();
    const auto elapsed = std::chrono::duration<double>(Clock::now() - start).count();

    EXPECT_EQ(frames, FRAMES);
    EXPECT_GE(elapsed, (FRAMES - 1) * static_cast<double>(engine.fixedDeltaTime()) * 0.9);
}

TEST_F(RunLoopTest, HeadlessEngineStopsOnCtrlC) {
    engine::Engine engine;
    int frames = 0;
    engine.addPlugin<FrameCounterPlugin>(frames, 3, true);

    engine.run();

    EXPECT_EQ(frames, 3);
    EXPECT_FALSE(engine.isRunning());
}

TEST_F(RunLoopTest, UnpacedEngineIsAvailableOnRequest) {
    engine::Engine engine;
    int frames = 0;
    engine.addPlugin<FrameCounterPlugin>(frames, 1000, false);

    const auto start = Clock::now();
    engine.run({.frameRate = -1.0, .stopOnSignals = false});

    EXPECT_EQ(frames, 1000);
    EXPECT_LT(Clock::now() - start, std::chrono::seconds(1)) << "1000 frames at 60 Hz would take 16 s";
}

#ifndef _WIN32  // std::clock() measures wall time on Windows, CPU time elsewhere
TEST_F(RunLoopTest, IdleHeadlessEngineBarelyUsesTheCpu) {
    constexpr int FRAMES = 20;  // a third of a second at 60 Hz
    engine::Engine engine;
    int frames = 0;
    engine.addPlugin<FrameCounterPlugin>(frames, FRAMES, false);

    const std::clock_t cpuStart = std::clock();
    const auto wallStart = Clock::now();
    engine.run();
    const double cpu = static_cast<double>(std::clock() - cpuStart) / CLOCKS_PER_SEC;
    const double wall = std::chrono::duration<double>(Clock::now() - wallStart).count();

    // A busy loop uses about 100 % of the wall time.
    EXPECT_LT(cpu, 0.2 * wall) << "cpu " << cpu << " s for " << wall << " s";
}
#endif

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
