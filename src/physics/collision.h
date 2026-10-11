#pragma once

#include <glm/glm.hpp>
#include <limits>
#include "scene/entity.h"

class Scene;

namespace Physics {

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

// Scene raycasting queries
bool Raycast(Scene& scene, const glm::vec3& origin, const glm::vec3& direction, float maxDistance, RaycastHit& outHit);
bool Raycast(Scene& scene, const Ray& ray, float maxDistance, RaycastHit& outHit);

} // namespace Physics
