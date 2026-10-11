#include "collision.h"
#include "scene/scene.h"
#include "scene/components.h"
#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace Physics {

// ============================================================================
// Ray-Primitive Intersection Tests
// ============================================================================

bool IntersectRaySphere(const Ray& ray, const glm::vec3& center, float radius, float& outDistance, glm::vec3& outNormal)
{
	glm::vec3 m = ray.Origin - center;
	float b = glm::dot(m, ray.Direction);
	float c = glm::dot(m, m) - radius * radius;

	if (c > 0.0f && b > 0.0f)
		return false;

	float discr = b * b - c;
	if (discr < 0.0f)
		return false;

	float sqrtDiscr = std::sqrt(discr);
	float t = -b - sqrtDiscr;

	if (t < 0.0f)
		t = -b + sqrtDiscr;

	if (t < 0.0f)
		return false;

	outDistance = t;
	glm::vec3 hitPoint = ray.GetPoint(t);
	outNormal = (radius > 0.00001f) ? glm::normalize(hitPoint - center) : glm::vec3(0.0f, 1.0f, 0.0f);
	return true;
}

bool IntersectRayAABB(const Ray& ray, const glm::vec3& aabbMin, const glm::vec3& aabbMax, float& outDistance, glm::vec3& outNormal)
{
	float tmin = 0.0f;
	float tmax = std::numeric_limits<float>::max();
	glm::vec3 normal(0.0f);

	for (int i = 0; i < 3; ++i)
	{
		float originComp = ray.Origin[i];
		float dirComp = ray.Direction[i];
		float minComp = aabbMin[i];
		float maxComp = aabbMax[i];

		if (std::abs(dirComp) < 1e-7f)
		{
			// Ray is parallel to the slab
			if (originComp < minComp || originComp > maxComp)
				return false;
		}
		else
		{
			float invD = 1.0f / dirComp;
			float t1 = (minComp - originComp) * invD;
			float t2 = (maxComp - originComp) * invD;

			glm::vec3 n(0.0f);
			n[i] = -1.0f;

			if (t1 > t2)
			{
				std::swap(t1, t2);
				n[i] = 1.0f;
			}

			if (t1 > tmin)
			{
				tmin = t1;
				normal = n;
			}

			tmax = std::min(tmax, t2);

			if (tmin > tmax)
				return false;
		}
	}

	if (tmax < 0.0f)
		return false;

	outDistance = (tmin >= 0.0f) ? tmin : tmax;
	outNormal = (tmin >= 0.0f) ? normal : -normal;
	return true;
}

bool IntersectRayOBB(const Ray& ray, const glm::vec3& boxCenter, const glm::vec3& boxSize, const glm::mat4& worldTransform, float& outDistance, glm::vec3& outNormal)
{
	glm::mat4 boxWorld = worldTransform * glm::translate(glm::mat4(1.0f), boxCenter);
	glm::mat4 invWorld = glm::inverse(boxWorld);

	glm::vec3 localOrigin = glm::vec3(invWorld * glm::vec4(ray.Origin, 1.0f));
	glm::vec3 localDir = glm::vec3(invWorld * glm::vec4(ray.Direction, 0.0f));

	float localDirLen = glm::length(localDir);
	if (localDirLen < 1e-7f)
		return false;

	Ray localRay;
	localRay.Origin = localOrigin;
	localRay.Direction = localDir / localDirLen;

	glm::vec3 halfSize = boxSize * 0.5f;
	glm::vec3 localMin = -halfSize;
	glm::vec3 localMax = halfSize;

	float localDist = 0.0f;
	glm::vec3 localNormal(0.0f);
	if (!IntersectRayAABB(localRay, localMin, localMax, localDist, localNormal))
		return false;

	glm::vec3 localHit = localRay.GetPoint(localDist);
	glm::vec3 worldHit = glm::vec3(boxWorld * glm::vec4(localHit, 1.0f));

	float worldDist = glm::distance(worldHit, ray.Origin);
	if (glm::dot(ray.Direction, worldHit - ray.Origin) < 0.0f)
		return false;

	glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(boxWorld)));
	glm::vec3 worldNorm = glm::normalize(normalMatrix * localNormal);

	outDistance = worldDist;
	outNormal = worldNorm;
	return true;
}

