// entity.h
#pragma once

#include "shader.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>


class Entity {
public:
	Entity() : position(0.0f), scale(1.0f), rotation(0.0f) {
		UpdateModelMatrix();
	}

	virtual ~Entity() = default;

	virtual void Draw(Shader& shader) = 0;

	void SetPosition(const glm::vec3& pos) { position = pos; UpdateModelMatrix(); }
	void SetScale(const glm::vec3& scl) { scale = scl; UpdateModelMatrix(); }
	void SetRotation(const glm::vec3& rot) { rotation = rot; UpdateModelMatrix(); }

	glm::vec3 GetPosition() const { return position; }
	glm::vec3 GetScale() const { return scale; }
	glm::vec3 GetRotation() const { return rotation; }
	glm::mat4 GetModelMatrix() const { return modelMatrix; }
	
protected:
	glm::mat4 modelMatrix;

	void UpdateModelMatrix() {
		modelMatrix = glm::mat4(1.0f);
		modelMatrix = glm::translate(modelMatrix, position);
		modelMatrix = glm::rotate(modelMatrix, glm::radians(rotation.x), glm::vec3(1, 0, 0));
		modelMatrix = glm::rotate(modelMatrix, glm::radians(rotation.y), glm::vec3(0, 1, 0));
		modelMatrix = glm::rotate(modelMatrix, glm::radians(rotation.z), glm::vec3(0, 0, 1));
		modelMatrix = glm::scale(modelMatrix, scale);
	}

private:
	glm::vec3 position;
	glm::vec3 scale;
	glm::vec3 rotation;
};
