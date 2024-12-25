#include "rigidbody.h"
#include <iostream>

Rigidbody::Rigidbody(Model* model)
	: position(0.0f), velocity(0.0f), mass(1.f) {

	// Initialize bounding volumes based on the model's AABB and OBB
	aabb = model->GetAABB();
	obb = model->GetOBB();
}

void Rigidbody::ApplyForce(const glm::vec3& force) {
	glm::vec3 acceleration = force / GetMass();

	SetVelocity(GetVelocity() + acceleration);
}

void Rigidbody::Update(float deltaTime) {
	if (deltaTime <= 0.0f) return; // Avoid updating with invalid deltaTime

	//std::cout << "Initial Position: " << position.x << ", " << position.y << ", " << position.z << std::endl;
	//std::cout << "Initial Velocity: " << velocity.x << ", " << velocity.y << ", " << velocity.z << std::endl;

	SetPosition(GetPosition() + GetVelocity() * deltaTime);

	//std::cout << "Updated Position: " << position.x << ", " << position.y << ", " << position.z << std::endl;
}

bool Rigidbody::CheckCollision(const Rigidbody& other) const {
	if (aabb.max.x < other.aabb.min.x || aabb.min.x > other.aabb.max.x) return false;
	if (aabb.max.y < other.aabb.min.y || aabb.min.y > other.aabb.max.y) return false;
	if (aabb.max.z < other.aabb.min.z || aabb.min.z > other.aabb.max.z) return false;
	return true;
}

void Rigidbody::ResolveCollision(Rigidbody& other) {
	if (GetMass() == 0.0f || other.GetMass() == 0.0f) return;

	glm::vec3 collisionNormal = glm::normalize(other.GetPosition() - GetPosition());
	glm::vec3 relativeVelocity = other.GetVelocity() - GetVelocity();

	float velocityAlongNormal = glm::dot(relativeVelocity, collisionNormal);
	if (velocityAlongNormal > 0) return;

	float e = 1.0f; // Elasticity factor
	float j = -(1 + e) * velocityAlongNormal / (1 / GetMass() + 1 / other.GetMass());

	glm::vec3 impulse = j * collisionNormal;
	SetVelocity(GetVelocity() - impulse / GetMass());
	other.SetVelocity(other.GetVelocity() + impulse / other.GetMass());
}
