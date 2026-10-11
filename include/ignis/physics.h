#pragma once

#include "api.h"
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

class Scene;

namespace Ignis::Physics {

enum CollisionLayer : uint32_t {
    Layer_Default     = 1 << 0,  // General scene objects
    Layer_Static      = 1 << 1,  // Architecture, terrain, static environment
    Layer_Player      = 1 << 2,  // Player character
    Layer_Enemy       = 1 << 3,  // AI enemies and NPCs
    Layer_Projectile  = 1 << 4,  // Bullets, rockets, grenades
    Layer_Trigger     = 1 << 5,  // Interactive zones, doors, pickups
    Layer_Custom1     = 1 << 6,
    Layer_Custom2     = 1 << 7,
    Layer_All         = 0xFFFFFFFF
};

struct Ray {
    glm::vec3 Origin{ 0.0f };
    glm::vec3 Direction{ 0.0f, 0.0f, -1.0f };

    Ray() = default;
    Ray(const glm::vec3& origin, const glm::vec3& direction)
        : Origin(origin), Direction(glm::length(direction) > 0.00001f ? glm::normalize(direction) : glm::vec3(0.0f, 0.0f, -1.0f)) {}

    glm::vec3 GetPoint(float distance) const
    {
        return Origin + Direction * distance;
    }
};

} // namespace Ignis::Physics

#include "scene/entity.h"

namespace Ignis::Physics {

struct RaycastHit {
    bool Hit = false;
    float Distance = 0.0f;
    glm::vec3 Point{ 0.0f };
    glm::vec3 Normal{ 0.0f };
    uint32_t EntityHandle = 0;
    Entity GetHitEntity(Scene* scene) const;
    Entity HitEntity;
};

bool Raycast(Scene& scene, const Ray& ray, float maxDistance, RaycastHit& outHit, uint32_t layerMask = Layer_All, bool includeTriggers = false);
bool RaycastAll(Scene& scene, const Ray& ray, float maxDistance, std::vector<RaycastHit>& outHits, uint32_t layerMask = Layer_All, bool includeTriggers = false);

} // namespace Ignis::Physics

// Global convenience namespace
namespace Physics {
    using namespace Ignis::Physics;
}
