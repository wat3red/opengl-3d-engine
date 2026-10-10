#include "scene.h"
#include "light.h"

Scene::Scene()
	: m_SelectedEntity{}, m_Skybox(nullptr)
{
}

Scene::~Scene()
{
	Clear();
}

Entity Scene::CreateEntity(const std::string& name)
{
	entt::entity handle = m_Registry.create();
	Entity entity = { handle, this };
	entity.AddComponent<TagComponent>(name.empty() ? "Entity" : name);
	entity.AddComponent<TransformComponent>();
	return entity;
}

void Scene::DestroyEntity(Entity entity)
{
	if (entity == m_SelectedEntity)
	{
		m_SelectedEntity = {};
	}
	m_Registry.destroy(entity.GetHandle());
}

void Scene::Update(float deltaTime)
{
	auto view = m_Registry.view<TagComponent, TransformComponent, RigidbodyComponent>();
	for (auto [entityHandle, tag, transform, rb] : view.each())
	{
		if (tag.Active && rb.Body && deltaTime > 0.0f)
		{
			rb.Body->Update(deltaTime);
			if (glm::length(rb.Body->GetVelocity()) > 0.0001f)
			{
				transform.Position = rb.Body->GetPosition();
			}
		}
	}
}

void Scene::Clear()
{
	m_Registry.clear();
	m_SelectedEntity = {};
	m_Skybox.reset();
}

Entity Scene::GetSunLight()
{
	auto view = m_Registry.view<TagComponent, LightComponent>();
	for (auto [entityHandle, tag, light] : view.each())
	{
		if (tag.Active && light.Type == LightType::Directional)
		{
			return { entityHandle, this };
		}
	}
	return {};
}

Entity Scene::CreateModelEntity(const std::string& name, const std::string& modelPath, bool flipUVs)
{
	Entity entity = CreateEntity(name);
	auto& mesh = entity.AddComponent<MeshComponent>(modelPath, flipUVs);
	if (mesh.ModelAsset)
	{
		entity.AddComponent<RigidbodyComponent>(mesh.ModelAsset.get());
	}
	if (!m_SelectedEntity)
	{
		m_SelectedEntity = entity;
	}
	return entity;
}

Entity Scene::CreateDirectionalLight(const std::string& name, const glm::vec3& direction, const glm::vec3& color, float intensity)
{
	Entity entity = CreateEntity(name);
	entity.SetRotation(EulerFromDirection(direction));
	entity.AddComponent<LightComponent>(LightComponent::CreateDirectional(direction, color, intensity));
	if (!m_SelectedEntity)
	{
		m_SelectedEntity = entity;
	}
	return entity;
}

Entity Scene::CreatePointLight(const std::string& name, const glm::vec3& position, const glm::vec3& color, float intensity, float range)
{
	Entity entity = CreateEntity(name);
	entity.SetPosition(position);
	entity.AddComponent<LightComponent>(LightComponent::CreatePoint(color, intensity, range));
	if (!m_SelectedEntity)
	{
		m_SelectedEntity = entity;
	}
	return entity;
}

Entity Scene::CreateSpotLight(const std::string& name, const glm::vec3& position, const glm::vec3& direction, const glm::vec3& color, float intensity, float innerCutoff, float outerCutoff, float range)
{
	Entity entity = CreateEntity(name);
	entity.SetPosition(position);
	entity.SetRotation(EulerFromDirection(direction));
	entity.AddComponent<LightComponent>(LightComponent::CreateSpot(direction, color, intensity, innerCutoff, outerCutoff, range));
	if (!m_SelectedEntity)
	{
		m_SelectedEntity = entity;
	}
	return entity;
}

std::vector<Entity> Scene::GetAllEntitiesWithMesh()
{
	std::vector<Entity> result;
	auto view = m_Registry.view<MeshComponent>();
	result.reserve(view.size());
	for (auto entityHandle : view)
	{
		result.emplace_back(entityHandle, this);
	}
	return result;
}

std::vector<Entity> Scene::GetAllEntitiesWithLight()
{
	std::vector<Entity> result;
	auto view = m_Registry.view<LightComponent>();
	result.reserve(view.size());
	for (auto entityHandle : view)
	{
		result.emplace_back(entityHandle, this);
	}
	return result;
}
