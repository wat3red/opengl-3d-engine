#pragma once

#include "scene/scriptable_entity.h"
#include "scene/components.h"
#include "scene/scene.h"
#include "game/scripts/doom_game_state.h"
#include <glm/glm.hpp>
#include <cmath>

class ShotgunViewModel : public ScriptableEntity {
public:
	glm::vec3 Offset{ 0.15f, -0.18f, 0.42f }; // X: right, Y: down, Z: forward
	float BobFrequency = 8.0f;
	float BobAmount = 0.012f;

	void OnCreate() override
	{
		auto& tc = GetComponent<TransformComponent>();
		tc.Scale = glm::vec3(0.016f);
	}

	void OnUpdate(float deltaTime) override
	{
		if (deltaTime <= 0.0f)
			return;

		Entity primaryCam = GetScene()->GetPrimaryCameraEntity();
		if (!primaryCam || !primaryCam.HasComponent<CameraComponent>())
			return;

		auto& camTc = primaryCam.GetTransform();
		auto& camComp = primaryCam.GetComponent<CameraComponent>();

		glm::vec3 camPos = camComp.CameraInstance.Position;
		glm::vec3 camFront = camComp.CameraInstance.Front;
		glm::vec3 camRight = glm::normalize(glm::cross(camFront, glm::vec3(0.0f, 1.0f, 0.0f)));
		glm::vec3 camUp = glm::normalize(glm::cross(camRight, camFront));

		// Walk sway / bobbing
		m_BobTimer += deltaTime * BobFrequency;
		float bobY = std::sin(m_BobTimer) * BobAmount;
		float bobX = std::cos(m_BobTimer * 0.5f) * BobAmount * 0.6f;

		// Recoil kickback on fire
		float kickback = 0.0f;
		if (DoomGameState::MuzzleFlashTimer > 0.0f)
		{
			kickback = 0.08f * (DoomGameState::MuzzleFlashTimer / 0.08f);
		}

		auto& tc = GetComponent<TransformComponent>();
		glm::vec3 targetPos = camPos
			+ camRight * (Offset.x + bobX)
			+ camUp * (Offset.y + bobY)
			+ camFront * (Offset.z - kickback);

		tc.Position = targetPos;

		// Model forward vector is +X. Align +X with camFront, +Y with camUp, +Z with camRight
		float sy = -camFront.z;
		float ry = std::asin(std::clamp(sy, -1.0f, 1.0f));
		float cy = std::cos(ry);
		float rx = 0.0f, rz = 0.0f;
		if (std::abs(cy) > 1e-4f)
		{
			rx = std::atan2(camUp.z, camRight.z);
			rz = std::atan2(camFront.y, camFront.x);
		}
		else
		{
			rz = std::atan2(-camUp.x, camUp.y);
		}
		tc.Rotation = glm::vec3(glm::degrees(rx), glm::degrees(ry), glm::degrees(rz));
	}

private:
	float m_BobTimer = 0.0f;
};
