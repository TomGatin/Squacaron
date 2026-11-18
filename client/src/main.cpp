/*
** EPITECH PROJECT, 2026
** r-type
** File description:
** Client entry point
*/

#include <cstddef>
#include <exception>
#include <filesystem>
#include <iostream>
#include <span>

#include "ClientOptions.hpp"
#include "RTypeGamePlugin.hpp"
#include "assets/AssetPlugin.hpp"
#include "assets/audio/AudioAssetsPlugin.hpp"
#include "assets/render/RenderAssetsPlugin.hpp"
#include "assets/scene/SceneAssetsPlugin.hpp"
#include "assets/scripting/AssetScriptingPlugin.hpp"
#include "audio/AudioPlugin.hpp"
#include "config/Settings.hpp"
#include "core/ContentRoot.hpp"
#include "core/Engine.hpp"
#include "hierarchy/HierarchyPlugin.hpp"
#include "input/InputPlugin.hpp"
#include "metrics/MetricsPlugin.hpp"
#include "metrics/ShipMetricsPlugin.hpp"
#include "metrics/overlay/MetricsOverlayPlugin.hpp"
#include "net/ClientNetwork.hpp"
#include "prefabs/PrefabPlugin.hpp"
#include "prefabs/scripting/PrefabScripting.hpp"
#include "render/RenderPlugin.hpp"
#include "render/window/WindowConfig.hpp"
#include "scene/ScenePlugin.hpp"
#include "scene/scripting/SceneScriptingPlugin.hpp"
#include "scenes/ClientScenesPlugin.hpp"
#include "scripting/ScriptPlugin.hpp"
#include "settings/SettingsPlugin.hpp"
#include "systems/physics/PhysicsPlugin.hpp"
#include "systems/render/AnimationPlugin.hpp"
#include "tools/console/ConsolePlugin.hpp"
#include "tools/console/ConsoleTypes.hpp"
#include "tools/devconsole/DevConsolePlugin.hpp"
#include "tools/scripting/ConsoleScriptingPlugin.hpp"
#include "ui/UiPlugin.hpp"

namespace {

constexpr int EXIT_ERROR = 84;

}  // namespace

int main(int ac, char** av) {
    const std::span args(av, static_cast<std::size_t>(ac));
    const auto options = client::parseOptions(args);
    if (!options) {
        std::cerr << "usage: " << args.front() << " [host] [port]\n";
        return EXIT_ERROR;
    }
    try {
        // Scripts and assets are read next to the executable, so the client runs from any working directory.
        const std::filesystem::path root = sq::engine::contentRoot();
        sq::engine::Engine engine;
        // Escape means "back" and "pause" in the menus: the game quits from its title screen.
        engine
            .addPlugin<sq::render::RenderPlugin>(
                sq::render::WindowConfig{.title = "R-Type", .escapeClosesWindow = false})
            .addPlugin<sq::input::InputPlugin>()
            .addPlugin<sq::audio::AudioPlugin>()
            .addPlugin<sq::systems::PhysicsPlugin>()
            .addPlugin<sq::systems::AnimationPlugin>()
            .addPlugin<sq::prefabs::PrefabPlugin>()
            .addPlugin<sq::scripting::ScriptPlugin>(sq::scripting::ScriptTick::EveryFrame, root)
            .addPlugin<sq::prefabs::PrefabScriptingPlugin>()
            .addPlugin<sq::assets::AssetPlugin>(root)
            .addPlugin<sq::assets::AssetScriptingPlugin>("scripts/rtype/Assets")
            .addPlugin<sq::assets::RenderAssetsPlugin>()
            .addPlugin<sq::assets::AudioAssetsPlugin>()
            .addPlugin<sq::hierarchy::HierarchyPlugin>()
            .addPlugin<sq::ui::UiPlugin>()
            .addPlugin<sq::metrics::MetricsPlugin>()
            .addPlugin<sq::metrics::MetricsOverlayPlugin>()
            .addPlugin<client::ShipMetricsPlugin>()
            .addPlugin<sq::scene::ScenePlugin>()
            .addPlugin<sq::scene::SceneScriptingPlugin>()
            .addPlugin<sq::assets::SceneAssetsPlugin>()
            .addPlugin<sq::settings::SettingsPlugin>(sq::config::userConfigDirectory("rtype") / "settings.toml")
            // Cheats only reach this client: the server stays authoritative and runs its own rules.
            .addPlugin<sq::tools::ConsolePlugin>(sq::tools::ConsoleConfig{.allowCheats = true})
            .addPlugin<sq::tools::ConsoleScriptingPlugin>()
            .addPlugin<sq::tools::DevConsolePlugin>()
            .addPlugin<client::RTypeGamePlugin>();
        client::addNetworkPlugins(engine);
        engine.addPlugin<client::ClientScenesPlugin>(options->host, options->port, args.size() > 1);

        engine.run();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "client error: " << error.what() << '\n';
        return EXIT_ERROR;
    }
}
