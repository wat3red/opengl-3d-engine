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

	void GetWorldAABB(glm::vec3& outMin, glm::vec3& outMax) const
	{
		if (!m_Model) {
			outMin = m_Transform.Position;
			outMax = m_Transform.Position;
			return;
		}

		const auto& aabb = m_Model->GetAABB();
		glm::mat4 M = m_Transform.GetMatrix();

		glm::vec3 corners[8] = {
			glm::vec3(aabb.min.x, aabb.min.y, aabb.min.z),
			glm::vec3(aabb.max.x, aabb.min.y, aabb.min.z),
			glm::vec3(aabb.min.x, aabb.max.y, aabb.min.z),
			glm::vec3(aabb.max.x, aabb.max.y, aabb.min.z),
			glm::vec3(aabb.min.x, aabb.min.y, aabb.max.z),
			glm::vec3(aabb.max.x, aabb.min.y, aabb.max.z),
			glm::vec3(aabb.min.x, aabb.max.y, aabb.max.z),
			glm::vec3(aabb.max.x, aabb.max.y, aabb.max.z)
		};

		outMin = glm::vec3(std::numeric_limits<float>::max());
		outMax = glm::vec3(std::numeric_limits<float>::lowest());

		for (int i = 0; i < 8; ++i) {
			glm::vec3 transformed = glm::vec3(M * glm::vec4(corners[i], 1.0f));
			outMin = glm::min(outMin, transformed);
			outMax = glm::max(outMax, transformed);
		}
	}

	glm::vec3 GetBoundingBoxMin() const
	{
		glm::vec3 bmin, bmax;
		GetWorldAABB(bmin, bmax);
		return bmin;
	}

	glm::vec3 GetBoundingBoxMax() const
	{
		glm::vec3 bmin, bmax;
		GetWorldAABB(bmin, bmax);
		return bmax;
	}

private:
	std::string m_ModelPath;
	std::shared_ptr<Model> m_Model;
	Rigidbody* m_Rigidbody;
};