bool IntersectRayCapsule(const Ray& ray, const glm::vec3& center, float radius, float height, const glm::mat4& worldTransform, float& outDistance, glm::vec3& outNormal)
{
	glm::mat4 capWorld = worldTransform * glm::translate(glm::mat4(1.0f), center);
	glm::mat4 invWorld = glm::inverse(capWorld);

	glm::vec3 localOrigin = glm::vec3(invWorld * glm::vec4(ray.Origin, 1.0f));
	glm::vec3 localDir = glm::vec3(invWorld * glm::vec4(ray.Direction, 0.0f));

	float localDirLen = glm::length(localDir);
	if (localDirLen < 1e-7f)
		return false;

	localDir /= localDirLen;

	float halfH = std::max(0.0f, height * 0.5f - radius);
	float bestLocalDist = std::numeric_limits<float>::max();
	glm::vec3 bestLocalNorm(0.0f, 1.0f, 0.0f);
	bool hitAny = false;

	// 1. Check cylinder side (aligned along local Y axis)
	float A = localDir.x * localDir.x + localDir.z * localDir.z;
	if (A > 1e-7f)
	{
		float B = 2.0f * (localOrigin.x * localDir.x + localOrigin.z * localDir.z);
		float C = localOrigin.x * localOrigin.x + localOrigin.z * localOrigin.z - radius * radius;
		float discr = B * B - 4.0f * A * C;
		if (discr >= 0.0f)
		{
			float sqrtD = std::sqrt(discr);
			float t0 = (-B - sqrtD) / (2.0f * A);
			float t1 = (-B + sqrtD) / (2.0f * A);

			float tCandidates[2] = { t0, t1 };
			for (float tCand : tCandidates)
			{
				if (tCand > 0.0f && tCand < bestLocalDist)
				{
					float hitY = localOrigin.y + tCand * localDir.y;
					if (hitY >= -halfH && hitY <= halfH)
					{
						bestLocalDist = tCand;
						bestLocalNorm = glm::normalize(glm::vec3(localOrigin.x + tCand * localDir.x, 0.0f, localOrigin.z + tCand * localDir.z));
						hitAny = true;
					}
				}
			}
		}
	}

	// 2. Check top cap hemisphere at (0, +halfH, 0)
	{
		float dSphere = 0.0f;
		glm::vec3 nSphere(0.0f);
		Ray localR;
		localR.Origin = localOrigin;
		localR.Direction = localDir;
		if (IntersectRaySphere(localR, glm::vec3(0.0f, halfH, 0.0f), radius, dSphere, nSphere))
		{
			glm::vec3 pt = localR.GetPoint(dSphere);
			if (pt.y >= halfH && dSphere < bestLocalDist)
			{
				bestLocalDist = dSphere;
				bestLocalNorm = nSphere;
				hitAny = true;
			}
		}
	}

	// 3. Check bottom cap hemisphere at (0, -halfH, 0)
	{
		float dSphere = 0.0f;
		glm::vec3 nSphere(0.0f);
		Ray localR;
		localR.Origin = localOrigin;
		localR.Direction = localDir;
		if (IntersectRaySphere(localR, glm::vec3(0.0f, -halfH, 0.0f), radius, dSphere, nSphere))
		{
			glm::vec3 pt = localR.GetPoint(dSphere);
			if (pt.y <= -halfH && dSphere < bestLocalDist)
			{
				bestLocalDist = dSphere;
				bestLocalNorm = nSphere;
				hitAny = true;
			}
		}
	}

	if (!hitAny)
		return false;

	glm::vec3 localHit = localOrigin + bestLocalDist * localDir;
	glm::vec3 worldHit = glm::vec3(capWorld * glm::vec4(localHit, 1.0f));

	float worldDist = glm::distance(worldHit, ray.Origin);
	if (glm::dot(ray.Direction, worldHit - ray.Origin) < 0.0f)
		return false;

	glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(capWorld)));
	glm::vec3 worldNorm = glm::normalize(normalMatrix * bestLocalNorm);

	outDistance = worldDist;
	outNormal = worldNorm;
	return true;
}

// ============================================================================
// Swept Sphere Primitive Tests
// ============================================================================

bool IntersectSweptSphereAABB(const glm::vec3& start, const glm::vec3& displacement, float radius, const glm::vec3& aabbMin, const glm::vec3& aabbMax, float& outDistance, glm::vec3& outNormal)
{
	float dispLen = glm::length(displacement);
	if (dispLen < 1e-7f)
		return false;

	glm::vec3 dir = displacement / dispLen;
	glm::vec3 expMin = aabbMin - glm::vec3(radius);
	glm::vec3 expMax = aabbMax + glm::vec3(radius);

	Ray ray(start, dir);
	float hitDist = 0.0f;
	glm::vec3 hitNorm(0.0f);

	if (!IntersectRayAABB(ray, expMin, expMax, hitDist, hitNorm))
		return false;

	if (hitDist > dispLen)
		return false;

	glm::vec3 hitPoint = ray.GetPoint(hitDist);
	glm::vec3 clamped = glm::clamp(hitPoint, aabbMin, aabbMax);

	glm::vec3 n = hitPoint - clamped;
	if (glm::length(n) > 1e-5f)
	{
		outNormal = glm::normalize(n);
	}
	else
	{
		outNormal = hitNorm;
	}

	outDistance = hitDist;
	return true;
}

