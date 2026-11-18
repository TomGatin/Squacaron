/**
 * @file ScriptSystem.hpp
 * @brief ECS system running the Wren scripts of the entities: lifecycle, events, fibers and hot reload.
 */

#pragma once
#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "ecs/CommandBuffer.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "events/EventBus.hpp"
#include "scripting/FiberScheduler.hpp"
#include "scripting/IScriptExtension.hpp"  // IWYU pragma: export
#include "scripting/OwnerScope.hpp"        // IWYU pragma: export
#include "scripting/ScriptComponent.hpp"   // IWYU pragma: export
#include "scripting/ScriptErrorTracker.hpp"
#include "scripting/ScriptHotReloader.hpp"
#include "scripting/ScriptOwner.hpp"       // IWYU pragma: export
#include "scripting/WrenVM.hpp"            // IWYU pragma: export
#include "scripting/detail/WrenTypes.hpp"  // IWYU pragma: export
#include "utils/MoveOnlyFunction.hpp"

namespace sq::scripting {

/**
 * @brief Runs the Wren scripts attached to entities with `ScriptComponent`.
 *
 * Builds each script instance (`new(entity)`, `onSpawn()`), calls `update(dt)` on those that opt in, forwards
 * collisions and other events, resumes the fibers of `Engine.wait`, and reloads the modules modified on disk.
 * Script mutations go through its `CommandBuffer`, flushed at the end of `update()`. Bridges plug in through
 * `addExtension()`.
 *
 * Not copyable nor movable: the VM, the registry listener and the event subscriptions point to it. `ScriptPlugin`
 * owns one.
 */
class ScriptSystem {
  public:
    /** @brief Creates a system; call `init()` before anything else. */
    ScriptSystem();
    /** @brief Releases the script handles, the registry listener and the collision subscription. */
    ~ScriptSystem();
    /** @brief Deleted copy constructor. */
    ScriptSystem(const ScriptSystem&) = delete;
    /** @brief Deleted copy assignment operator. */
    ScriptSystem& operator=(const ScriptSystem&) = delete;
    /** @brief Deleted move constructor: the VM and the listeners point to this object. */
    ScriptSystem(ScriptSystem&&) = delete;
    /** @brief Deleted move assignment operator. */
    ScriptSystem& operator=(ScriptSystem&&) = delete;

    /**
     * @brief Binds the system to a world, registers the core bindings and loads the built-in modules.
     * @param registry The ECS registry; must outlive the system.
     * @param bus The event bus.
     */
    void init(sq::ecs::Registry& registry, sq::events::EventBus& bus);

    /**
     * @brief One frame: hot reload check, new scripts, fibers, `update(dt)`, deferred destructions and mutations.
     * @param dt Seconds since the previous frame.
     */
    void update(float dt);

    /**
     * @brief Tells whether the script of an entity was disabled after `MAX_CONSECUTIVE_SCRIPT_FAILURES` failed calls.
     *
     * A disabled script gets no more `update(dt)`, `onCollision(other)` or fiber resumes; its `onDestroy()` still
     * runs. Every script is enabled again by the next hot reload.
     * @param e Entity to check.
     * @return True if its script is disabled.
     */
    [[nodiscard]] bool isScriptDisabled(sq::ecs::Entity e) const noexcept { return _errors.isDisabled(e); }

    /**
     * @brief Sets the directory script modules are read from.
     *
     * A module name is a path relative to it, without `.wren` (`scripts/Boss` is `<root>/scripts/Boss.wren`), for
     * both `ScriptComponent::moduleName` and `import`. Absolute names and names climbing out of it (`..`) are
     * refused. Kept across hot reloads.
     * @param root Script root, usually `sq::engine::contentRoot()`; empty (the default) means the working directory.
     */
    void setScriptRoot(std::filesystem::path root) { _scriptRoot = std::move(root); }

    /** @brief Gets the script root (empty for the working directory). */
    [[nodiscard]] const std::filesystem::path& scriptRoot() const noexcept { return _scriptRoot; }

    /**
     * @brief Builds the script instance of an entity, then calls its `onSpawn()` (not on a reload).
     * @param e Entity owning the component.
     * @param script Its component.
     * @param isReload True when rebuilding after a hot reload.
     */
    void instantiateScript(sq::ecs::Entity e, ScriptComponent& script, bool isReload = false);

    /**
     * @brief Calls `methodName(arg)` on the script of the target entity of each `EventType` published on `bus`.
     * @tparam EventType Event subscribed to.
     * @tparam ArgExtractor Callable returning a `std::pair<sq::ecs::Entity, ArgType>` (target, argument) for an event.
     * @param bus Bus to subscribe to.
     * @param methodName Wren method (e.g. "onHit").
     * @param extractor Extracts the target and the argument.
     */
    template <typename EventType, typename ArgExtractor>
    void registerEventForwarding(sq::events::EventBus& bus, std::string const& methodName, ArgExtractor&& extractor);

