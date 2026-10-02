// model_entity.h
#pragma once

#include "entity.h"
#include "model.h"
#include "asset_manager.h"
#include "physics/rigidbody.h"
#include <memory>
#include <string>

class ModelEntity : public Entity {
public:
	ModelEntity(const std::string& name, const std::string& modelPath, bool flipUVs = false)
		: Entity(name), m_ModelPath(modelPath), m_Model(AssetManager::Get().LoadModel(modelPath, flipUVs)), m_Rigidbody(nullptr)
	{
		if (m_Model)
		{
			m_Rigidbody = new Rigidbody(m_Model.get());
		}
	}

	explicit ModelEntity(const std::string& modelPath)
		: ModelEntity("Model Entity", modelPath, false) {}

	~ModelEntity() override
	{
		delete m_Rigidbody;
	}

	void Update(float deltaTime) override
	{
		if (m_Rigidbody && deltaTime > 0.0f)
		{
			m_Rigidbody->Update(deltaTime);
			// Update position from physics if velocity is active
			if (glm::length(m_Rigidbody->GetVelocity()) > 0.0001f)
			{
				m_Transform.Position = m_Rigidbody->GetPosition();
			}
		}
	}

	void Draw(Shader& shader) override
	{
		if (!m_Active || !m_Model) return;

		shader.SetUniformMat4f("model", m_Transform.GetMatrix());
		m_Model->Draw(shader);
	}

	std::shared_ptr<Model> GetModel() const { return m_Model; }
	const std::string& GetModelPath() const { return m_ModelPath; }
	Rigidbody* GetRigidbody() const { return m_Rigidbody; }

	glm::vec3 GetBoundingBoxMin() const
	{
		if (!m_Model) return glm::vec3(0.0f);
		return glm::vec3(m_Transform.GetMatrix() * glm::vec4(m_Model->GetAABB().min, 1.0f));
	}

	glm::vec3 GetBoundingBoxMax() const
	{
		if (!m_Model) return glm::vec3(0.0f);
		return glm::vec3(m_Transform.GetMatrix() * glm::vec4(m_Model->GetAABB().max, 1.0f));
	}

private:
	std::string m_ModelPath;
	std::shared_ptr<Model> m_Model;
	Rigidbody* m_Rigidbody;
};