bool IntersectSweptSphereOBB(const glm::vec3& start, const glm::vec3& displacement, float radius, const glm::vec3& boxCenter, const glm::vec3& boxSize, const glm::mat4& worldTransform, float& outDistance, glm::vec3& outNormal)
{
	glm::mat4 boxWorld = worldTransform * glm::translate(glm::mat4(1.0f), boxCenter);
	glm::mat4 invWorld = glm::inverse(boxWorld);

	glm::vec3 localStart = glm::vec3(invWorld * glm::vec4(start, 1.0f));
	glm::vec3 localDisp = glm::vec3(invWorld * glm::vec4(displacement, 0.0f));

	glm::vec3 halfSize = boxSize * 0.5f;
	float localDist = 0.0f;
	glm::vec3 localNorm(0.0f);

	if (!IntersectSweptSphereAABB(localStart, localDisp, radius, -halfSize, halfSize, localDist, localNorm))
		return false;

	glm::vec3 worldNorm = glm::normalize(glm::mat3(worldTransform) * localNorm);
	outNormal = worldNorm;

	float dispLen = glm::length(displacement);
	float localDispLen = glm::length(localDisp);
	float fraction = (localDispLen > 1e-6f) ? (localDist / localDispLen) : 0.0f;
	outDistance = fraction * dispLen;

	return true;
}

bool IntersectSweptSphereSphere(const glm::vec3& start, const glm::vec3& displacement, float radius, const glm::vec3& sphereCenter, float sphereRadius, float& outDistance, glm::vec3& outNormal)
{
	float dispLen = glm::length(displacement);
	if (dispLen < 1e-7f)
		return false;

	Ray ray(start, displacement / dispLen);
	float hitDist = 0.0f;
	glm::vec3 hitNorm(0.0f);

	if (!IntersectRaySphere(ray, sphereCenter, radius + sphereRadius, hitDist, hitNorm))
		return false;

	if (hitDist > dispLen)
		return false;

	outDistance = hitDist;
	outNormal = hitNorm;
	return true;
}

bool IntersectSweptSphereCapsule(const glm::vec3& start, const glm::vec3& displacement, float radius, const glm::vec3& capsuleCenter, float capsuleRadius, float capsuleHeight, const glm::mat4& worldTransform, float& outDistance, glm::vec3& outNormal)
{
	float dispLen = glm::length(displacement);
	if (dispLen < 1e-7f)
		return false;

	Ray ray(start, displacement / dispLen);
	float hitDist = 0.0f;
	glm::vec3 hitNorm(0.0f);

	if (!IntersectRayCapsule(ray, capsuleCenter, capsuleRadius + radius, capsuleHeight, worldTransform, hitDist, hitNorm))
		return false;

	if (hitDist > dispLen)
		return false;

	outDistance = hitDist;
	outNormal = hitNorm;
	return true;
}

// ============================================================================
// Scene Raycasting Queries
// ============================================================================

bool Raycast(Scene& scene, const glm::vec3& origin, const glm::vec3& direction, float maxDistance, RaycastHit& outHit, uint32_t layerMask, bool includeTriggers)
{
	Ray ray(origin, direction);
	return Raycast(scene, ray, maxDistance, outHit, layerMask, includeTriggers);
}

