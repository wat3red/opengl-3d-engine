// Rigidbody.h
#pragma once
#include <glm/glm.hpp>

#include "model.h"

class Rigidbody {
public:
	Rigidbody(Model* model);

	void ApplyForce(const glm::vec3& force);
	void Update(float deltaTime);

	// Collision detection
	bool CheckCollision(const Rigidbody& other) const;

	// Collision response
	void ResolveCollision(Rigidbody& other);

	glm::vec3 GetPosition() const { return position; }
	void SetPosition(glm::vec3 val) { position = val; }

	glm::vec3 GetVelocity() const { return velocity; }
	void SetVelocity(glm::vec3 val) { velocity = val; }

	float GetMass() const { return mass; }
	void SetMass(float val) { mass = val; }
private:
	glm::vec3 position;
	glm::vec3 velocity;
	float mass;

	Model::AABB aabb;
	Model::OBB obb;
};
