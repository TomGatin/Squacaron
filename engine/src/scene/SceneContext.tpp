/**
 * @file SceneContext.tpp
 * @brief Template implementations for SceneContext.
 */

#pragma once

#include <utility>

#include "scene/SceneContext.hpp"  // NOLINT(misc-header-include-cycle)

namespace sq::scene {

template <typename EventType>
void SceneContext::subscribe(sq::ecs::MoveOnlyFunction<void(const EventType&)> callback) {
    sq::events::EventBus& eventBus = bus();
    const sq::events::EventBus::EventToken token = eventBus.subscribe<EventType>(std::move(callback));
    _subscriptions.push_back(Subscription{.token = token, .unsubscribe = &unsubscribeFrom<EventType>});
}

template <typename EventType>
void SceneContext::unsubscribeFrom(sq::events::EventBus& bus, sq::events::EventBus::EventToken token) {
    bus.unsubscribe<EventType>(token);
}

}  // namespace sq::scene
