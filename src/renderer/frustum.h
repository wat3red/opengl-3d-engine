#pragma once

#include <glm/glm.hpp>
#include <array>

struct FrustumPlane {
	glm::vec3 normal;
	float distance;

	FrustumPlane() : normal(0.0f, 1.0f, 0.0f), distance(0.0f) {}
	FrustumPlane(const glm::vec3& n, float d) : normal(n), distance(d) {}

	FrustumPlane(const glm::vec4& p) {
		float length = glm::length(glm::vec3(p));
		if (length > 0.0f) {
			normal = glm::vec3(p) / length;
			distance = p.w / length;
		} else {
			normal = glm::vec3(0.0f, 1.0f, 0.0f);
			distance = 0.0f;
		}
	}

	float DistanceToPoint(const glm::vec3& point) const {
		return glm::dot(normal, point) + distance;
	}
};

class Frustum {
public:
	Frustum() = default;

	// Extract 6 frustum planes from View-Projection matrix (Gribb-Hartmann method for OpenGL NDC [-1, 1])
	static Frustum FromViewProjection(const glm::mat4& vp) {
		Frustum frustum;

		// Note: GLM matrices are column-major (vp[col][row])
		// Left plane: row4 + row1
		frustum.planes[0] = FrustumPlane(glm::vec4(
			vp[0][3] + vp[0][0],
			vp[1][3] + vp[1][0],
			vp[2][3] + vp[2][0],
			vp[3][3] + vp[3][0]
		));

		// Right plane: row4 - row1
		frustum.planes[1] = FrustumPlane(glm::vec4(
			vp[0][3] - vp[0][0],
			vp[1][3] - vp[1][0],
			vp[2][3] - vp[2][0],
			vp[3][3] - vp[3][0]
		));

		// Bottom plane: row4 + row2
		frustum.planes[2] = FrustumPlane(glm::vec4(
			vp[0][3] + vp[0][1],
			vp[1][3] + vp[1][1],
			vp[2][3] + vp[2][1],
			vp[3][3] + vp[3][1]
		));

		// Top plane: row4 - row2
		frustum.planes[3] = FrustumPlane(glm::vec4(
			vp[0][3] - vp[0][1],
			vp[1][3] - vp[1][1],
			vp[2][3] - vp[2][1],
			vp[3][3] - vp[3][1]
		));

		// Near plane: row4 + row3 (for OpenGL [-1, 1] range)
		frustum.planes[4] = FrustumPlane(glm::vec4(
			vp[0][3] + vp[0][2],
			vp[1][3] + vp[1][2],
			vp[2][3] + vp[2][2],
			vp[3][3] + vp[3][2]
		));

		// Far plane: row4 - row3
		frustum.planes[5] = FrustumPlane(glm::vec4(
			vp[0][3] - vp[0][2],
			vp[1][3] - vp[1][2],
			vp[2][3] - vp[2][2],
			vp[3][3] - vp[3][2]
		));

		return frustum;
	}

	bool IsPointInside(const glm::vec3& point) const {
		for (const auto& plane : planes) {
			if (plane.DistanceToPoint(point) < 0.0f)
				return false;
		}
		return true;
	}

	bool IsSphereVisible(const glm::vec3& center, float radius) const {
		for (const auto& plane : planes) {
			if (plane.DistanceToPoint(center) < -radius)
				return false;
		}
		return true;
	}

	// High performance AABB frustum test: tests positive vertex along plane normal
	bool IsAABBVisible(const glm::vec3& min, const glm::vec3& max) const {
		for (const auto& plane : planes) {
			glm::vec3 positiveVertex(
				plane.normal.x >= 0.0f ? max.x : min.x,
				plane.normal.y >= 0.0f ? max.y : min.y,
				plane.normal.z >= 0.0f ? max.z : min.z
			);

			if (plane.DistanceToPoint(positiveVertex) < 0.0f)
				return false; // Bounding box is completely outside this plane
		}
		return true;
	}

	const std::array<FrustumPlane, 6>& GetPlanes() const { return planes; }

private:
	std::array<FrustumPlane, 6> planes;
};
