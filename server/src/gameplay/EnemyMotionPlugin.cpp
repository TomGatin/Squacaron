#include "gameplay/EnemyMotionPlugin.hpp"

#include <cmath>
#include <numbers>

#include "core/Engine.hpp"
#include "ecs/Entity.hpp"
#include "ecs/components/Transform.hpp"
#include "ecs/time/Time.hpp"
#include "prefabs/EnemyMotion.hpp"

namespace rtype::server::gameplay {

void EnemyMotionPlugin::fixedUpdate(sq::engine::Engine& engine, const sq::time::Time& time) {
    engine.registry().runSystem<::prefabs::SineWave, sq::ecs::Transform>(
        [&time](sq::ecs::Entity /*entity*/, ::prefabs::SineWave& wave, sq::ecs::Transform& transform) {
            wave.time += time.deltaTime;
            transform.y =
                wave.baseY + (std::sin(wave.time * wave.frequency * 2.0F * std::numbers::pi_v<float>) * wave.amplitude);
        });
}

}  // namespace rtype::server::gameplay