    /**
     * @brief Forwards a contact event (with `entityA` and `entityB`) to `onCollision(other)` of the scripts that set
     * `hasCollision`. `engine_physics_scripting` does it for the physics contacts.
     *
     * A side is skipped when either entity is dead or queued for destruction this frame, so a script never hits an
     * entity another system just destroyed. Calling it again moves the subscription.
     * @tparam ContactEvent Event type, e.g. `sq::events::CollisionBeginEvent`.
     * @param bus Bus receiving the contact events.
     */
    template <typename ContactEvent>
    void forwardCollisions(sq::events::EventBus& bus);

    /** @brief Unsubscribes the collision forwarding, if any. Safe to call twice. */
    void stopForwardingCollisions();

    /** @brief Gets the Wren VM. */
    [[nodiscard]] WrenVM& getVm() noexcept { return _vm; }
    /** @brief Gets the command buffer the script mutations go through. */
    [[nodiscard]] sq::ecs::CommandBuffer& getCommandBuffer() noexcept { return _commandBuffer; }
    /** @brief Gets the registry given to `init()`, or nullptr before. */
    [[nodiscard]] sq::ecs::Registry* getRegistry() noexcept { return _registry; }

    /**
     * @brief Spawns a prefab for `Engine.spawnPrefab(name, x, y)`: the first extension whose `spawnPrefab` accepts it
     * does (`engine_prefabs_scripting` provides one). Logs a warning if none does.
     * @param name Prefab name.
     * @param x Horizontal position.
     * @param y Vertical position.
     * @return True if an extension spawned it.
     */
    bool requestPrefabSpawn(std::string_view name, double x, double y);

    /**
     * @brief Callback to run on each entity spawned for the Wren code running now: tells every extension
     * (`onScriptSpawn`) with the current owner. Pass it to the spawner, e.g. `PrefabManager::spawn`.
     * @return The callback.
     */
    [[nodiscard]] sq::ecs::MoveOnlyFunction<void(sq::ecs::Entity, sq::ecs::Registry&)> spawnObserver();

    /**
     * @brief Finds an added extension by type.
     * @tparam Extension A class publicly derived from `IScriptExtension`.
     * @return The first extension of that type, or nullptr.
     */
    template <typename Extension>
    [[nodiscard]] Extension* findExtension() const noexcept;

    /** @brief Gets the scratch buffer reused by the `World` query bindings. */
    [[nodiscard]] std::vector<sq::ecs::Entity>& getQueryBuffer() noexcept { return _queryBuffer; }
    /** @brief Whether an entity is queued for destruction by a script this frame. */
    [[nodiscard]] bool isPendingDestroy(sq::ecs::Entity e) const noexcept;
    /** @brief Entities queued for destruction by a script this frame. */
    [[nodiscard]] std::span<sq::ecs::Entity const> getPendingDestroyEntities() const noexcept {
        return _pendingDestroyEntities;
    }

    /** @brief Gets the script instance of an entity, or nullptr. */
    [[nodiscard]] ScriptObject* getScriptInstance(sq::ecs::Entity e) const noexcept;
    /** @brief Tells whether an entity has a script instance. */
    [[nodiscard]] bool hasScriptInstance(sq::ecs::Entity e) const noexcept { return _instanceHandles.contains(e); }

    /**
     * @brief Puts a fiber to sleep for `seconds`, on behalf of the current owner (`Engine.wait`).
     * @param fiber Owning handle to the fiber.
     * @param seconds Delay.
     */
    void addSleepingFiber(detail::UniqueWrenHandle fiber, float seconds) {
        _fibers.add(std::move(fiber), seconds, _currentOwner);
    }

    /** @brief Drops the sleeping fibers of an owner, so they never run again. @param owner Their owner. */
    void cancelFibers(ScriptOwner owner) { _fibers.cancel(owner); }

    /** @brief Gets the owner of the Wren code running now: an entity, an extension owner, or `NO_SCRIPT_OWNER`. */
    [[nodiscard]] ScriptOwner currentOwner() const noexcept { return _currentOwner; }

    /** @brief Enables or disables the hot reload of the script files (enabled by default). */
    void enableHotReload(bool enabled) noexcept { _reloader.enable(enabled); }
    /** @brief Tells whether hot reload is enabled. */
    [[nodiscard]] bool isHotReloadEnabled() const noexcept { return _reloader.isEnabled(); }

    /**
     * @brief Checks the watched files now, and replaces the VM if a module changed and still runs.
     * @return True if the scripts were reloaded.
     */
    bool reloadModifiedScripts();

    /**
     * @brief Tells whether every watched module ran again without error in the last hot reload. Read by extensions
     * in `onVmReset(VmResetPhase::After)`.
     * @return True when the last replay succeeded, or before any hot reload.
     */
    [[nodiscard]] bool lastReplaySucceeded() const noexcept { return _reloader.lastReplaySucceeded(); }