bool Raycast(Scene& scene, const Ray& ray, float maxDistance, RaycastHit& outHit, uint32_t layerMask, bool includeTriggers)
{
	outHit.Hit = false;
	outHit.Distance = maxDistance;

	auto& registry = scene.GetRegistry();

	// 1. Box Colliders
	{
		auto view = registry.view<TagComponent, TransformComponent, BoxColliderComponent>();
		for (auto [handle, tag, tc, box] : view.each())
		{
			if (!tag.Active || (box.Layer & layerMask) == 0)
				continue;
			if (!includeTriggers && box.IsTrigger)
				continue;

			float dist = 0.0f;
			glm::vec3 normal(0.0f);
			if (IntersectRayOBB(ray, box.Center, box.Size, tc.GetMatrix(), dist, normal))
			{
				if (dist >= 0.0f && dist <= outHit.Distance)
				{
					outHit.Hit = true;
					outHit.Distance = dist;
					outHit.Point = ray.GetPoint(dist);
					outHit.Normal = normal;
					outHit.HitEntity = Entity{ handle, &scene };
				}
			}
		}
	}

	// 2. Sphere Colliders
	{
		auto view = registry.view<TagComponent, TransformComponent, SphereColliderComponent>();
		for (auto [handle, tag, tc, sphere] : view.each())
		{
			if (!tag.Active || (sphere.Layer & layerMask) == 0)
				continue;
			if (!includeTriggers && sphere.IsTrigger)
				continue;

			float maxScale = std::max({ tc.Scale.x, tc.Scale.y, tc.Scale.z });
			float worldRadius = sphere.Radius * maxScale;
			glm::vec3 worldCenter = glm::vec3(tc.GetMatrix() * glm::vec4(sphere.Center, 1.0f));

			float dist = 0.0f;
			glm::vec3 normal(0.0f);
			if (IntersectRaySphere(ray, worldCenter, worldRadius, dist, normal))
			{
				if (dist >= 0.0f && dist <= outHit.Distance)
				{
					outHit.Hit = true;
					outHit.Distance = dist;
					outHit.Point = ray.GetPoint(dist);
					outHit.Normal = normal;
					outHit.HitEntity = Entity{ handle, &scene };
				}
			}
		}
	}

	// 3. Capsule Colliders
	{
		auto view = registry.view<TagComponent, TransformComponent, CapsuleColliderComponent>();
		for (auto [handle, tag, tc, capsule] : view.each())
		{
			if (!tag.Active || (capsule.Layer & layerMask) == 0)
				continue;
			if (!includeTriggers && capsule.IsTrigger)
				continue;

			float dist = 0.0f;
			glm::vec3 normal(0.0f);
			if (IntersectRayCapsule(ray, capsule.Center, capsule.Radius, capsule.Height, tc.GetMatrix(), dist, normal))
			{
				if (dist >= 0.0f && dist <= outHit.Distance)
				{
					outHit.Hit = true;
					outHit.Distance = dist;
					outHit.Point = ray.GetPoint(dist);
					outHit.Normal = normal;
					outHit.HitEntity = Entity{ handle, &scene };
				}
			}
		}
	}

	// 4. Mesh Colliders
	{
		auto view = registry.view<TagComponent, TransformComponent, MeshColliderComponent, MeshComponent>();
		for (auto [handle, tag, tc, meshCol, mc] : view.each())
		{
			if (!tag.Active || !mc.ModelAsset || (meshCol.Layer & layerMask) == 0)
				continue;
			if (!includeTriggers && meshCol.IsTrigger)
				continue;

			glm::vec3 aabbMin(0.0f), aabbMax(0.0f);
			mc.GetWorldAABB(tc.GetMatrix(), aabbMin, aabbMax);

			float dist = 0.0f;
			glm::vec3 normal(0.0f);
			if (IntersectRayAABB(ray, aabbMin, aabbMax, dist, normal))
			{
				if (dist >= 0.0f && dist <= outHit.Distance)
				{
					outHit.Hit = true;
					outHit.Distance = dist;
					outHit.Point = ray.GetPoint(dist);
					outHit.Normal = normal;
					outHit.HitEntity = Entity{ handle, &scene };
				}
			}
		}
	}

	return outHit.Hit;
}

