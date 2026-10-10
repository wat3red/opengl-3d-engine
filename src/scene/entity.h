#pragma once

#include <entt/entt.hpp>
#include <string>
#include <cassert>
#include "components.h"

class Scene;

class Entity {
public:
	Entity() = default;
	Entity(entt::entity handle, Scene* scene)
		: m_EntityHandle(handle), m_Scene(scene) {}
	Entity(const Entity& other) = default;

	template<typename T, typename... Args>
	T& AddComponent(Args&&... args);

	template<typename T, typename... Args>
	T& AddOrReplaceComponent(Args&&... args);

	template<typename T>
	T& GetComponent();

	template<typename T>
	const T& GetComponent() const;

	template<typename T>
	bool HasComponent() const;

	template<typename T>
	void RemoveComponent();

	operator bool() const { return m_EntityHandle != entt::null && m_Scene != nullptr; }
	operator entt::entity() const { return m_EntityHandle; }
	operator uint32_t() const { return static_cast<uint32_t>(m_EntityHandle); }

	entt::entity GetHandle() const { return m_EntityHandle; }
	Scene* GetScene() const { return m_Scene; }

	bool operator==(const Entity& other) const
	{
		return m_EntityHandle == other.m_EntityHandle && m_Scene == other.m_Scene;
	}

	bool operator!=(const Entity& other) const
	{
		return !(*this == other);
	}

	// High-level ergonomic convenience helpers
	const std::string& GetName() const;
	void SetName(const std::string& name);

	bool IsActive() const;
	void SetActive(bool active);

	TransformComponent& GetTransform();
	const TransformComponent& GetTransform() const;

	glm::mat4 GetModelMatrix() const;

	glm::vec3 GetPosition() const;
	void SetPosition(const glm::vec3& position);

	glm::vec3 GetRotation() const;
	void SetRotation(const glm::vec3& rotation);

	glm::vec3 GetScale() const;
	void SetScale(const glm::vec3& scale);

private:
	entt::entity m_EntityHandle{ entt::null };
	Scene* m_Scene{ nullptr };
};
