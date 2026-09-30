// scene.h
#pragma once

#include "model_entity.h"
#include "light.h"
#include <vector>
#include <memory>
#include <string>

class Scene {
public:
	Scene();
	~Scene();

	void AddEntity(const std::shared_ptr<ModelEntity>& entity);
	void AddLight(const std::shared_ptr<Light>& light);

	void Update(float deltaTime);

	const std::vector<std::shared_ptr<ModelEntity>>& GetEntities() const { return m_Entities; }
	std::vector<std::shared_ptr<ModelEntity>>& GetEntities() { return m_Entities; }

	const std::vector<std::shared_ptr<Light>>& GetLights() const { return m_Lights; }
	std::vector<std::shared_ptr<Light>>& GetLights() { return m_Lights; }

	std::shared_ptr<DirectionalLight> GetSunLight() const { return m_SunLight; }
	void SetSunLight(const std::shared_ptr<DirectionalLight>& sun) { m_SunLight = sun; }

	std::shared_ptr<Entity> GetSelectedEntity() const { return m_SelectedEntity; }
	void SetSelectedEntity(const std::shared_ptr<Entity>& entity) { m_SelectedEntity = entity; }

	void Clear();

private:
	std::vector<std::shared_ptr<ModelEntity>> m_Entities;
	std::vector<std::shared_ptr<Light>> m_Lights;
	std::shared_ptr<DirectionalLight> m_SunLight;
	std::shared_ptr<Entity> m_SelectedEntity;
};