bool RaycastAll(Scene& scene, const Ray& ray, float maxDistance, std::vector<RaycastHit>& outHits, uint32_t layerMask, bool includeTriggers)
{
	outHits.clear();
	auto& registry = scene.GetRegistry();

	// Box
	{
		auto view = registry.view<TagComponent, TransformComponent, BoxColliderComponent>();
		for (auto [handle, tag, tc, box] : view.each())
		{
			if (!tag.Active || (box.Layer & layerMask) == 0)
				continue;
			if (!includeTriggers && box.IsTrigger)
				continue;

			float dist = 0.0f;
			glm::vec3 normal(0.0f);
			if (IntersectRayOBB(ray, box.Center, box.Size, tc.GetMatrix(), dist, normal))
			{
				if (dist >= 0.0f && dist <= maxDistance)
				{
					RaycastHit hit;
					hit.Hit = true;
					hit.Distance = dist;
					hit.Point = ray.GetPoint(dist);
					hit.Normal = normal;
					hit.HitEntity = Entity{ handle, &scene };
					outHits.push_back(hit);
				}
			}
		}
	}

	// Sphere
	{
		auto view = registry.view<TagComponent, TransformComponent, SphereColliderComponent>();
		for (auto [handle, tag, tc, sphere] : view.each())
		{
			if (!tag.Active || (sphere.Layer & layerMask) == 0)
				continue;
			if (!includeTriggers && sphere.IsTrigger)
				continue;

			float maxScale = std::max({ tc.Scale.x, tc.Scale.y, tc.Scale.z });
			float worldRadius = sphere.Radius * maxScale;
			glm::vec3 worldCenter = glm::vec3(tc.GetMatrix() * glm::vec4(sphere.Center, 1.0f));

			float dist = 0.0f;
			glm::vec3 normal(0.0f);
			if (IntersectRaySphere(ray, worldCenter, worldRadius, dist, normal))
			{
				if (dist >= 0.0f && dist <= maxDistance)
				{
					RaycastHit hit;
					hit.Hit = true;
					hit.Distance = dist;
					hit.Point = ray.GetPoint(dist);
					hit.Normal = normal;
					hit.HitEntity = Entity{ handle, &scene };
					outHits.push_back(hit);
				}
			}
		}
	}

	// Capsule
	{
		auto view = registry.view<TagComponent, TransformComponent, CapsuleColliderComponent>();
		for (auto [handle, tag, tc, capsule] : view.each())
		{
			if (!tag.Active || (capsule.Layer & layerMask) == 0)
				continue;
			if (!includeTriggers && capsule.IsTrigger)
				continue;

			float dist = 0.0f;
			glm::vec3 normal(0.0f);
			if (IntersectRayCapsule(ray, capsule.Center, capsule.Radius, capsule.Height, tc.GetMatrix(), dist, normal))
			{
				if (dist >= 0.0f && dist <= maxDistance)
				{
					RaycastHit hit;
					hit.Hit = true;
					hit.Distance = dist;
					hit.Point = ray.GetPoint(dist);
					hit.Normal = normal;
					hit.HitEntity = Entity{ handle, &scene };
					outHits.push_back(hit);
				}
			}
		}
	}

	// Mesh
	{
		auto view = registry.view<TagComponent, TransformComponent, MeshColliderComponent, MeshComponent>();
		for (auto [handle, tag, tc, meshCol, mc] : view.each())
		{
			if (!tag.Active || !mc.ModelAsset || (meshCol.Layer & layerMask) == 0)
				continue;
			if (!includeTriggers && meshCol.IsTrigger)
				continue;

			glm::vec3 aabbMin(0.0f), aabbMax(0.0f);
			mc.GetWorldAABB(tc.GetMatrix(), aabbMin, aabbMax);

			float dist = 0.0f;
			glm::vec3 normal(0.0f);
			if (IntersectRayAABB(ray, aabbMin, aabbMax, dist, normal))
			{
				if (dist >= 0.0f && dist <= maxDistance)
				{
					RaycastHit hit;
					hit.Hit = true;
					hit.Distance = dist;
					hit.Point = ray.GetPoint(dist);
					hit.Normal = normal;
					hit.HitEntity = Entity{ handle, &scene };
					outHits.push_back(hit);
				}
			}
		}
	}

	std::sort(outHits.begin(), outHits.end(), [](const RaycastHit& a, const RaycastHit& b) {
		return a.Distance < b.Distance;
	});

	return !outHits.empty();
}

// ============================================================================
// Spatial Overlap Queries
// ============================================================================

