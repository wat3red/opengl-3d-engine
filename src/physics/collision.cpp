#include "collision.h"
#include "scene/scene.h"
#include "scene/components.h"
#include "renderer/mesh.h"
#include "renderer/model.h"
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
	float largestT1 = std::numeric_limits<float>::lowest();
	glm::vec3 largestT1Normal(0.0f);

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

			if (t1 > largestT1)
			{
				largestT1 = t1;
				largestT1Normal = n;
			}

			if (t1 >= tmin)
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

	if (glm::length(normal) < 0.5f)
	{
		normal = (glm::length(largestT1Normal) > 0.5f) ? largestT1Normal : -ray.Direction;
	}

	outDistance = (tmin >= 0.0f) ? tmin : tmax;
	outNormal = normal;
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

bool IntersectRayTriangle(const Ray& ray, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, float& outDistance, glm::vec3& outNormal)
{
	glm::vec3 edge1 = b - a;
	glm::vec3 edge2 = c - a;
	glm::vec3 pvec = glm::cross(ray.Direction, edge2);
	float det = glm::dot(edge1, pvec);

	if (std::abs(det) < 1e-7f)
		return false;

	float invDet = 1.0f / det;
	glm::vec3 tvec = ray.Origin - a;
	float u = glm::dot(tvec, pvec) * invDet;
	if (u < 0.0f || u > 1.0f)
		return false;

	glm::vec3 qvec = glm::cross(tvec, edge1);
	float v = glm::dot(ray.Direction, qvec) * invDet;
	if (v < 0.0f || u + v > 1.0f)
		return false;

	float t = glm::dot(edge2, qvec) * invDet;
	if (t < 0.0f)
		return false;

	outDistance = t;
	glm::vec3 rawNorm = glm::cross(edge1, edge2);
	float len = glm::length(rawNorm);
	glm::vec3 geomNorm = (len > 1e-6f) ? (rawNorm / len) : glm::vec3(0.0f, 1.0f, 0.0f);
	outNormal = (glm::dot(geomNorm, ray.Direction) < 0.0f) ? geomNorm : -geomNorm;
	return true;
}

