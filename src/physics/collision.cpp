#include "collision.h"
#include "scene/scene.h"
#include "scene/components.h"
#include <algorithm>
#include <cmath>

namespace Physics {

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

	float halfH = height * 0.5f;
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

bool Raycast(Scene& scene, const glm::vec3& origin, const glm::vec3& direction, float maxDistance, RaycastHit& outHit)
{
	Ray ray(origin, direction);
	return Raycast(scene, ray, maxDistance, outHit);
}

bool Raycast(Scene& scene, const Ray& ray, float maxDistance, RaycastHit& outHit)
{
	outHit.Hit = false;
	outHit.Distance = maxDistance;

	auto& registry = scene.GetRegistry();

	// 1. Test Box Colliders
	{
		auto view = registry.view<TagComponent, TransformComponent, BoxColliderComponent>();
		for (auto [handle, tag, tc, box] : view.each())
		{
			if (!tag.Active)
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

	// 2. Test Sphere Colliders
	{
		auto view = registry.view<TagComponent, TransformComponent, SphereColliderComponent>();
		for (auto [handle, tag, tc, sphere] : view.each())
		{
			if (!tag.Active)
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

	// 3. Test Capsule Colliders
	{
		auto view = registry.view<TagComponent, TransformComponent, CapsuleColliderComponent>();
		for (auto [handle, tag, tc, capsule] : view.each())
		{
			if (!tag.Active)
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

	// 4. Test Mesh Colliders (World AABB test)
	{
		auto view = registry.view<TagComponent, TransformComponent, MeshColliderComponent, MeshComponent>();
		for (auto [handle, tag, tc, meshCol, mc] : view.each())
		{
			if (!tag.Active || !mc.ModelAsset)
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

} // namespace Physics
