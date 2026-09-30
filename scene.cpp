// scene.cpp
#include "scene.h"

Scene::Scene()
	: m_SunLight(nullptr), m_SelectedEntity(nullptr)
{
}

Scene::~Scene()
{
	Clear();
}

void Scene::AddEntity(const std::shared_ptr<ModelEntity>& entity)
{
	if (entity)
	{
		m_Entities.push_back(entity);
		if (!m_SelectedEntity)
		{
			m_SelectedEntity = entity;
		}
	}
}

void Scene::AddLight(const std::shared_ptr<Light>& light)
{
	if (light)
	{
		m_Lights.push_back(light);
		if (light->GetType() == LightType::Directional && !m_SunLight)
		{
			m_SunLight = std::dynamic_pointer_cast<DirectionalLight>(light);
		}
	}
}

void Scene::Update(float deltaTime)
{
	for (auto& entity : m_Entities)
	{
		if (entity && entity->IsActive())
		{
			entity->Update(deltaTime);
		}
	}
}

void Scene::Clear()
{
	m_Entities.clear();
	m_Lights.clear();
	m_SunLight.reset();
	m_SelectedEntity.reset();
}
