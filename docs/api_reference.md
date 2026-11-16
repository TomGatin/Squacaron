# C++ API Reference

Welcome to the automated C++ API Reference for the Squacaron engine and its modules.

The documentation in this section is **automatically extracted from code docstrings**
using [Doxygen](https://www.doxygen.nl/index.html) and generated into MkDocs Material
via [MkDoxy](https://github.com/JakubAndrysek/MkDoxy).

---

## Quick Navigation

* **[Class Index](rtype/classes.md)**: Alphabetical list of all classes, structs, and interfaces.
* **[Namespaces](rtype/namespaces.md)**: Index of all engine namespaces, under the `sq` root (`sq::ecs`, `sq::scripting`, `sq::net`...).
* **[Class Hierarchy](rtype/hierarchy.md)**: Inheritance tree for classes and base abstractions.
* **[Files & Headers](rtype/files.md)**: Header files and source documentation.

---

## Core Subsystems

### 1. Engine Core, Plugins & Mediator (`engine`)

The core architecture framework orchestrating the game loop, DAG dependency resolution, and decoupled abstract services.

| Class / Struct                                                       | Role & Description                                                                                          |
|:---------------------------------------------------------------------|:------------------------------------------------------------------------------------------------------------|
| [`sq::engine::Engine`](rtype/classsq_1_1engine_1_1Engine.md)                   | Central coordinator owning Registry, EventBus, Mediator, Clock, and the continuous game loop.               |
| [`sq::engine::IPlugin`](rtype/classsq_1_1engine_1_1IPlugin.md)                 | Base interface providing lifecycle stages (`build`, `init`, `fixedUpdate`, `update`, `render`, `shutdown`). |
| [`sq::engine::PluginBuilder`](rtype/classsq_1_1engine_1_1PluginBuilder.md)     | Fluent dependency configuration builder for declaring required and optional constraints.                    |
| [`sq::engine::PluginGraph`](rtype/classsq_1_1engine_1_1PluginGraph.md)         | Topological sorter (Kahn's algorithm) managing dependency DAGs and cycle detection.                         |
| [`sq::engine::Mediator`](rtype/classsq_1_1engine_1_1Mediator.md)               | Decoupled typed service registry supporting abstract interface resolution with `std::expected`.             |
| [`sq::engine::IWindowService`](rtype/classsq_1_1engine_1_1IWindowService.md)   | Abstract window lifecycle interface for close state and frame presentation.                                 |
| [`sq::engine::IRenderService`](rtype/classsq_1_1engine_1_1IRenderService.md)   | Abstract 2D rendering service interface for entity collection and camera views.                             |
| [`sq::engine::IInputService`](rtype/classsq_1_1engine_1_1IInputService.md)     | Abstract input service interface providing action mappings and controller state.                            |
| [`sq::engine::IPhysicsService`](rtype/classsq_1_1engine_1_1IPhysicsService.md) | Abstract physics service interface for simulation stepping and rigid body metrics.                          |

---

### 2. Scripting Subsystem (`scripting` & `systems`)

The scripting module embeds the Wren lightweight VM into the C++23 engine, providing safe, high-performance scripting
for gameplay designers without macro dependencies.

| Class / Struct                                                                                  | Role & Description                                                                                                                           |
|:------------------------------------------------------------------------------------------------|:---------------------------------------------------------------------------------------------------------------------------------------------|
| [`sq::scripting::WrenVM`](rtype/classsq_1_1scripting_1_1WrenVM.md)                                        | High-level C++23 RAII facade managing the Wren VM instance, template slots (`setSlot<T>`, `getSlot<T>`), and `std::expected` interpretation. |
| [`sq::scripting::ComponentBinder<T>`](rtype/classsq_1_1scripting_1_1ComponentBinder.md)                   | Type-safe fluent builder binding C++ ECS component properties to Wren foreign classes using compile-time pointer-to-members (NTTP).          |
| [`sq::scripting::ComponentBinderBase`](rtype/classsq_1_1scripting_1_1ComponentBinderBase.md)              | Non-template base managing foreign class allocation, property storage, and Wren script generation.                                           |
| [`sq::scripting::ScriptSystem`](rtype/classsq_1_1scripting_1_1ScriptSystem.md)                                | ECS system managing script lifecycles (`onSpawn`), timed coroutines (`Engine.wait`), opt-in frame updates (`update`), and event forwarding.  |
| [`sq::systems::SleepingFiber`](rtype/structsq_1_1systems_1_1SleepingFiber.md)                             | Holds a suspended Wren Fiber and countdown timer for non-blocking coroutines.                                                                |
| [`sq::scripting::ScriptComponent`](rtype/structsq_1_1ecs_1_1ScriptComponent.md)                                 | ECS component attaching a Wren script class instance to an entity.                                                                           |
| [`sq::scripting::detail::PropertyAccessor`](rtype/structsq_1_1scripting_1_1detail_1_1PropertyAccessor.md) | Internal FFI template helper converting C++ struct fields to/from Wren VM slots.                                                             |
| [`sq::scripting::detail::UniqueWrenHandle`](rtype/namespacescripting_1_1detail.md)                  | RAII `std::unique_ptr` ensuring automatic release of Wren VM handles.                                                                        |

---

### 2. Entity Component System (`ecs`)

High-performance data-oriented ECS architecture utilizing sparse sets and deferred mutations.

| Class / Struct                                             | Role & Description                                                                                              |
|:-----------------------------------------------------------|:----------------------------------------------------------------------------------------------------------------|
| [`sq::ecs::Registry`](rtype/classsq_1_1ecs_1_1Registry.md)           | Central ECS coordinator managing entity lifecycles, component sparse arrays, and views.                         |
| [`sq::ecs::CommandBuffer`](rtype/classsq_1_1ecs_1_1CommandBuffer.md) | Deferred command buffer for safe structural mutations (`deferSpawn`, `deferDestroy`, `flush`) during iteration. |
| [`sq::ecs::SparseArray<T>`](rtype/classsq_1_1ecs_1_1SparseArray.md)  | Contiguous packed storage indexed via sparse entity IDs for cache-friendly iteration.                           |
| [`sq::events::EventBus`](rtype/classsq_1_1ecs_1_1EventBus.md)           | Decoupled publish/subscribe event dispatcher connecting systems and scripts.                                    |

---

### 3. Network Subsystem (`sq::ecs::net`)

Engine transport (`engine_network` interfaces, `engine_network_asio` implementation): TCP + UDP byte transport with a
game-defined `NetworkConfig`. Game code only uses the interfaces and the factory.

| Class / Struct                                                                           | Role & Description                                                    |
|:-----------------------------------------------------------------------------------------|:----------------------------------------------------------------------|
| [`sq::net::INetworkServer`](rtype/classsq_1_1ecs_1_1net_1_1INetworkServer.md)                 | Server transport: start, send, broadcast, kick, poll events.          |
| [`sq::net::INetworkClient`](rtype/classsq_1_1ecs_1_1net_1_1INetworkClient.md)                 | Client transport: connect, open/close UDP, send, poll events.         |
| [`sq::net::NetworkConfig`](rtype/structsq_1_1ecs_1_1net_1_1NetworkConfig.md)                  | Game-chosen settings: protocol id, peer limit, timeouts, size limits. |
| [`sq::net::NetworkEntityMap`](rtype/classsq_1_1ecs_1_1net_1_1NetworkEntityMap.md)             | Registry resource mapping network ids to local entities.              |
| [`sq::net::detail::NetworkServer`](rtype/classsq_1_1ecs_1_1net_1_1detail_1_1NetworkServer.md) | Asio server: accept loop, handshake, UDP binding, timeouts (private). |
| [`sq::net::detail::NetworkClient`](rtype/classsq_1_1ecs_1_1net_1_1detail_1_1NetworkClient.md) | Asio client: handshake, UDP bind and keepalives (private).            |
| [`sq::net::detail::TcpSession`](rtype/classsq_1_1ecs_1_1net_1_1detail_1_1TcpSession.md)       | Framed TCP connection with a bounded write queue (private).           |

---

## How to Document New Code

All classes, methods, and structures must include Doxygen docstrings (`/** ... */`) in their `.hpp` headers. MkDoxy
automatically parses these tags upon running `mkdocs build`.

```cpp
/**
 * @brief Brief one-line summary of what the class/function does.
 *
 * Detailed explanation of algorithms, memory ownership, or invariants.
 *
 * @tparam T Description of template parameter.
 * @param arg Description of parameter.
 * @return Description of return value.
 */
```
