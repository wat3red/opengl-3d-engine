#pragma once

#include "shader.h"
#include "transform.h"
#include <string>

class Entity {
public:
	Entity(const std::string& name = "Entity")
		: m_Name(name), m_Active(true) {}

	virtual ~Entity() = default;

	virtual void Update(float deltaTime) {}
	virtual void Draw(Shader& shader) = 0;

	const std::string& GetName() const { return m_Name; }
	void SetName(const std::string& name) { m_Name = name; }

	bool IsActive() const { return m_Active; }
	void SetActive(bool active) { m_Active = active; }

	Transform& GetTransform() { return m_Transform; }
	const Transform& GetTransform() const { return m_Transform; }

	glm::vec3 GetPosition() const { return m_Transform.Position; }
	void SetPosition(const glm::vec3& position) { m_Transform.Position = position; }

	glm::vec3 GetRotation() const { return m_Transform.Rotation; }
	void SetRotation(const glm::vec3& rotation) { m_Transform.Rotation = rotation; }

	glm::vec3 GetScale() const { return m_Transform.Scale; }
	void SetScale(const glm::vec3& scale) { m_Transform.Scale = scale; }

	glm::mat4 GetModelMatrix() const { return m_Transform.GetMatrix(); }

protected:
	std::string m_Name;
	Transform m_Transform;
	bool m_Active;
};
