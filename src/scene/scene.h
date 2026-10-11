#pragma once

#include <entt/entt.hpp>
#include <vector>
#include <memory>
#include <string>

#include "components.h"
#include "entity.h"
#include "skybox.h"

class Scene {
public:
	Scene();
	~Scene();

	Entity CreateEntity(const std::string& name = "Entity");
	void DestroyEntity(Entity entity);

	void Update(float deltaTime);
	void Clear();

	// Runtime Lifecycle
	void OnRuntimeStart();
	void OnUpdateRuntime(float deltaTime);
	void OnRuntimeGUI();
	void OnRuntimeStop();
	void OnUpdateEditor(float deltaTime);

	Entity GetPrimaryCameraEntity();

	// Scene Cloning / State Isolation
	void DuplicateFrom(const Scene& other);
	static std::shared_ptr<Scene> Copy(const std::shared_ptr<Scene>& other);

	Entity GetSelectedEntity() const { return m_SelectedEntity; }
	void SetSelectedEntity(Entity entity) { m_SelectedEntity = entity; }

	entt::registry& GetRegistry() { return m_Registry; }
	const entt::registry& GetRegistry() const { return m_Registry; }
	size_t GetEntityCount() const { return m_Registry.view<TagComponent>().size(); }

	std::shared_ptr<Skybox> GetSkybox() const { return m_Skybox; }
	void SetSkybox(const std::shared_ptr<Skybox>& skybox) { m_Skybox = skybox; }

	// Sunlight query helper
	Entity GetSunLight();

	// Factory helpers for standard game objects
	Entity CreateModelEntity(const std::string& name, const std::string& modelPath, bool flipUVs = false);
	Entity CreateDirectionalLight(const std::string& name, const glm::vec3& direction, const glm::vec3& color = glm::vec3(1.0f), float intensity = 1.0f);
	Entity CreatePointLight(const std::string& name, const glm::vec3& position, const glm::vec3& color = glm::vec3(1.0f), float intensity = 1.0f, float range = 25.0f);
	Entity CreateSpotLight(const std::string& name, const glm::vec3& position, const glm::vec3& direction, const glm::vec3& color = glm::vec3(1.0f), float intensity = 1.0f, float innerCutoff = 12.5f, float outerCutoff = 17.5f, float range = 35.0f);

	// Backward-compatibility queries
	std::vector<Entity> GetAllEntitiesWithMesh();
	std::vector<Entity> GetAllEntitiesWithLight();

private:
	entt::registry m_Registry;
	Entity m_SelectedEntity;
	std::shared_ptr<Skybox> m_Skybox;

	friend class Entity;
};

// --- Entity Template Implementations ---

inline Entity Scene::GetPrimaryCameraEntity()
{
	auto view = m_Registry.view<TagComponent, CameraComponent>();
	for (auto [entityHandle, tag, camera] : view.each())
	{
		if (tag.Active && camera.Primary)
		{
			return Entity{ entityHandle, this };
		}
	}
return {};
}

#ifdef IGNIS_BUILD_MODULE
inline glm::vec3 CharacterControllerComponent::Move(Scene& scene, Entity entity, const glm::vec3& displacement) { return displacement; }

inline void CharacterControllerComponent::SimpleMove(Scene& scene, Entity entity, const glm::vec3& horizontalVelocity, float deltaTime)
{
	if (Ignis::GetContext() && Ignis::GetContext()->Physics.CharacterControllerSimpleMove)
		Ignis::GetContext()->Physics.CharacterControllerSimpleMove(this, &scene, (uint32_t)entity.GetHandle(), horizontalVelocity, deltaTime);
}

inline void CharacterControllerComponent::Jump(float jumpForce)
{
	if (Ignis::GetContext() && Ignis::GetContext()->Physics.CharacterControllerJump)
		Ignis::GetContext()->Physics.CharacterControllerJump(this, jumpForce);
}
#endif

template<typename T, typename... Args>
inline T& Entity::AddComponent(Args&&... args)
{
	assert(!HasComponent<T>() && "Entity already has component!");
	return m_Scene->m_Registry.emplace<T>(m_EntityHandle, std::forward<Args>(args)...);
}

template<typename T, typename... Args>
inline T& Entity::AddOrReplaceComponent(Args&&... args)
{
	return m_Scene->m_Registry.emplace_or_replace<T>(m_EntityHandle, std::forward<Args>(args)...);
}

template<typename T>
inline T& Entity::GetComponent()
{
	assert(HasComponent<T>() && "Entity does not have component!");
	return m_Scene->m_Registry.get<T>(m_EntityHandle);
}

template<typename T>
inline const T& Entity::GetComponent() const
{
	assert(HasComponent<T>() && "Entity does not have component!");
	return m_Scene->m_Registry.get<T>(m_EntityHandle);
}

template<typename T>
inline bool Entity::HasComponent() const
{
	return m_Scene != nullptr && m_EntityHandle != entt::null && m_Scene->m_Registry.all_of<T>(m_EntityHandle);
}

template<typename T>
inline void Entity::RemoveComponent()
{
	assert(HasComponent<T>() && "Entity does not have component!");
	m_Scene->m_Registry.remove<T>(m_EntityHandle);
}

inline const std::string& Entity::GetName() const
{
	static const std::string s_Empty = "";
	if (HasComponent<TagComponent>())
		return GetComponent<TagComponent>().Tag;
	return s_Empty;
}

inline void Entity::SetName(const std::string& name)
{
	if (HasComponent<TagComponent>())
		GetComponent<TagComponent>().Tag = name;
}

inline bool Entity::IsActive() const
{
	if (HasComponent<TagComponent>())
		return GetComponent<TagComponent>().Active;
	return true;
}

inline void Entity::SetActive(bool active)
{
	if (HasComponent<TagComponent>())
		GetComponent<TagComponent>().Active = active;
}

inline TransformComponent& Entity::GetTransform()
{
	return GetComponent<TransformComponent>();
}

inline const TransformComponent& Entity::GetTransform() const
{
	return GetComponent<TransformComponent>();
}

inline glm::mat4 Entity::GetModelMatrix() const
{
	if (HasComponent<TransformComponent>())
		return GetComponent<TransformComponent>().GetMatrix();
	return glm::mat4(1.0f);
}

inline glm::vec3 Entity::GetPosition() const
{
	if (HasComponent<TransformComponent>())
		return GetComponent<TransformComponent>().Position;
	return glm::vec3(0.0f);
}

inline void Entity::SetPosition(const glm::vec3& position)
{
	if (HasComponent<TransformComponent>())
		GetComponent<TransformComponent>().Position = position;
}

inline glm::vec3 Entity::GetRotation() const
{
	if (HasComponent<TransformComponent>())
		return GetComponent<TransformComponent>().Rotation;
	return glm::vec3(0.0f);
}

inline void Entity::SetRotation(const glm::vec3& rotation)
{
	if (HasComponent<TransformComponent>())
		GetComponent<TransformComponent>().Rotation = rotation;
}

inline glm::vec3 Entity::GetScale() const
{
	if (HasComponent<TransformComponent>())
		return GetComponent<TransformComponent>().Scale;
	return glm::vec3(1.0f);
}

inline void Entity::SetScale(const glm::vec3& scale)
{
	if (HasComponent<TransformComponent>())
		GetComponent<TransformComponent>().Scale = scale;
}
