/**
 * @file IScriptExtension.hpp
 * @brief The one way a bridge module (scenes, assets, a game) plugs into the `ScriptSystem`.
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ecs/Entity.hpp"
#include "scripting/ScriptOwner.hpp"

namespace ecs {
class Registry;
}

namespace scripting {

class WrenVM;

/** @brief Moment of a hot-reload VM swap reported to `IScriptExtension::onVmReset`. */
enum class VmResetPhase : std::uint8_t {
    /** @brief The VM is about to be destroyed: release every handle into it. */
    Before,
    /** @brief The new VM is ready, with the core bindings, the extensions and the reloaded modules. */
    After,
};

/** @brief A built-in module scripts can import by name, like `core/Engine`. */
struct ModuleSource {
    /** @brief Import name (e.g. `core/Scene`). */
    std::string name;
    /** @brief Wren source of the module. */
    std::string source;
};

/**
 * @brief Extension of the script system, added with `ScriptSystem::addExtension()`.
 *
 * Every hook has a default that does nothing: override only what the extension needs. An extension that runs Wren
 * code on its own behalf (a scene, a console command) builds its owner ids with `makeExtensionOwner()` and runs that
 * code inside a `ScriptSystem::OwnerScope`.
 */
class IScriptExtension {
  public:
    /** @brief Virtual destructor. */
    virtual ~IScriptExtension() = default;

    /**
     * @brief Built-in modules of the extension, read once by `addExtension()` and kept across hot reloads. They are
     * looked up after the core modules and before the files on disk.
     * @return The modules.
     */
    [[nodiscard]] virtual std::vector<ModuleSource> modules() const { return {}; }

    /**
     * @brief Registers the foreign methods of the extension on a VM: the live one at `addExtension()`, every staging
     * VM that checks a modified script, and the new VM of each hot reload.
     * @param vm VM to bind.
     */
    virtual void bind(WrenVM& vm) { (void)vm; }

    /**
     * @brief Told before and after a hot reload replaces the VM.
     * @param phase `Before`: release every handle into the VM. `After`: the new VM is ready;
     * `ScriptSystem::lastReplaySucceeded()` says whether every module ran again.
     */
    virtual void onVmReset(VmResetPhase phase) { (void)phase; }

    /**
     * @brief Called for every entity that Wren code spawns (`Engine.spawnPrefab`, `ScriptSystem::spawnPrefab`), once
     * it is built.
     * @param registry Registry holding the entity.
     * @param spawned The new entity.
     * @param owner Owner that was running when the spawn was requested.
     */
    virtual void onScriptSpawn(ecs::Registry& registry, ecs::Entity spawned, ScriptOwner owner) {
        (void)registry;
        (void)spawned;
        (void)owner;
    }

  protected:
    /** @brief Default constructor. */
    IScriptExtension() = default;
    /** @brief Copy constructor, for the classes implementing it. */
    IScriptExtension(const IScriptExtension&) = default;
    /** @brief Copy assignment, for the classes implementing it. */
    IScriptExtension& operator=(const IScriptExtension&) = default;
    /** @brief Move constructor, for the classes implementing it. */
    IScriptExtension(IScriptExtension&&) = default;
    /** @brief Move assignment, for the classes implementing it. */
    IScriptExtension& operator=(IScriptExtension&&) = default;
};

}  // namespace scripting
