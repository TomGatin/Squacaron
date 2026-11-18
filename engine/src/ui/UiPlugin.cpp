/**
 * @file UiPlugin.cpp
 * @brief UiPlugin implementation.
 */

#include "ui/UiPlugin.hpp"

#include <memory>

#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Registry.hpp"
#include "ecs/time/Time.hpp"
#include "hierarchy/HierarchyPlugin.hpp"
#include "render/api/IScreenDrawService.hpp"
#include "render/api/IWindowService.hpp"
#include "ui/components/UiNavigation.hpp"
#include "ui/system/UiButtonSystem.hpp"
#include "ui/system/UiInteractionSystem.hpp"
#include "ui/system/UiLayout.hpp"
#include "ui/system/UiRenderSystem.hpp"

namespace sq::ui {

void UiPlugin::build(sq::engine::PluginBuilder& builder) {
    builder.requireService<sq::engine::IWindowService>();
    builder.requireService<sq::engine::IScreenDrawService>();
    builder.require<sq::hierarchy::HierarchyPlugin>();
}

void UiPlugin::init(sq::engine::Engine& engine) {
    _uiRenderSystem = std::make_unique<UiRenderSystem>();
    sq::ecs::Registry& registry = engine.registry();
    if (!registry.hasResource<UiFocus>()) {
        registry.emplaceResource<UiFocus>();
    }
    if (!registry.hasResource<UiNavigation>()) {
        registry.emplaceResource<UiNavigation>();
    }
}

UiScreenRect UiPlugin::screenOf(sq::engine::Engine& engine) {
    const auto& window = engine.service<sq::engine::IWindowService>();
    return UiScreenRect{.width = static_cast<float>(window.width()), .height = static_cast<float>(window.height())};
}

void UiPlugin::update(sq::engine::Engine& engine, const sq::time::Time& time) {
    if (_uiRenderSystem == nullptr) {
        return;
    }
    const auto& draw = engine.service<sq::engine::IScreenDrawService>();
    computeLayout(engine.registry(), screenOf(engine), UiRenderSystem::measureWith(draw), _layout);
    auto& window = engine.service<sq::engine::IWindowService>();
    UiButtonSystem::update(engine.registry(), _layout, window, engine.eventBus());
    _interaction.update(engine.registry(), _layout, window, engine.eventBus(), time.deltaTime);
}

void UiPlugin::lateUpdate(sq::engine::Engine& engine, const sq::time::Time& /*time*/) {
    if (_uiRenderSystem == nullptr) {
        return;
    }
    _uiRenderSystem->update(engine.registry(), screenOf(engine), engine.service<sq::engine::IScreenDrawService>());
}

void UiPlugin::shutdown(sq::engine::Engine& /*engine*/) {
    _uiRenderSystem.reset();
    _layout.clear();
    _layout.shrink_to_fit();
}

}  // namespace sq::ui
