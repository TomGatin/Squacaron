#include "VolumeCVarsPlugin.hpp"

#include <algorithm>
#include <string>

#include "config/Settings.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "events/EventBus.hpp"
#include "log/Logger.hpp"
#include "menu/MenuScenesPlugin.hpp"
#include "settings/SettingsEvents.hpp"
#include "settings/SettingsPlugin.hpp"
#include "tools/console/ConsoleEvents.hpp"
#include "tools/console/ConsolePlugin.hpp"
#include "tools/cvars/CVarManager.hpp"
#include "tools/cvars/CVarTypes.hpp"

namespace rlss::client {

/** @brief Log tag of the plugin. */
constexpr const char* LOG_TAG = "console";

void VolumeCVarsPlugin::build(sq::engine::PluginBuilder& builder) {
    builder.require<sq::settings::SettingsPlugin>();
    builder.require<sq::tools::ConsolePlugin>();
    // The menus declare the volume settings in their init: read them after.
    builder.require<MenuScenesPlugin>();
}

void VolumeCVarsPlugin::init(sq::engine::Engine& engine) {
    sq::config::Settings& settings = engine.getPlugin<sq::settings::SettingsPlugin>().settings();
    auto& cvars = engine.service<sq::tools::CVarManager>();
    for (Volume& volume : _volumes) {
        // A hand-edited settings file may hold anything: start inside the range of the cvar.
        const auto current = static_cast<float>(std::clamp(settings.getNumber(volume.setting), 0.0, 1.0));
        volume.ref = cvars.add(std::string(volume.cvar), current, std::string(volume.description),
                               sq::tools::CVarOptions{.flags = sq::tools::CVarFlag::Archived,
                                                      .range = sq::tools::CVarRange{.min = 0.0, .max = 1.0}});
    }

    // Console -> settings: applies the volume and saves it. Skipped when the setting already holds the value, so a
    // change coming from the menu does not bounce back (and lose precision through float).
    _cvarChanged =
        engine.eventBus().subscribe<sq::tools::CVarChanged>([this, &settings](const sq::tools::CVarChanged& event) {
            for (const Volume& volume : _volumes) {
                if (event.name == volume.cvar &&
                    static_cast<float>(settings.getNumber(volume.setting)) != volume.ref.get()) {
                    (void)settings.set(volume.setting, static_cast<double>(volume.ref.get()));
                }
            }
        });
    // Menu -> console.
    _settingChanged = engine.eventBus().subscribe<sq::settings::SettingChanged>(
        [this, &settings](const sq::settings::SettingChanged& event) {
            for (const Volume& volume : _volumes) {
                if (event.key != volume.setting) {
                    continue;
                }
                const auto value = static_cast<float>(std::clamp(settings.getNumber(volume.setting), 0.0, 1.0));
                if (auto synced = volume.ref.set(value); !synced) {
                    sq::engine::log::warning(LOG_TAG, synced.error());
                }
            }
        });
}

void VolumeCVarsPlugin::shutdown(sq::engine::Engine& engine) {
    if (_cvarChanged) {
        engine.eventBus().unsubscribe<sq::tools::CVarChanged>(*_cvarChanged);
        _cvarChanged.reset();
    }
    if (_settingChanged) {
        engine.eventBus().unsubscribe<sq::settings::SettingChanged>(*_settingChanged);
        _settingChanged.reset();
    }
}

}  // namespace rlss::client
