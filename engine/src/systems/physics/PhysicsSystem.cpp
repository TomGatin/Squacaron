#include "systems/physics/PhysicsSystem.hpp"

#include <box2d/b2_body.h>
#include <box2d/b2_circle_shape.h>
#include <box2d/b2_fixture.h>
#include <box2d/b2_math.h>
#include <box2d/b2_polygon_shape.h>
#include <box2d/b2_world.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "ecs/ComponentType.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/Transform.hpp"
#include "events/EventBus.hpp"
#include "systems/physics/PhysicsConstants.hpp"
#include "systems/physics/PhysicsEvents.hpp"
#include "systems/physics/components/Collider2D.hpp"
#include "systems/physics/components/RigidBody2D.hpp"
#include "systems/physics/detail/Box2DContactListener.hpp"

namespace sq::systems {

namespace {

using sq::physics::detail::BodySerial;
using sq::physics::detail::Box2DContactListener;
using sq::physics::detail::ContactSide;
using sq::physics::detail::PendingContact;

/** @brief Converts an ECS rigid-body mode to its Box2D equivalent. */
b2BodyType toBox2DBodyType(sq::ecs::BodyType bodyType) {
    switch (bodyType) {
        case sq::ecs::BodyType::Static:
            return b2_staticBody;
        case sq::ecs::BodyType::Kinematic:
            return b2_kinematicBody;
        case sq::ecs::BodyType::Dynamic:
            return b2_dynamicBody;
    }
    return b2_staticBody;
}

/** @brief Box2D body owned by an entity. */
struct BodyRecord {
    /** @brief Box2D body handle. */
    b2Body* body = nullptr;
    /** @brief Serial given to the body at creation. */
    BodySerial serial = 0;
};

}  // namespace

/** @brief Box2D-backed state of a PhysicsSystem. */
struct PhysicsSystem::Impl {
    Impl(sq::ecs::Registry& registry, sq::events::EventBus& bus);
    ~Impl();

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;

    void update(float fixedDt);
    void createBody(sq::ecs::Entity entity, const sq::ecs::Transform& transform, const sq::ecs::RigidBody2D& rigidBody);
    void synchronizeBody(sq::ecs::Entity entity, sq::ecs::Transform& transform, sq::ecs::RigidBody2D& rigidBody);
    void synchronizeCollider(sq::ecs::Entity entity, const sq::ecs::Transform& transform, b2Body& body);
    static void destroyFixtures(b2Body& body);
    void destroyBody(sq::ecs::Entity entity);
    [[nodiscard]] std::optional<sq::ecs::Entity> entityOf(const ContactSide& side) const;
    void publishContacts();

