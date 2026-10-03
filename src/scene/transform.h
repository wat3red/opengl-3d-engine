#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Transform {
public:
	glm::vec3 Position = glm::vec3(0.0f);
	glm::vec3 Rotation = glm::vec3(0.0f); // Euler angles in degrees
	glm::vec3 Scale = glm::vec3(1.0f);

	Transform() = default;
	Transform(const glm::vec3& position, const glm::vec3& rotation = glm::vec3(0.0f), const glm::vec3& scale = glm::vec3(1.0f))
		: Position(position), Rotation(rotation), Scale(scale) {}

	[[nodiscard]] glm::mat4 GetMatrix() const
	{
		glm::mat4 matrix = glm::mat4(1.0f);
		matrix = glm::translate(matrix, Position);
		matrix = glm::rotate(matrix, glm::radians(Rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		matrix = glm::rotate(matrix, glm::radians(Rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		matrix = glm::rotate(matrix, glm::radians(Rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
		matrix = glm::scale(matrix, Scale);
		return matrix;
	}

	bool operator==(const Transform&) const = default;
};
