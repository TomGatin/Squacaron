#include "menu/MenuScenesPlugin.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include "ClientEvents.hpp"
#include "Controls.hpp"
#include "audio/IAudioService.hpp"
#include "audio/types/AudioTypes.hpp"
#include "config/Settings.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Registry.hpp"
#include "ecs/time/Time.hpp"
#include "input/IInputService.hpp"
#include "input/InputPlugin.hpp"
#include "menu/MenuScenes.hpp"
#include "net/ClientReplicationPlugin.hpp"
#include "net/INetworkManager.hpp"
#include "net/INetworkService.hpp"
#include "net/NetworkEvents.hpp"
#include "net/NetworkPlugin.hpp"
#include "protocol/Protocol.hpp"
#include "scene/SceneManager.hpp"
#include "scene/ScenePlugin.hpp"
#include "settings/SettingsEvents.hpp"
#include "settings/SettingsPlugin.hpp"
#include "ui/UiPlugin.hpp"

namespace rlss::client {

namespace {

/** @brief Saved player name. */
constexpr std::string_view NAME_KEY = "player.name";

/**
 * @brief Why a request was refused.
 * @param status Answer of the server.
 * @return Message for the player.
 */
std::string_view refusal(const protocol::JoinStatus status) {
    switch (status) {
        case protocol::JoinStatus::Full:
            return "PARTIE PLEINE";
        case protocol::JoinStatus::NotFound:
            return "PARTIE TERMINEE";
        case protocol::JoinStatus::ServerFull:
            return "SERVEUR PLEIN";
        case protocol::JoinStatus::Joined:
            break;
    }
    return {};
}

/**
 * @brief Sends a message to the server, on the reliable channel.
 * @param engine Host engine.
 * @param bytes Encoded message.
 */
void send(sq::engine::Engine& engine, const protocol::Bytes& bytes) {
    engine.service<sq::net::INetworkService>().send(sq::net::SERVER_PEER, bytes, sq::net::Channel::Reliable);
}

/**
 * @brief Gives the volume settings to the audio service, if there is one.
 * @param engine Host engine.
 * @param settings Settings.
 */
void applyVolumes(sq::engine::Engine& engine, const sq::config::Settings& settings) {
    auto audio = engine.mediator().getService<sq::engine::IAudioService>();
    if (!audio) {
        return;
    }
    audio->get().setVolume(sq::audio::AudioBus::Master, static_cast<float>(settings.getNumber("audio.master")));
    audio->get().setVolume(sq::audio::AudioBus::Music, static_cast<float>(settings.getNumber("audio.music")));
    audio->get().setVolume(sq::audio::AudioBus::Sfx, static_cast<float>(settings.getNumber("audio.sfx")));
}

/**
 * @brief Rebuilds the key bindings from the saved controls, if the program reads input.
 * @param engine Host engine.
 * @param settings Settings.
 */
void applyBindings(sq::engine::Engine& engine, const sq::config::Settings& settings) {
    if (auto inputService = engine.mediator().getService<sq::engine::IInputService>()) {
        setupBindings(inputService->get().mapping(), readControlKeys(settings));
    }
}

}  // namespace

void MenuScenesPlugin::build(sq::engine::PluginBuilder& builder) {
    builder.require<sq::net::NetworkPlugin>();
    builder.require<sq::net::ClientReplicationPlugin>();
    builder.require<sq::scene::ScenePlugin>();
    builder.require<sq::settings::SettingsPlugin>();
    builder.require<sq::ui::UiPlugin>();
    builder.optionally<sq::input::InputPlugin>();  // initialized first, so the saved keys replace the default ones
}

void MenuScenesPlugin::init(sq::engine::Engine& engine) {
    sq::ecs::Registry& registry = engine.registry();
    registry.emplaceResource<MenuState>();

    sq::config::Settings& settings = engine.getPlugin<sq::settings::SettingsPlugin>().settings();
    settings.define(std::string(NAME_KEY), std::string{});
    settings.define("audio.master", 1.0);
    settings.define("audio.music", 0.8);
    settings.define("audio.sfx", 1.0);
    defineControlSettings(settings);
    applyVolumes(engine, settings);
    applyBindings(engine, settings);
    engine.eventBus().subscribe<sq::settings::SettingChanged>(
        [&engine, &settings](const sq::settings::SettingChanged& event) {
            if (event.key.starts_with("audio.")) {
                applyVolumes(engine, settings);
            } else if (event.key.starts_with(CONTROLS_PREFIX)) {
                applyBindings(engine, settings);
            }
        });

    _packets =
        engine.eventBus().subscribe<sq::net::PacketReceived>([this, &engine](const sq::net::PacketReceived& packet) {
            if (const auto message = protocol::decode(packet.data)) {
                onMessage(engine, *message);
            }
        });

    sq::scene::SceneManager& scenes = engine.getPlugin<sq::scene::ScenePlugin>().manager();
    scenes.registerScene<NameScene>();
    scenes.registerScene<MenuScene>();
    scenes.registerScene<BrowserScene>();
    scenes.registerScene<JoiningScene>();
    scenes.registerScene<MatchScene>();
    scenes.registerScene<EndedScene>();
    scenes.registerScene<SettingsScene>();
    scenes.registerScene<ControlsScene>();

    // A name saved by an earlier session skips the name screen.
    if (!confirmName(engine, settings.getString(NAME_KEY))) {
        registry.getResource<MenuState>().notice.clear();
        scenes.switchTo<NameScene>();
    }
}

bool MenuScenesPlugin::confirmName(sq::engine::Engine& engine, std::string typed) {
    while (!typed.empty() && typed.back() == ' ') {
        typed.pop_back();
    }
    auto& state = engine.registry().getResource<MenuState>();
    if (!protocol::isValidName(typed)) {
        state.notify("ENTRE UN PSEUDO");
        return false;
    }
    state.name = typed;
    send(engine, protocol::encode(protocol::SetName{.name = typed}));
    (void)engine.getPlugin<sq::settings::SettingsPlugin>().settings().set(NAME_KEY, typed);
    engine.getPlugin<sq::scene::ScenePlugin>().manager().switchTo<MenuScene>();
    return true;
}

void MenuScenesPlugin::request(sq::engine::Engine& engine, const protocol::Bytes& request) {
    auto& state = engine.registry().getResource<MenuState>();
    if (state.requestPending) {
        return;  // a release and a confirm in the same frame, for instance: one request is enough
    }
    state.requestPending = true;
    send(engine, request);
    engine.getPlugin<sq::scene::ScenePlugin>().manager().push<JoiningScene>();
}

void MenuScenesPlugin::cancelRequest(sq::engine::Engine& engine) {
    auto& state = engine.registry().getResource<MenuState>();
    if (state.requestPending) {
        state.requestPending = false;
        ++state.cancelledRequests;
    }
}

void MenuScenesPlugin::leaveMatch(sq::engine::Engine& engine) {
    auto& state = engine.registry().getResource<MenuState>();
    state.inMatch = false;
    state.endedShown = false;
    send(engine, protocol::encode(protocol::LeaveMatch{}));
    _link->setMatch(std::nullopt);
    // Every entity of the match goes (the views remove their parts on `ReplicatedEntityDespawned`), and its last tick
    // is forgotten: the next match counts its steps from 0.
    engine.getPlugin<sq::net::ClientReplicationPlugin>().reset(engine);
    engine.eventBus().publish(MatchLeft{});
    engine.getPlugin<sq::scene::ScenePlugin>().manager().switchTo<MenuScene>();
}

void MenuScenesPlugin::onMessage(sq::engine::Engine& engine, const protocol::Message& message) {
    auto& state = engine.registry().getResource<MenuState>();
    // `state.inMatch` rather than the scene stack: `Welcome` or the final status can come in the same poll as the
    // `JoinResult`, before the queued switch to `MatchScene` is applied.
    if (const auto* list = std::get_if<protocol::MatchList>(&message)) {
        if (*list != state.matches) {
            state.matches = *list;
            engine.eventBus().publish(MatchListUpdated{});
        }
    } else if (const auto* result = std::get_if<protocol::JoinResult>(&message)) {
        onJoinResult(engine, *result);
    } else if (const auto* welcome = std::get_if<protocol::Welcome>(&message)) {
        if (state.inMatch) {
            state.team = welcome->team;
        }
    } else if (const auto* status = std::get_if<protocol::MatchStatus>(&message)) {
        if (state.inMatch && status->phase == protocol::MatchPhase::Ended && !state.endedShown) {
            state.final = *status;
            state.endedShown = true;
            engine.getPlugin<sq::scene::ScenePlugin>().manager().push<EndedScene>();
        }
    }
}

void MenuScenesPlugin::onJoinResult(sq::engine::Engine& engine, const protocol::JoinResult& result) {
    sq::scene::SceneManager& scenes = engine.getPlugin<sq::scene::ScenePlugin>().manager();
    auto& state = engine.registry().getResource<MenuState>();
    if (state.cancelledRequests > 0) {
        // The answer to a request the player cancelled: drop it, and leave the match it put the player in.
        --state.cancelledRequests;
        if (result.status == protocol::JoinStatus::Joined) {
            send(engine, protocol::encode(protocol::LeaveMatch{}));
        }
        return;
    }
    state.requestPending = false;
    if (result.status != protocol::JoinStatus::Joined) {
        if (scenes.top() == scenes.idOf<JoiningScene>()) {
            state.notify(std::string(refusal(result.status)));
            scenes.pop();
        }
        return;
    }
    // Joined, even if the wait timed out: the server put the player in the match, follow it.
    if (state.inMatch) {
        return;
    }
    state.inMatch = true;
    state.endedShown = false;
    state.team.reset();
    _link->setMatch(result.matchId);
    scenes.switchTo<MatchScene>();
    engine.eventBus().publish(MatchJoined{.matchId = result.matchId});
}

void MenuScenesPlugin::update(sq::engine::Engine& engine, const sq::time::Time& time) {
    auto& state = engine.registry().getResource<MenuState>();
    if (state.noticeTime > 0.0F) {
        state.noticeTime -= time.deltaTime;
        if (state.noticeTime <= 0.0F) {
            state.notice.clear();
        }
    }
}

void MenuScenesPlugin::shutdown(sq::engine::Engine& engine) {
    if (_packets) {
        engine.eventBus().unsubscribe<sq::net::PacketReceived>(*_packets);
        _packets.reset();
    }
}

}  // namespace rlss::client
