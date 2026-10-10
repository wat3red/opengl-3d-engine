#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include "components.h"

inline glm::mat4 RotationMatrixFromEuler(const glm::vec3& rotation)
{
	glm::mat4 m = glm::mat4(1.0f);
	m = glm::rotate(m, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
	m = glm::rotate(m, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
	m = glm::rotate(m, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
	return m;
}

inline glm::vec3 DirectionFromEuler(const glm::vec3& rotation)
{
	glm::mat4 m = RotationMatrixFromEuler(rotation);
	return glm::normalize(glm::vec3(m * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)));
}

inline glm::vec3 EulerFromDirection(const glm::vec3& direction)
{
	glm::vec3 f = glm::normalize(direction);
	float rx = glm::degrees(std::asin(glm::clamp(f.y, -1.0f, 1.0f)));
	float ry = glm::degrees(std::atan2(-f.x, -f.z));
	return glm::vec3(rx, ry, 0.0f);
}