bool OverlapSphere(Scene& scene, const glm::vec3& center, float radius, std::vector<Entity>& outEntities, uint32_t layerMask, bool includeTriggers)
{
	outEntities.clear();
	std::unordered_set<entt::entity> addedHandles;
	auto& registry = scene.GetRegistry();

	// 1. Box Colliders
	{
		auto view = registry.view<TagComponent, TransformComponent, BoxColliderComponent>();
		for (auto [handle, tag, tc, box] : view.each())
		{
			if (!tag.Active || (box.Layer & layerMask) == 0)
				continue;
			if (!includeTriggers && box.IsTrigger)
				continue;

			glm::mat4 boxWorld = tc.GetMatrix() * glm::translate(glm::mat4(1.0f), box.Center);
			glm::mat4 invWorld = glm::inverse(boxWorld);
			glm::vec3 localCenter = glm::vec3(invWorld * glm::vec4(center, 1.0f));

			glm::vec3 halfSize = box.Size * 0.5f;
			glm::vec3 clamped = glm::clamp(localCenter, -halfSize, halfSize);
			if (glm::distance(localCenter, clamped) <= radius)
			{
				if (addedHandles.insert(handle).second)
					outEntities.emplace_back(handle, &scene);
			}
		}
	}

	// 2. Sphere Colliders
	{
		auto view = registry.view<TagComponent, TransformComponent, SphereColliderComponent>();
		for (auto [handle, tag, tc, sphere] : view.each())
		{
			if (!tag.Active || (sphere.Layer & layerMask) == 0)
				continue;
			if (!includeTriggers && sphere.IsTrigger)
				continue;

			float maxScale = std::max({ tc.Scale.x, tc.Scale.y, tc.Scale.z });
			float worldRadius = sphere.Radius * maxScale;
			glm::vec3 worldCenter = glm::vec3(tc.GetMatrix() * glm::vec4(sphere.Center, 1.0f));

			if (glm::distance(center, worldCenter) <= (radius + worldRadius))
			{
				if (addedHandles.insert(handle).second)
					outEntities.emplace_back(handle, &scene);
			}
		}
	}

	// 3. Capsule Colliders
	{
		auto view = registry.view<TagComponent, TransformComponent, CapsuleColliderComponent>();
		for (auto [handle, tag, tc, capsule] : view.each())
		{
			if (!tag.Active || (capsule.Layer & layerMask) == 0)
				continue;
			if (!includeTriggers && capsule.IsTrigger)
				continue;

			glm::mat4 capWorld = tc.GetMatrix() * glm::translate(glm::mat4(1.0f), capsule.Center);
			glm::mat4 invWorld = glm::inverse(capWorld);
			glm::vec3 localCenter = glm::vec3(invWorld * glm::vec4(center, 1.0f));

			float halfH = std::max(0.0f, capsule.Height * 0.5f - capsule.Radius);
			glm::vec3 clampedSegment(0.0f, glm::clamp(localCenter.y, -halfH, halfH), 0.0f);
			if (glm::distance(localCenter, clampedSegment) <= (radius + capsule.Radius))
			{
				if (addedHandles.insert(handle).second)
					outEntities.emplace_back(handle, &scene);
			}
		}
	}

	// 4. Mesh Colliders
	{
		auto view = registry.view<TagComponent, TransformComponent, MeshColliderComponent, MeshComponent>();
		for (auto [handle, tag, tc, meshCol, mc] : view.each())
		{
			if (!tag.Active || !mc.ModelAsset || (meshCol.Layer & layerMask) == 0)
				continue;
			if (!includeTriggers && meshCol.IsTrigger)
				continue;

			glm::vec3 aabbMin(0.0f), aabbMax(0.0f);
			mc.GetWorldAABB(tc.GetMatrix(), aabbMin, aabbMax);

			glm::vec3 clamped = glm::clamp(center, aabbMin, aabbMax);
			if (glm::distance(center, clamped) <= radius)
			{
				if (addedHandles.insert(handle).second)
					outEntities.emplace_back(handle, &scene);
			}
		}
	}

	return !outEntities.empty();
}

bool OverlapBox(Scene& scene, const glm::vec3& center, const glm::vec3& halfExtents, std::vector<Entity>& outEntities, uint32_t layerMask, bool includeTriggers)
{
	outEntities.clear();
	std::unordered_set<entt::entity> addedHandles;
	auto& registry = scene.GetRegistry();

	glm::vec3 queryMin = center - halfExtents;
	glm::vec3 queryMax = center + halfExtents;

	// Check Box Colliders
	{
		auto view = registry.view<TagComponent, TransformComponent, BoxColliderComponent>();
		for (auto [handle, tag, tc, box] : view.each())
		{
			if (!tag.Active || (box.Layer & layerMask) == 0)
				continue;
			if (!includeTriggers && box.IsTrigger)
				continue;

			glm::vec3 boxPos = glm::vec3(tc.GetMatrix() * glm::vec4(box.Center, 1.0f));
			glm::vec3 bMin = boxPos - box.Size * 0.5f;
			glm::vec3 bMax = boxPos + box.Size * 0.5f;

			bool overlap = (queryMin.x <= bMax.x && queryMax.x >= bMin.x) &&
			               (queryMin.y <= bMax.y && queryMax.y >= bMin.y) &&
			               (queryMin.z <= bMax.z && queryMax.z >= bMin.z);

			if (overlap)
			{
				if (addedHandles.insert(handle).second)
					outEntities.emplace_back(handle, &scene);
			}
		}
	}

	return !outEntities.empty();
}

// ============================================================================
// Swept Capsule & Depenetration Queries
// ============================================================================