    /** @brief Calls the `onDestroy()` hook of an entity's script, then releases its instance. */
    void triggerDestroyHook(sq::ecs::Entity e);
    /** @brief Queues an entity for destruction at the end of the frame, after its `onDestroy()`. */
    void queueDestroyEntity(sq::ecs::Entity e);

    /**
     * @brief Adds bindings to register again on every new VM (hot reload, staging checks), e.g. game components.
     * @param callback Registers foreign methods or classes on a VM.
     */
    void addBinderReplay(std::function<void(WrenVM&)> callback) { _binderReplays.push_back(std::move(callback)); }

    /**
     * @brief Plugs an extension in: its modules become importable and it is bound to the VM now and after each
     * reset. It must outlive the system or be removed first.
     * @param extension The extension.
     */
    void addExtension(IScriptExtension& extension);

    /**
     * @brief Unplugs an extension: it is no longer bound, told about resets or spawns. Its modules stay importable.
     * @param extension The extension; ignored if it was not added.
     */
    void removeExtension(IScriptExtension& extension);

    /**
     * @brief Imports a script module (built-in or `<name>.wren` under the script root), and watches its file.
     * @param moduleName Module name.
     * @return True if the module is loaded.
     */
    bool loadModule(const std::string& moduleName) { return ensureScriptModuleLoaded(moduleName); }

  private:
    friend class OwnerScope;

    void invokeEntityMethod(sq::ecs::Entity target, std::string const& signature, double arg);
    void forwardCollision(sq::ecs::Entity target, sq::ecs::Entity other);
    void prepareCollisionForwarding();
    void recordResult(ScriptOwner owner, CallResult result, std::string_view hook);
    void prepareVm(WrenVM& vm);
    void installModuleLoader();
    void makeCallHandles();
    bool ensureScriptModuleLoaded(const std::string& moduleName);
    [[nodiscard]] detail::UniqueWrenHandle createScriptInstance(sq::ecs::Entity e, const ScriptComponent& script);
    void releaseActiveHandles();
    void recreateVm();
    void reinstantiateActiveScripts();
    void notifyVmReset(VmResetPhase phase);
    void detachDestroyListener() noexcept;

    /** @brief Registry given to `init()`. */
    sq::ecs::Registry* _registry = nullptr;
    /** @brief Id of the registry destroy listener running `onDestroy()` of killed entities. */
    std::optional<std::size_t> _destroyListenerId;
    /** @brief Bus given to `init()`. */
    sq::events::EventBus* _bus = nullptr;
    /** @brief Mutations made by scripts, flushed at the end of `update()`. */
    sq::ecs::CommandBuffer _commandBuffer;
    /** @brief The Wren VM. */
    WrenVM _vm;
    /** @brief Directory the module files are read from. */
    std::filesystem::path _scriptRoot;
    /** @brief Built-in modules of the extensions, by import name. */
    std::unordered_map<std::string, std::string> _extraModules;
    /** @brief Entities whose script opted in to `update(dt)`. */
    std::vector<sq::ecs::Entity> _updatingEntities;
    /** @brief Entities queued for destruction by a script this frame. */
    std::vector<sq::ecs::Entity> _pendingDestroyEntities;
    /** @brief Scratch buffer of the `World` queries. */
    std::vector<sq::ecs::Entity> _queryBuffer;
    /** @brief Script instances, by entity. */
    std::unordered_map<sq::ecs::Entity, detail::UniqueWrenHandle> _instanceHandles;
    /** @brief Entities to rebuild after a VM reset. */
    std::vector<sq::ecs::Entity> _entitiesToReinstantiate;
    /** @brief Call handles of `new(_)`, `update(_)`, `call()`. */
    detail::UniqueWrenHandle _newHandle, _updateHandle, _fiberCallHandle;
    /** @brief Call handles of the forwarded event methods, by signature. */
    std::unordered_map<std::string, detail::UniqueWrenHandle> _cachedMethodHandles;
    /** @brief Unsubscribes the collision forwarding; empty when there is none. */
    std::function<void()> _stopCollisions;
    /** @brief Bindings registered again on every new VM. */
    std::vector<std::function<void(WrenVM&)>> _binderReplays;
    /** @brief Extensions, in the order they were added. */
    std::vector<IScriptExtension*> _extensions;
    /** @brief Owner of the Wren code running now. */
    ScriptOwner _currentOwner = NO_SCRIPT_OWNER;
    /** @brief Fibers waiting for `Engine.wait`. */
    FiberScheduler _fibers;
    /** @brief Failed calls, rate limit and disabled scripts. */
    ScriptErrorTracker _errors;
    /** @brief Watched files and replay of hot reload. */
    ScriptHotReloader _reloader;
};

}  // namespace sq::scripting

#include "scripting/ScriptSystem.tpp"  // IWYU pragma: export