glm::vec3 ClosestPointOnTriangle(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
{
	glm::vec3 ab = b - a;
	glm::vec3 ac = c - a;
	glm::vec3 ap = p - a;
	float d1 = glm::dot(ab, ap);
	float d2 = glm::dot(ac, ap);
	if (d1 <= 0.0f && d2 <= 0.0f) return a;

	glm::vec3 bp = p - b;
	float d3 = glm::dot(ab, bp);
	float d4 = glm::dot(ac, bp);
	if (d3 >= 0.0f && d4 <= d3) return b;

	float vc = d1 * d4 - d3 * d2;
	if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
	{
		float v = d1 / (d1 - d3);
		return a + v * ab;
	}

	glm::vec3 cp = p - c;
	float d5 = glm::dot(ab, cp);
	float d6 = glm::dot(ac, cp);
	if (d6 >= 0.0f && d5 <= d6) return c;

	float vb = d5 * d2 - d1 * d6;
	if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
	{
		float w = d2 / (d2 - d6);
		return a + w * ac;
	}

	float va = d3 * d6 - d5 * d4;
	if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
	{
		float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
		return b + w * (c - b);
	}

	float denom = 1.0f / (va + vb + vc);
	float v = vb * denom;
	float w = vc * denom;
	return a + ab * v + ac * w;
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

static bool IntersectSweptSphereSegment(const glm::vec3& start, const glm::vec3& dir, float dispLen, float radius,
	const glm::vec3& p0, const glm::vec3& p1, float& outDist, glm::vec3& outNormal)
{
	glm::vec3 seg = p1 - p0;
	float segLen = glm::length(seg);
	if (segLen < 1e-6f) return false;
	glm::vec3 ue = seg / segLen;

	glm::vec3 w = (start - p0) - glm::dot(start - p0, ue) * ue;
	glm::vec3 dPerp = dir - glm::dot(dir, ue) * ue;

	float A = glm::dot(dPerp, dPerp);
	if (A < 1e-7f) return false;

	float B = 2.0f * glm::dot(w, dPerp);
	float C = glm::dot(w, w) - radius * radius;
	float disc = B * B - 4.0f * A * C;
	if (disc < 0.0f) return false;

	float t = (-B - std::sqrt(disc)) / (2.0f * A);
	if (t >= 0.0f && t <= dispLen)
	{
		float s = glm::dot(start + t * dir - p0, ue);
		if (s >= 0.0f && s <= segLen)
		{
			glm::vec3 ptOnSeg = p0 + s * ue;
			glm::vec3 centerAtHit = start + t * dir;
			glm::vec3 norm = centerAtHit - ptOnSeg;
			float normLen = glm::length(norm);
			outDist = t;
			outNormal = (normLen > 1e-5f) ? (norm / normLen) : glm::vec3(0.0f, 1.0f, 0.0f);
			return true;
		}
	}
	return false;
}

bool IntersectSweptSphereTriangle(const glm::vec3& start, const glm::vec3& displacement, float radius,
	const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, float& outDistance, glm::vec3& outNormal)
{
	float dispLen = glm::length(displacement);
	if (dispLen < 1e-7f) return false;
	glm::vec3 dir = displacement / dispLen;
	Ray ray(start, dir);

	glm::vec3 edge1 = b - a;
	glm::vec3 edge2 = c - a;
	glm::vec3 rawNorm = glm::cross(edge1, edge2);
	float nLen = glm::length(rawNorm);
	if (nLen < 1e-6f) return false;
	glm::vec3 normal = rawNorm / nLen;

	float bestDist = dispLen + 1.0f;
	glm::vec3 bestNormal(0.0f);
	bool hit = false;

	// Check if already touching or penetrating at start
	float dStartPlane = glm::dot(normal, start - a);
	if (std::abs(dStartPlane) <= radius + 0.05f)
	{
		glm::vec3 closest = ClosestPointOnTriangle(start, a, b, c);
		float dClosest = glm::distance(start, closest);
		if (dClosest <= radius)
		{
			glm::vec3 sepDir = (dClosest > 1e-5f) ? glm::normalize(start - closest) : normal;
			if (glm::dot(dir, sepDir) <= 0.0f)
			{
				outDistance = 0.0f;
				outNormal = sepDir;
				return true;
			}
		}
	}

	// 1. Face test (both sides)
	float ndotd = glm::dot(normal, dir);
	if (std::abs(ndotd) > 1e-6f)
	{
		float dPlane = glm::dot(normal, a);
		float startDist = glm::dot(normal, start) - dPlane;

		if (startDist >= radius && ndotd < 0.0f)
		{
			float planeDist = (startDist - radius) / (-ndotd);
			if (planeDist >= 0.0f && planeDist <= dispLen && planeDist < bestDist)
			{
				glm::vec3 spherePosAtHit = start + dir * planeDist;
				glm::vec3 ptOnTri = spherePosAtHit - normal * radius;
				glm::vec3 closest = ClosestPointOnTriangle(ptOnTri, a, b, c);
				if (glm::distance(ptOnTri, closest) < 0.01f)
				{
					bestDist = planeDist;
					bestNormal = normal;
					hit = true;
				}
			}
		}
		else if (startDist <= -radius && ndotd > 0.0f)
		{
			float planeDist = (-startDist - radius) / ndotd;
			if (planeDist >= 0.0f && planeDist <= dispLen && planeDist < bestDist)
			{
				glm::vec3 spherePosAtHit = start + dir * planeDist;
				glm::vec3 ptOnTri = spherePosAtHit + normal * radius;
				glm::vec3 closest = ClosestPointOnTriangle(ptOnTri, a, b, c);
				if (glm::distance(ptOnTri, closest) < 0.01f)
				{
					bestDist = planeDist;
					bestNormal = -normal;
					hit = true;
				}
			}
		}
	}

	// 2. Vertex tests
	const glm::vec3 verts[3] = { a, b, c };
	for (int i = 0; i < 3; ++i)
	{
		float dVert = 0.0f;
		glm::vec3 nVert(0.0f);
		if (IntersectRaySphere(ray, verts[i], radius, dVert, nVert))
		{
			if (dVert >= 0.0f && dVert <= dispLen && dVert < bestDist)
			{
				bestDist = dVert;
				bestNormal = nVert;
				hit = true;
			}
		}
	}

	// 3. Edge tests
	float dEdge = 0.0f;
	glm::vec3 nEdge(0.0f);
	if (IntersectSweptSphereSegment(start, dir, dispLen, radius, a, b, dEdge, nEdge))
	{
		if (dEdge < bestDist) { bestDist = dEdge; bestNormal = nEdge; hit = true; }
	}
	if (IntersectSweptSphereSegment(start, dir, dispLen, radius, b, c, dEdge, nEdge))
	{
		if (dEdge < bestDist) { bestDist = dEdge; bestNormal = nEdge; hit = true; }
	}
	if (IntersectSweptSphereSegment(start, dir, dispLen, radius, c, a, dEdge, nEdge))
	{
		if (dEdge < bestDist) { bestDist = dEdge; bestNormal = nEdge; hit = true; }
	}

	if (hit)
	{
		outDistance = bestDist;
		outNormal = bestNormal;
		return true;
	}
	return false;
}

// ============================================================================
// Mesh BVH Implementation
// ============================================================================

void MeshBVH::Build(const std::vector<::Mesh>& meshes)
{
	m_Triangles.clear();
	m_Nodes.clear();

	for (const auto& mesh : meshes)
	{
		if (!mesh.indices.empty())
		{
			for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
			{
				unsigned int i0 = mesh.indices[i];
				unsigned int i1 = mesh.indices[i + 1];
				unsigned int i2 = mesh.indices[i + 2];
				if (i0 < mesh.vertices.size() && i1 < mesh.vertices.size() && i2 < mesh.vertices.size())
				{
					BVHTriangle tri;
					tri.V0 = mesh.vertices[i0].Position;
					tri.V1 = mesh.vertices[i1].Position;
					tri.V2 = mesh.vertices[i2].Position;
					glm::vec3 rawNorm = glm::cross(tri.V1 - tri.V0, tri.V2 - tri.V0);
					if (glm::length(rawNorm) < 1e-7f)
						continue;

					tri.Min = glm::min(glm::min(tri.V0, tri.V1), tri.V2);
					tri.Max = glm::max(glm::max(tri.V0, tri.V1), tri.V2);
					tri.Centroid = (tri.V0 + tri.V1 + tri.V2) * (1.0f / 3.0f);
					m_Triangles.push_back(tri);
				}
			}
		}
		else
		{
			for (size_t i = 0; i + 2 < mesh.vertices.size(); i += 3)
			{
				BVHTriangle tri;
				tri.V0 = mesh.vertices[i].Position;
				tri.V1 = mesh.vertices[i + 1].Position;
				tri.V2 = mesh.vertices[i + 2].Position;
				glm::vec3 rawNorm = glm::cross(tri.V1 - tri.V0, tri.V2 - tri.V0);
				if (glm::length(rawNorm) < 1e-7f)
					continue;

				tri.Min = glm::min(glm::min(tri.V0, tri.V1), tri.V2);
				tri.Max = glm::max(glm::max(tri.V0, tri.V1), tri.V2);
				tri.Centroid = (tri.V0 + tri.V1 + tri.V2) * (1.0f / 3.0f);
				m_Triangles.push_back(tri);
			}
		}
	}

	if (!m_Triangles.empty())
	{
		m_Nodes.reserve(m_Triangles.size() * 2);
		BuildRecursive(0, static_cast<int>(m_Triangles.size()), 0);
	}
}

int MeshBVH::BuildRecursive(int start, int end, int depth)
{
	int count = end - start;
	if (count <= 0) return -1;

	int nodeIndex = static_cast<int>(m_Nodes.size());
	m_Nodes.emplace_back();

	glm::vec3 boxMin(std::numeric_limits<float>::max());
	glm::vec3 boxMax(std::numeric_limits<float>::lowest());
	glm::vec3 centroidMin(std::numeric_limits<float>::max());
	glm::vec3 centroidMax(std::numeric_limits<float>::lowest());

	for (int i = start; i < end; ++i)
	{
		boxMin = glm::min(boxMin, m_Triangles[i].Min);
		boxMax = glm::max(boxMax, m_Triangles[i].Max);
		centroidMin = glm::min(centroidMin, m_Triangles[i].Centroid);
		centroidMax = glm::max(centroidMax, m_Triangles[i].Centroid);
	}

	m_Nodes[nodeIndex].AABBMin = boxMin;
	m_Nodes[nodeIndex].AABBMax = boxMax;

	constexpr int MAX_LEAF_TRIS = 4;
	constexpr int MAX_DEPTH = 32;

	if (count <= MAX_LEAF_TRIS || depth >= MAX_DEPTH)
	{
		m_Nodes[nodeIndex].LeftChild = ~start;
		m_Nodes[nodeIndex].RightChild = count;
		return nodeIndex;
	}

	glm::vec3 extent = centroidMax - centroidMin;
	int axis = 0;
	if (extent.y > extent.x && extent.y > extent.z) axis = 1;
	else if (extent.z > extent.x && extent.z > extent.y) axis = 2;

	int mid = (start + end) / 2;
	std::nth_element(
		m_Triangles.begin() + start,
		m_Triangles.begin() + mid,
		m_Triangles.begin() + end,
		[axis](const BVHTriangle& a, const BVHTriangle& b) {
			return a.Centroid[axis] < b.Centroid[axis];
		}
	);

	int left = BuildRecursive(start, mid, depth + 1);
	int right = BuildRecursive(mid, end, depth + 1);

	m_Nodes[nodeIndex].LeftChild = left;
	m_Nodes[nodeIndex].RightChild = right;
	return nodeIndex;
}

bool MeshBVH::Raycast(const Ray& localRay, float maxDist, float& outDist, glm::vec3& outNormal) const
{
	if (m_Nodes.empty()) return false;

	float bestDist = maxDist;
	glm::vec3 bestNormal(0.0f);
	bool hitAny = false;

	int stack[64];
	int stackPtr = 0;
	stack[stackPtr++] = 0;

	while (stackPtr > 0)
	{
		int nodeIdx = stack[--stackPtr];
		const auto& node = m_Nodes[nodeIdx];

		float boxDist = 0.0f;
		glm::vec3 boxNorm(0.0f);
		if (!IntersectRayAABB(localRay, node.AABBMin, node.AABBMax, boxDist, boxNorm))
			continue;

		if (boxDist > bestDist)
			continue;

		if (node.IsLeaf())
		{
			int start = ~node.LeftChild;
			int count = node.RightChild;
			for (int i = 0; i < count; ++i)
			{
				const auto& tri = m_Triangles[start + i];
				float triDist = 0.0f;
				glm::vec3 triNorm(0.0f);
				if (IntersectRayTriangle(localRay, tri.V0, tri.V1, tri.V2, triDist, triNorm))
				{
					if (triDist >= 0.0f && triDist < bestDist)
					{
						bestDist = triDist;
						bestNormal = triNorm;
						hitAny = true;
					}
				}
			}
		}
		else
		{
			if (node.RightChild >= 0) stack[stackPtr++] = node.RightChild;
			if (node.LeftChild >= 0) stack[stackPtr++] = node.LeftChild;
		}
	}

	if (hitAny)
	{
		outDist = bestDist;
		outNormal = bestNormal;
		return true;
	}
	return false;
}

void MeshBVH::QueryBox(const glm::vec3& localMin, const glm::vec3& localMax, std::vector<int>& outTriIndices) const
{
	if (m_Nodes.empty()) return;

	int stack[64];
	int stackPtr = 0;
	stack[stackPtr++] = 0;

	while (stackPtr > 0)
	{
		int nodeIdx = stack[--stackPtr];
		const auto& node = m_Nodes[nodeIdx];

		if (localMax.x < node.AABBMin.x || localMin.x > node.AABBMax.x ||
			localMax.y < node.AABBMin.y || localMin.y > node.AABBMax.y ||
			localMax.z < node.AABBMin.z || localMin.z > node.AABBMax.z)
			continue;

		if (node.IsLeaf())
		{
			int start = ~node.LeftChild;
			int count = node.RightChild;
			for (int i = 0; i < count; ++i)
			{
				outTriIndices.push_back(start + i);
			}
		}
		else
		{
			if (node.RightChild >= 0) stack[stackPtr++] = node.RightChild;
			if (node.LeftChild >= 0) stack[stackPtr++] = node.LeftChild;
		}
	}
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

	// 4. Mesh Colliders (BVH Triangle Mesh)
	{
		auto view = registry.view<TagComponent, TransformComponent, MeshColliderComponent, MeshComponent>();
		for (auto [handle, tag, tc, meshCol, mc] : view.each())
		{
			if (!tag.Active || !mc.ModelAsset || (meshCol.Layer & layerMask) == 0)
				continue;
			if (!includeTriggers && meshCol.IsTrigger)
				continue;

			const auto* bvh = mc.ModelAsset->GetBVH();
			if (!bvh)
				continue;

			glm::mat4 modelMatrix = tc.GetMatrix();
			glm::mat4 invMatrix = glm::inverse(modelMatrix);

			Ray localRay;
			localRay.Origin = glm::vec3(invMatrix * glm::vec4(ray.Origin, 1.0f));
			localRay.Direction = glm::vec3(invMatrix * glm::vec4(ray.Direction, 0.0f));
			float localDirLen = glm::length(localRay.Direction);
			if (localDirLen < 1e-6f)
				continue;
			localRay.Direction /= localDirLen;

			float maxLocalDist = (maxDistance < std::numeric_limits<float>::max() * 0.5f) ? (maxDistance * localDirLen) : std::numeric_limits<float>::max();
			float localDist = 0.0f;
			glm::vec3 localNorm(0.0f);

			if (bvh->Raycast(localRay, maxLocalDist, localDist, localNorm))
			{
				glm::vec3 localHitPoint = localRay.GetPoint(localDist);
				glm::vec3 worldHitPoint = glm::vec3(modelMatrix * glm::vec4(localHitPoint, 1.0f));
				float worldDist = glm::distance(worldHitPoint, ray.Origin);

				if (worldDist >= 0.0f && worldDist <= outHit.Distance)
				{
					glm::mat3 normMat = glm::transpose(glm::inverse(glm::mat3(modelMatrix)));
					glm::vec3 worldNorm = glm::normalize(normMat * localNorm);

					outHit.Hit = true;
					outHit.Distance = worldDist;
					outHit.Point = worldHitPoint;
					outHit.Normal = worldNorm;
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

	// Mesh Colliders (BVH Triangle Mesh)
	{
		auto view = registry.view<TagComponent, TransformComponent, MeshColliderComponent, MeshComponent>();
		for (auto [handle, tag, tc, meshCol, mc] : view.each())
		{
			if (!tag.Active || !mc.ModelAsset || (meshCol.Layer & layerMask) == 0)
				continue;
			if (!includeTriggers && meshCol.IsTrigger)
				continue;

			const auto* bvh = mc.ModelAsset->GetBVH();
			if (!bvh)
				continue;

			glm::mat4 modelMatrix = tc.GetMatrix();
			glm::mat4 invMatrix = glm::inverse(modelMatrix);

			Ray localRay;
			localRay.Origin = glm::vec3(invMatrix * glm::vec4(ray.Origin, 1.0f));
			localRay.Direction = glm::vec3(invMatrix * glm::vec4(ray.Direction, 0.0f));
			float localDirLen = glm::length(localRay.Direction);
			if (localDirLen < 1e-6f)
				continue;
			localRay.Direction /= localDirLen;

			float maxLocalDist = (maxDistance < std::numeric_limits<float>::max() * 0.5f) ? (maxDistance * localDirLen) : std::numeric_limits<float>::max();
			float localDist = 0.0f;
			glm::vec3 localNorm(0.0f);

			if (bvh->Raycast(localRay, maxLocalDist, localDist, localNorm))
			{
				glm::vec3 localHitPoint = localRay.GetPoint(localDist);
				glm::vec3 worldHitPoint = glm::vec3(modelMatrix * glm::vec4(localHitPoint, 1.0f));
				float worldDist = glm::distance(worldHitPoint, ray.Origin);

				if (worldDist <= maxDistance)
				{
					glm::mat3 normMat = glm::transpose(glm::inverse(glm::mat3(modelMatrix)));
					glm::vec3 worldNorm = glm::normalize(normMat * localNorm);

					RaycastHit hit;
					hit.Hit = true;
					hit.Distance = worldDist;
					hit.Point = worldHitPoint;
					hit.Normal = worldNorm;
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

	// 4. Mesh Colliders (BVH Triangle Mesh)
	{
		auto view = registry.view<TagComponent, TransformComponent, MeshColliderComponent, MeshComponent>();
		for (auto [handle, tag, tc, meshCol, mc] : view.each())
		{
			if (!tag.Active || !mc.ModelAsset || (meshCol.Layer & layerMask) == 0)
				continue;
			if (!includeTriggers && meshCol.IsTrigger)
				continue;

			const auto* bvh = mc.ModelAsset->GetBVH();
			if (!bvh)
				continue;

			glm::mat4 modelMatrix = tc.GetMatrix();
			glm::mat4 invMatrix = glm::inverse(modelMatrix);

			glm::vec3 wMin = center - glm::vec3(radius);
			glm::vec3 wMax = center + glm::vec3(radius);

			glm::vec3 lMin(std::numeric_limits<float>::max());
			glm::vec3 lMax(std::numeric_limits<float>::lowest());
			for (int c = 0; c < 8; ++c)
			{
				glm::vec3 corner(
					(c & 1) ? wMax.x : wMin.x,
					(c & 2) ? wMax.y : wMin.y,
					(c & 4) ? wMax.z : wMin.z
				);
				glm::vec3 localCorner = glm::vec3(invMatrix * glm::vec4(corner, 1.0f));
				lMin = glm::min(lMin, localCorner);
				lMax = glm::max(lMax, localCorner);
			}

			std::vector<int> triIndices;
			bvh->QueryBox(lMin, lMax, triIndices);

			for (int triIdx : triIndices)
			{
				const auto& tri = bvh->GetTriangles()[triIdx];
				glm::vec3 wA = glm::vec3(modelMatrix * glm::vec4(tri.V0, 1.0f));
				glm::vec3 wB = glm::vec3(modelMatrix * glm::vec4(tri.V1, 1.0f));
				glm::vec3 wC = glm::vec3(modelMatrix * glm::vec4(tri.V2, 1.0f));

				glm::vec3 closest = ClosestPointOnTriangle(center, wA, wB, wC);
				if (glm::distance(center, closest) <= radius)
				{
					if (addedHandles.insert(handle).second)
						outEntities.emplace_back(handle, &scene);
					break;
				}
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

		// 4. Mesh Colliders (BVH Triangle Mesh)
		{
			auto view = registry.view<TagComponent, TransformComponent, MeshColliderComponent, MeshComponent>();
			for (auto [handle, tag, tc, meshCol, mc] : view.each())
			{
				if (handle == ignoreHandle || !tag.Active || !mc.ModelAsset || meshCol.IsTrigger || (meshCol.Layer & layerMask) == 0)
					continue;

				const auto* bvh = mc.ModelAsset->GetBVH();
				if (!bvh)
					continue;

				glm::mat4 modelMatrix = tc.GetMatrix();
				glm::mat4 invMatrix = glm::inverse(modelMatrix);

				glm::vec3 wMin = glm::min(spherePos, spherePos + displacement) - glm::vec3(radius);
				glm::vec3 wMax = glm::max(spherePos, spherePos + displacement) + glm::vec3(radius);

				glm::vec3 lMin(std::numeric_limits<float>::max());
				glm::vec3 lMax(std::numeric_limits<float>::lowest());
				for (int c = 0; c < 8; ++c)
				{
					glm::vec3 corner(
						(c & 1) ? wMax.x : wMin.x,
						(c & 2) ? wMax.y : wMin.y,
						(c & 4) ? wMax.z : wMin.z
					);
					glm::vec3 localCorner = glm::vec3(invMatrix * glm::vec4(corner, 1.0f));
					lMin = glm::min(lMin, localCorner);
					lMax = glm::max(lMax, localCorner);
				}

				std::vector<int> triIndices;
				bvh->QueryBox(lMin, lMax, triIndices);

				for (int triIdx : triIndices)
				{
					const auto& tri = bvh->GetTriangles()[triIdx];
					glm::vec3 wA = glm::vec3(modelMatrix * glm::vec4(tri.V0, 1.0f));
					glm::vec3 wB = glm::vec3(modelMatrix * glm::vec4(tri.V1, 1.0f));
					glm::vec3 wC = glm::vec3(modelMatrix * glm::vec4(tri.V2, 1.0f));

					float dist = 0.0f;
					glm::vec3 norm(0.0f);
					if (IntersectSweptSphereTriangle(spherePos, displacement, radius, wA, wB, wC, dist, norm))
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

		// Mesh Colliders (BVH Triangle Mesh)
		{
			auto view = registry.view<TagComponent, TransformComponent, MeshColliderComponent, MeshComponent>();
			for (auto [handle, tag, tc, meshCol, mc] : view.each())
			{
				if (handle == ignoreHandle || !tag.Active || !mc.ModelAsset || meshCol.IsTrigger || (meshCol.Layer & layerMask) == 0)
					continue;

				const auto* bvh = mc.ModelAsset->GetBVH();
				if (!bvh)
					continue;

				glm::mat4 modelMatrix = tc.GetMatrix();
				glm::mat4 invMatrix = glm::inverse(modelMatrix);

				glm::vec3 wMin = spherePos - glm::vec3(radius);
				glm::vec3 wMax = spherePos + glm::vec3(radius);

				glm::vec3 lMin(std::numeric_limits<float>::max());
				glm::vec3 lMax(std::numeric_limits<float>::lowest());
				for (int c = 0; c < 8; ++c)
				{
					glm::vec3 corner(
						(c & 1) ? wMax.x : wMin.x,
						(c & 2) ? wMax.y : wMin.y,
						(c & 4) ? wMax.z : wMin.z
					);
					glm::vec3 localCorner = glm::vec3(invMatrix * glm::vec4(corner, 1.0f));
					lMin = glm::min(lMin, localCorner);
					lMax = glm::max(lMax, localCorner);
				}

				std::vector<int> triIndices;
				bvh->QueryBox(lMin, lMax, triIndices);

				for (int triIdx : triIndices)
				{
					const auto& tri = bvh->GetTriangles()[triIdx];
					glm::vec3 wA = glm::vec3(modelMatrix * glm::vec4(tri.V0, 1.0f));
					glm::vec3 wB = glm::vec3(modelMatrix * glm::vec4(tri.V1, 1.0f));
					glm::vec3 wC = glm::vec3(modelMatrix * glm::vec4(tri.V2, 1.0f));

					glm::vec3 closest = ClosestPointOnTriangle(spherePos, wA, wB, wC);
					float d = glm::distance(spherePos, closest);
					if (d < radius)
					{
						float pen = radius - d;
						glm::vec3 triNorm = glm::cross(wB - wA, wC - wA);
						float triNormLen = glm::length(triNorm);
						glm::vec3 fallbackNorm = (triNormLen > 1e-6f) ? (triNorm / triNormLen) : glm::vec3(0.0f, 1.0f, 0.0f);
						glm::vec3 push = (d > 1e-5f) ? ((spherePos - closest) / d) : fallbackNorm;
						outPushVector += push * pen;
						overlapped = true;
					}
				}
			}
		}
	}

	return overlapped;
}

} // namespace Physics