bool SweepCapsule(Scene& scene, const glm::vec3& startPos, const glm::vec3& displacement, float radius, float height, const glm::vec3& centerOffset, RaycastHit& outHit, Entity ignoreEntity, uint32_t layerMask)
{
	outHit.Hit = false;
	float dispLen = glm::length(displacement);
	if (dispLen < 1e-6f)
		return false;

	outHit.Distance = dispLen;

	glm::vec3 capCenter = startPos + centerOffset;
	float halfH = std::max(0.0f, height * 0.5f - radius);

	// Sample 5 spheres along capsule vertical axis
	const float samples[5] = { -halfH, -halfH * 0.5f, 0.0f, halfH * 0.5f, halfH };

	auto& registry = scene.GetRegistry();
	entt::entity ignoreHandle = ignoreEntity ? ignoreEntity.GetHandle() : entt::null;

	for (float yOff : samples)
	{
		glm::vec3 spherePos = capCenter + glm::vec3(0.0f, yOff, 0.0f);

		// 1. Box Colliders
		{
			auto view = registry.view<TagComponent, TransformComponent, BoxColliderComponent>();
			for (auto [handle, tag, tc, box] : view.each())
			{
				if (handle == ignoreHandle || !tag.Active || box.IsTrigger || (box.Layer & layerMask) == 0)
					continue;

				float dist = 0.0f;
				glm::vec3 norm(0.0f);
				if (IntersectSweptSphereOBB(spherePos, displacement, radius, box.Center, box.Size, tc.GetMatrix(), dist, norm))
				{
					if (dist >= 0.0f && dist < outHit.Distance)
					{
						outHit.Hit = true;
						outHit.Distance = dist;
						outHit.Normal = norm;
						outHit.Point = spherePos + glm::normalize(displacement) * dist;
						outHit.HitEntity = Entity{ handle, &scene };
					}
				}
			}
		}

		// 2. Sphere Colliders
		{
			auto view = registry.view<TagComponent, TransformComponent, SphereColliderComponent>();
			for (auto [handle, tag, tc, sphere] : view.each())
			{
				if (handle == ignoreHandle || !tag.Active || sphere.IsTrigger || (sphere.Layer & layerMask) == 0)
					continue;

				float maxScale = std::max({ tc.Scale.x, tc.Scale.y, tc.Scale.z });
				float worldRadius = sphere.Radius * maxScale;
				glm::vec3 worldCenter = glm::vec3(tc.GetMatrix() * glm::vec4(sphere.Center, 1.0f));

				float dist = 0.0f;
				glm::vec3 norm(0.0f);
				if (IntersectSweptSphereSphere(spherePos, displacement, radius, worldCenter, worldRadius, dist, norm))
				{
					if (dist >= 0.0f && dist < outHit.Distance)
					{
						outHit.Hit = true;
						outHit.Distance = dist;
						outHit.Normal = norm;
						outHit.Point = spherePos + glm::normalize(displacement) * dist;
						outHit.HitEntity = Entity{ handle, &scene };
					}
				}
			}
		}

		// 3. Capsule Colliders
		{
			auto view = registry.view<TagComponent, TransformComponent, CapsuleColliderComponent>();
			for (auto [handle, tag, tc, capsule] : view.each())
			{
				if (handle == ignoreHandle || !tag.Active || capsule.IsTrigger || (capsule.Layer & layerMask) == 0)
					continue;

				float dist = 0.0f;
				glm::vec3 norm(0.0f);
				if (IntersectSweptSphereCapsule(spherePos, displacement, radius, capsule.Center, capsule.Radius, capsule.Height, tc.GetMatrix(), dist, norm))
				{
					if (dist >= 0.0f && dist < outHit.Distance)
					{
						outHit.Hit = true;
						outHit.Distance = dist;
						outHit.Normal = norm;
						outHit.Point = spherePos + glm::normalize(displacement) * dist;
						outHit.HitEntity = Entity{ handle, &scene };
					}
				}
			}
		}

		// 4. Mesh Colliders
		{
			auto view = registry.view<TagComponent, TransformComponent, MeshColliderComponent, MeshComponent>();
			for (auto [handle, tag, tc, meshCol, mc] : view.each())
			{
				if (handle == ignoreHandle || !tag.Active || !mc.ModelAsset || meshCol.IsTrigger || (meshCol.Layer & layerMask) == 0)
					continue;

				glm::vec3 aabbMin(0.0f), aabbMax(0.0f);
				mc.GetWorldAABB(tc.GetMatrix(), aabbMin, aabbMax);

				float dist = 0.0f;
				glm::vec3 norm(0.0f);
				if (IntersectSweptSphereAABB(spherePos, displacement, radius, aabbMin, aabbMax, dist, norm))
				{
					if (dist >= 0.0f && dist < outHit.Distance)
					{
						outHit.Hit = true;
						outHit.Distance = dist;
						outHit.Normal = norm;
						outHit.Point = spherePos + glm::normalize(displacement) * dist;
						outHit.HitEntity = Entity{ handle, &scene };
					}
				}
			}
		}
	}

	return outHit.Hit;
}

