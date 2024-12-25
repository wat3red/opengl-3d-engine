// model_entity.h
#pragma once

#include "entity.h"
#include "model.h"
#include "physics/rigidbody.h"
#include <iostream>

class ModelEntity : public Entity {
public:
	ModelEntity(const std::string& modelPath, bool gamma = false, bool flipUVs = false)
		: model(new Model(modelPath, gamma, flipUVs)), rigidbody(new Rigidbody(model)) {
		//std::cout << "ModelEntity created for model: " << modelPath << std::endl;
	}

	void Update(float deltaTime) {
		//std::cout << "deltaTime: " << deltaTime << std::endl;

		if (rigidbody) {
			//std::cout << "rigidbody: " << rigidbody << std::endl;

			rigidbody->Update(deltaTime);
			//std::cout << "rigidbody->position x: " << rigidbody->position.x << std::endl;
			//std::cout << "rigidbody->position y: " << rigidbody->position.y << std::endl;
			//std::cout << "rigidbody->position z: " << rigidbody->position.z << std::endl;

			
			SetPosition(rigidbody->GetPosition()); // Update entity position for rendering
		}
	}

	Rigidbody* GetRigidbody() const { return rigidbody; };

	void Draw(Shader& shader) override {
		// Set the model matrix uniform before drawing
		shader.SetUniformMat4f("model", modelMatrix);
		model->Draw(shader);
	}

	glm::vec3 GetBoundingBoxMin() const {
		return glm::vec3(modelMatrix * glm::vec4(model->GetAABB().min, 1.0f));
	}

	glm::vec3 GetBoundingBoxMax() const {
		return glm::vec3(modelMatrix * glm::vec4(model->GetAABB().max, 1.0f));
	}



private:
	Model* model;
	Rigidbody* rigidbody;
};
