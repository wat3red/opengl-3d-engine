#pragma once

#include <glm/glm.hpp>
#include <limits>
#include <vector>
#include <cstdint>
#include "scene/entity.h"

class Scene;

namespace Physics {

// Collision layers bitmask flags
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

inline const char* GetLayerName(uint32_t layer)
{
	switch (layer)
	{
	case Layer_Default:    return "Default";
	case Layer_Static:     return "Static Environment";
	case Layer_Player:     return "Player";
	case Layer_Enemy:      return "Enemy";
	case Layer_Projectile: return "Projectile";
	case Layer_Trigger:    return "Trigger";
	case Layer_Custom1:    return "Custom 1";
	case Layer_Custom2:    return "Custom 2";
	default:               return "Mixed / Custom";
	}
}

inline bool CanCollide(uint32_t layerA, uint32_t maskA, uint32_t layerB, uint32_t maskB)
{
	return ((layerA & maskB) != 0) && ((layerB & maskA) != 0);
}

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

struct RaycastHit {
	bool Hit = false;
	Entity HitEntity{};
	glm::vec3 Point{ 0.0f };
	glm::vec3 Normal{ 0.0f, 1.0f, 0.0f };
	float Distance = std::numeric_limits<float>::max();
};

// Geometric ray-primitive intersection tests
bool IntersectRaySphere(const Ray& ray, const glm::vec3& center, float radius, float& outDistance, glm::vec3& outNormal);
bool IntersectRayAABB(const Ray& ray, const glm::vec3& aabbMin, const glm::vec3& aabbMax, float& outDistance, glm::vec3& outNormal);
bool IntersectRayOBB(const Ray& ray, const glm::vec3& boxCenter, const glm::vec3& boxSize, const glm::mat4& worldTransform, float& outDistance, glm::vec3& outNormal);
bool IntersectRayCapsule(const Ray& ray, const glm::vec3& center, float radius, float height, const glm::mat4& worldTransform, float& outDistance, glm::vec3& outNormal);

// Swept primitive tests
bool IntersectSweptSphereAABB(const glm::vec3& start, const glm::vec3& displacement, float radius, const glm::vec3& aabbMin, const glm::vec3& aabbMax, float& outDistance, glm::vec3& outNormal);
bool IntersectSweptSphereOBB(const glm::vec3& start, const glm::vec3& displacement, float radius, const glm::vec3& boxCenter, const glm::vec3& boxSize, const glm::mat4& worldTransform, float& outDistance, glm::vec3& outNormal);
bool IntersectSweptSphereSphere(const glm::vec3& start, const glm::vec3& displacement, float radius, const glm::vec3& sphereCenter, float sphereRadius, float& outDistance, glm::vec3& outNormal);
bool IntersectSweptSphereCapsule(const glm::vec3& start, const glm::vec3& displacement, float radius, const glm::vec3& capsuleCenter, float capsuleRadius, float capsuleHeight, const glm::mat4& worldTransform, float& outDistance, glm::vec3& outNormal);

// Scene raycasting queries
bool Raycast(Scene& scene, const glm::vec3& origin, const glm::vec3& direction, float maxDistance, RaycastHit& outHit, uint32_t layerMask = Layer_All, bool includeTriggers = false);
bool Raycast(Scene& scene, const Ray& ray, float maxDistance, RaycastHit& outHit, uint32_t layerMask = Layer_All, bool includeTriggers = false);
bool RaycastAll(Scene& scene, const Ray& ray, float maxDistance, std::vector<RaycastHit>& outHits, uint32_t layerMask = Layer_All, bool includeTriggers = false);

// Spatial overlap queries
bool OverlapSphere(Scene& scene, const glm::vec3& center, float radius, std::vector<Entity>& outEntities, uint32_t layerMask = Layer_All, bool includeTriggers = true);
bool OverlapBox(Scene& scene, const glm::vec3& center, const glm::vec3& halfExtents, std::vector<Entity>& outEntities, uint32_t layerMask = Layer_All, bool includeTriggers = true);

// Swept Capsule & Depenetration queries (used by Kinematic Character Controller)
bool SweepCapsule(Scene& scene, const glm::vec3& startPos, const glm::vec3& displacement, float radius, float height, const glm::vec3& centerOffset, RaycastHit& outHit, Entity ignoreEntity = {}, uint32_t layerMask = Layer_All);
bool CheckCapsuleOverlap(Scene& scene, const glm::vec3& pos, float radius, float height, const glm::vec3& centerOffset, glm::vec3& outPushVector, Entity ignoreEntity = {}, uint32_t layerMask = Layer_All);

} // namespace Physics