bool CheckCapsuleOverlap(Scene& scene, const glm::vec3& pos, float radius, float height, const glm::vec3& centerOffset, glm::vec3& outPushVector, Entity ignoreEntity, uint32_t layerMask)
{
	outPushVector = glm::vec3(0.0f);
	glm::vec3 capCenter = pos + centerOffset;
	float halfH = std::max(0.0f, height * 0.5f - radius);
	const float samples[5] = { -halfH, -halfH * 0.5f, 0.0f, halfH * 0.5f, halfH };

	auto& registry = scene.GetRegistry();
	entt::entity ignoreHandle = ignoreEntity ? ignoreEntity.GetHandle() : entt::null;
	bool overlapped = false;

	for (float yOff : samples)
	{
		glm::vec3 spherePos = capCenter + glm::vec3(0.0f, yOff, 0.0f);

		// Box
		{
			auto view = registry.view<TagComponent, TransformComponent, BoxColliderComponent>();
			for (auto [handle, tag, tc, box] : view.each())
			{
				if (handle == ignoreHandle || !tag.Active || box.IsTrigger || (box.Layer & layerMask) == 0)
					continue;

				glm::mat4 boxWorld = tc.GetMatrix() * glm::translate(glm::mat4(1.0f), box.Center);
				glm::mat4 invWorld = glm::inverse(boxWorld);
				glm::vec3 localCenter = glm::vec3(invWorld * glm::vec4(spherePos, 1.0f));

				glm::vec3 halfSize = box.Size * 0.5f;
				glm::vec3 clamped = glm::clamp(localCenter, -halfSize, halfSize);
				float d = glm::distance(localCenter, clamped);

				if (d < radius)
				{
					float pen = radius - d;
					glm::vec3 localPush = (d > 1e-5f) ? (localCenter - clamped) / d : glm::vec3(0.0f, 1.0f, 0.0f);
					glm::vec3 worldPush = glm::normalize(glm::mat3(boxWorld) * localPush);
					outPushVector += worldPush * pen;
					overlapped = true;
				}
			}
		}

		// Sphere
		{
			auto view = registry.view<TagComponent, TransformComponent, SphereColliderComponent>();
			for (auto [handle, tag, tc, sphere] : view.each())
			{
				if (handle == ignoreHandle || !tag.Active || sphere.IsTrigger || (sphere.Layer & layerMask) == 0)
					continue;

				float maxScale = std::max({ tc.Scale.x, tc.Scale.y, tc.Scale.z });
				float worldRadius = sphere.Radius * maxScale;
				glm::vec3 worldCenter = glm::vec3(tc.GetMatrix() * glm::vec4(sphere.Center, 1.0f));

				float d = glm::distance(spherePos, worldCenter);
				float targetD = radius + worldRadius;
				if (d < targetD)
				{
					float pen = targetD - d;
					glm::vec3 push = (d > 1e-5f) ? glm::normalize(spherePos - worldCenter) : glm::vec3(0.0f, 1.0f, 0.0f);
					outPushVector += push * pen;
					overlapped = true;
				}
			}
		}

		// Mesh AABB
		{
			auto view = registry.view<TagComponent, TransformComponent, MeshColliderComponent, MeshComponent>();
			for (auto [handle, tag, tc, meshCol, mc] : view.each())
			{
				if (handle == ignoreHandle || !tag.Active || !mc.ModelAsset || meshCol.IsTrigger || (meshCol.Layer & layerMask) == 0)
					continue;

				glm::vec3 aabbMin(0.0f), aabbMax(0.0f);
				mc.GetWorldAABB(tc.GetMatrix(), aabbMin, aabbMax);

				glm::vec3 clamped = glm::clamp(spherePos, aabbMin, aabbMax);
				float d = glm::distance(spherePos, clamped);
				if (d < radius)
				{
					float pen = radius - d;
					glm::vec3 push = (d > 1e-5f) ? glm::normalize(spherePos - clamped) : glm::vec3(0.0f, 1.0f, 0.0f);
					outPushVector += push * pen;
					overlapped = true;
				}
			}
		}
	}

	return overlapped;
}

} // namespace Physics