    sq::ecs::Registry* registry;
    sq::events::EventBus* bus;
    b2World world;
    Box2DContactListener listener;
    std::vector<PendingContact> dispatch;
    std::unordered_map<sq::ecs::Entity, BodyRecord> bodies;
    std::unordered_map<BodySerial, sq::ecs::Entity> entitiesBySerial;
    std::unordered_map<sq::ecs::Entity, b2Vec2> colliderScales;
    BodySerial nextSerial = 1;
    std::size_t destroyListenerId;
    std::size_t componentRemoveListenerId;
};

PhysicsSystem::Impl::Impl(sq::ecs::Registry& registry, sq::events::EventBus& bus)
    : registry(&registry),
      bus(&bus),
      world(b2Vec2_zero),
      destroyListenerId(registry.addDestroyListener([this](sq::ecs::Entity entity) { destroyBody(entity); })),
      componentRemoveListenerId(
          registry.addComponentRemoveListener([this](sq::ecs::Entity entity, sq::ecs::ComponentTypeId componentType) {
              if (componentType == sq::ecs::getComponentTypeId<sq::ecs::RigidBody2D>() ||
                  componentType == sq::ecs::getComponentTypeId<sq::ecs::Transform>()) {
                  destroyBody(entity);
              }
          })) {
    dispatch.reserve(sq::physics::INITIAL_CONTACT_CAPACITY);
    world.SetContactListener(&listener);
}

PhysicsSystem::Impl::~Impl() {
    registry->removeDestroyListener(destroyListenerId);
    registry->removeComponentRemoveListener(componentRemoveListenerId);
    listener.setSuppressed(true);
    world.SetContactListener(nullptr);
}

void PhysicsSystem::Impl::update(float fixedDt) {
    if (fixedDt <= 0.0F) {
        return;
    }

    registry->runSystem<sq::ecs::Transform, sq::ecs::RigidBody2D>(
        [this](sq::ecs::Entity entity, sq::ecs::Transform& transform, sq::ecs::RigidBody2D& rigidBody) {
            synchronizeBody(entity, transform, rigidBody);
        });

    world.Step(fixedDt, sq::physics::VELOCITY_ITERATIONS, sq::physics::POSITION_ITERATIONS);

    registry->runSystem<sq::ecs::Transform, sq::ecs::RigidBody2D>(
        [this](sq::ecs::Entity entity, sq::ecs::Transform& transform, sq::ecs::RigidBody2D& rigidBody) {
            if (rigidBody.bodyType == sq::ecs::BodyType::Static) {
                return;
            }

            const auto it = bodies.find(entity);
            if (it == bodies.end()) {
                return;
            }

            const b2Body& body = *it->second.body;
            const b2Vec2 position = body.GetPosition();
            const b2Vec2 velocity = body.GetLinearVelocity();
            transform.x = position.x * sq::physics::PPM;
            transform.y = position.y * sq::physics::PPM;
            transform.rotation = body.GetAngle();
            rigidBody.velocityX = velocity.x * sq::physics::PPM;
            rigidBody.velocityY = velocity.y * sq::physics::PPM;
        });

    publishContacts();
}

void PhysicsSystem::Impl::createBody(sq::ecs::Entity entity, const sq::ecs::Transform& transform,
                                     const sq::ecs::RigidBody2D& rigidBody) {
    b2BodyDef bodyDef;
    bodyDef.type = toBox2DBodyType(rigidBody.bodyType);
    bodyDef.position.Set(transform.x / sq::physics::PPM, transform.y / sq::physics::PPM);
    bodyDef.angle = transform.rotation;
    bodyDef.linearVelocity.Set(rigidBody.velocityX / sq::physics::PPM, rigidBody.velocityY / sq::physics::PPM);
    bodyDef.linearDamping = rigidBody.linearDamping;
    bodyDef.angularDamping = rigidBody.angularDamping;
    bodyDef.fixedRotation = rigidBody.fixedRotation;
    bodyDef.bullet = rigidBody.bullet;
    bodyDef.userData.pointer = nextSerial;

    b2Body* body = world.CreateBody(&bodyDef);
    bodies.insert_or_assign(entity, BodyRecord{.body = body, .serial = nextSerial});
    entitiesBySerial.insert_or_assign(nextSerial, entity);
    ++nextSerial;
    synchronizeCollider(entity, transform, *body);
}

void PhysicsSystem::Impl::synchronizeBody(sq::ecs::Entity entity, sq::ecs::Transform& transform,
                                          sq::ecs::RigidBody2D& rigidBody) {
    auto it = bodies.find(entity);
    if (it == bodies.end()) {
        createBody(entity, transform, rigidBody);
        return;
    }

    b2Body& body = *it->second.body;
    body.SetType(toBox2DBodyType(rigidBody.bodyType));
    body.SetLinearDamping(rigidBody.linearDamping);
    body.SetAngularDamping(rigidBody.angularDamping);
    body.SetFixedRotation(rigidBody.fixedRotation);
    body.SetBullet(rigidBody.bullet);

    const b2Vec2 currentPosition = body.GetPosition();
    const float targetX = transform.x / sq::physics::PPM;
    const float targetY = transform.y / sq::physics::PPM;
    if (std::fabs(currentPosition.x - targetX) > sq::physics::POSITION_EPSILON ||
        std::fabs(currentPosition.y - targetY) > sq::physics::POSITION_EPSILON ||
        std::fabs(body.GetAngle() - transform.rotation) > sq::physics::POSITION_EPSILON) {
        body.SetTransform(b2Vec2{targetX, targetY}, transform.rotation);
    }

    if (rigidBody.bodyType != sq::ecs::BodyType::Static) {
        b2Vec2 velocity{rigidBody.velocityX / sq::physics::PPM, rigidBody.velocityY / sq::physics::PPM};
        const float maxSpeed = std::max(0.0F, rigidBody.maxLinearSpeed / sq::physics::PPM);
        const float speed = velocity.Length();
        if (speed > maxSpeed && speed > 0.0F) {
            velocity *= maxSpeed / speed;
            rigidBody.velocityX = velocity.x * sq::physics::PPM;
            rigidBody.velocityY = velocity.y * sq::physics::PPM;
        }
        body.SetLinearVelocity(velocity);
    }
    synchronizeCollider(entity, transform, body);
}

void PhysicsSystem::Impl::destroyFixtures(b2Body& body) {
    while (b2Fixture* fixture = body.GetFixtureList()) {
        body.DestroyFixture(fixture);
    }
}

void PhysicsSystem::Impl::synchronizeCollider(sq::ecs::Entity entity, const sq::ecs::Transform& transform,
                                              b2Body& body) {
    if (!registry->hasComponent<sq::ecs::Collider2D>(entity)) {
        destroyFixtures(body);
        colliderScales.erase(entity);
        return;
    }

    const b2Vec2 scale{std::fabs(transform.scaleX), std::fabs(transform.scaleY)};
    const auto scaleIt = colliderScales.find(entity);
    const bool scaleChanged = scaleIt == colliderScales.end() ||
                              std::fabs(scaleIt->second.x - scale.x) > sq::physics::POSITION_EPSILON ||
                              std::fabs(scaleIt->second.y - scale.y) > sq::physics::POSITION_EPSILON;
    if (body.GetFixtureList() != nullptr && !scaleChanged) {
        return;
    }

    destroyFixtures(body);

    const sq::ecs::Collider2D& collider = registry->getComponents<sq::ecs::Collider2D>().get(entity);
    const bool hasValidDimensions =
        collider.shape == sq::ecs::ColliderShape::Circle
            ? std::max(scale.x, scale.y) > sq::physics::POSITION_EPSILON
            : scale.x > sq::physics::POSITION_EPSILON && scale.y > sq::physics::POSITION_EPSILON;
    if (!hasValidDimensions) {
        colliderScales.insert_or_assign(entity, scale);
        return;
    }

    b2FixtureDef fixtureDef;
    fixtureDef.density = collider.density;
    fixtureDef.friction = collider.friction;
    fixtureDef.restitution = collider.restitution;
    fixtureDef.isSensor = collider.isSensor;
    fixtureDef.filter.categoryBits = collider.categoryBits;
    fixtureDef.filter.maskBits = collider.maskBits;

    b2CircleShape circle;
    b2PolygonShape box;
    if (collider.shape == sq::ecs::ColliderShape::Circle) {
        circle.m_radius = collider.radius * std::max(scale.x, scale.y) / sq::physics::PPM;
        fixtureDef.shape = &circle;
    } else {
        box.SetAsBox(collider.halfWidth * scale.x / sq::physics::PPM, collider.halfHeight * scale.y / sq::physics::PPM);
        fixtureDef.shape = &box;
    }
    body.CreateFixture(&fixtureDef);
    colliderScales.insert_or_assign(entity, scale);
}

void PhysicsSystem::Impl::destroyBody(sq::ecs::Entity entity) {
    const auto it = bodies.find(entity);
    if (it == bodies.end()) {
        return;
    }
    listener.setSuppressed(true);
    world.DestroyBody(it->second.body);
    listener.setSuppressed(false);
    entitiesBySerial.erase(it->second.serial);
    bodies.erase(it);
    colliderScales.erase(entity);
}

std::optional<sq::ecs::Entity> PhysicsSystem::Impl::entityOf(const ContactSide& side) const {
    const auto owner = entitiesBySerial.find(side.serial);
    if (owner == entitiesBySerial.end() || !registry->isAlive(owner->second)) {
        return std::nullopt;
    }
    const auto it = bodies.find(owner->second);
    if (it == bodies.end() || it->second.serial != side.serial) {
        return std::nullopt;
    }
    return owner->second;
}

void PhysicsSystem::Impl::publishContacts() {
    listener.swapPending(dispatch);
    for (const PendingContact& contact : dispatch) {
        const std::optional<sq::ecs::Entity> entityA = entityOf(contact.a);
        const std::optional<sq::ecs::Entity> entityB = entityOf(contact.b);
        if (!entityA.has_value() || !entityB.has_value()) {
            continue;
        }
        if (contact.begin) {
            bus->publish(sq::events::CollisionBeginEvent{
                .entityA = *entityA,
                .entityB = *entityB,
                .categoryA = contact.a.category,
                .categoryB = contact.b.category,
                .isSensor = contact.isSensor,
                .hasContactPoint = contact.hasContactPoint,
                .normalX = contact.normalX,
                .normalY = contact.normalY,
                .pointX = contact.pointX,
                .pointY = contact.pointY,
            });
        } else {
            bus->publish(sq::events::CollisionEndEvent{
                .entityA = *entityA,
                .entityB = *entityB,
                .categoryA = contact.a.category,
                .categoryB = contact.b.category,
                .isSensor = contact.isSensor,
            });
        }
    }
    dispatch.clear();
}

PhysicsSystem::PhysicsSystem(sq::ecs::Registry& registry, sq::events::EventBus& bus)
    : _impl(std::make_unique<Impl>(registry, bus)) {}

PhysicsSystem::~PhysicsSystem() = default;

void PhysicsSystem::update(float fixedDt) { _impl->update(fixedDt); }

std::size_t PhysicsSystem::bodyCount() const noexcept { return _impl->bodies.size(); }

}  // namespace sq::systems
