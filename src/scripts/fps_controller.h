#pragma once

#include "scene/scriptable_entity.h"
#include "scene/components.h"
#include "scene/scene.h"
#include "core/input.h"
#include "physics/collision.h"
#include <iostream>
#include <algorithm>

/**
 * FPSController: Production-ready first-person gameplay script.
 * Demonstrates:
 * - Mouse look with pitch clamping (-89 to +89 degrees)
 * - Walk (WASD) and sprint (Left Shift)
 * - Jumping and grounded physics via CharacterControllerComponent
 * - Cursor locking / unlocking (Escape to toggle)
 * - Hitscan weapon firing with 3D raycasting (Left Click)
 * - Positional sound trigger via AudioSourceComponent
 */
class FPSController : public ScriptableEntity {
public:
	float WalkSpeed = 5.0f;
	float SprintSpeed = 9.0f;
	float MouseSensitivity = 0.12f;
	float JumpForce = 6.5f;
	float WeaponRange = 100.0f;

	float Yaw = -90.0f;
	float Pitch = 0.0f;
	bool CursorLocked = true;

	void OnCreate() override
	{
		Input::SetCursorMode(CursorMode::Locked);
		CursorLocked = true;

		// If entity does not have a CharacterController, ensure one is attached
		if (!HasComponent<CharacterControllerComponent>())
		{
			GetEntity().AddComponent<CharacterControllerComponent>();
		}

		std::cout << "[FPSController] Initialized for entity: " << GetEntity().GetName() << std::endl;
	}

	void OnUpdate(float deltaTime) override
	{
		if (deltaTime <= 0.0f)
			return;

		// 1. Cursor toggle on Escape
		if (Input::IsKeyPressed(KeyCode::Escape))
		{
			CursorLocked = false;
			Input::SetCursorMode(CursorMode::Normal);
		}
		if (!CursorLocked && Input::IsMouseButtonPressed(MouseCode::ButtonLeft))
		{
			CursorLocked = true;
			Input::SetCursorMode(CursorMode::Locked);
		}

		// 2. Mouse Look (when locked)
		if (CursorLocked)
		{
			glm::vec2 mouseDelta = Input::GetMouseDelta();
			Yaw += mouseDelta.x * MouseSensitivity;
			Pitch -= mouseDelta.y * MouseSensitivity;
			Pitch = std::clamp(Pitch, -89.0f, 89.0f);

			// Keep yaw in [-360, 360]
			if (Yaw > 360.0f) Yaw -= 360.0f;
			if (Yaw < -360.0f) Yaw += 360.0f;

			auto& tc = GetComponent<TransformComponent>();
			tc.Rotation.x = Pitch;
			tc.Rotation.y = Yaw;

			// If this entity also has a camera, sync its rotation
			if (HasComponent<CameraComponent>())
			{
				auto& cc = GetComponent<CameraComponent>();
				cc.CameraInstance.SetYawAndPitch(Yaw, Pitch);
			}
		}

		// 3. Movement input (WASD)
		float yawRad = glm::radians(Yaw);
		glm::vec3 forward(std::cos(yawRad), 0.0f, std::sin(yawRad));
		forward = glm::normalize(forward);
		glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));

		glm::vec3 moveDir(0.0f);
		if (Input::IsKeyPressed(KeyCode::W)) moveDir += forward;
		if (Input::IsKeyPressed(KeyCode::S)) moveDir -= forward;
		if (Input::IsKeyPressed(KeyCode::D)) moveDir += right;
		if (Input::IsKeyPressed(KeyCode::A)) moveDir -= right;

		float currentSpeed = Input::IsKeyPressed(KeyCode::LeftShift) ? SprintSpeed : WalkSpeed;
		glm::vec3 horizontalVel(0.0f);

		if (glm::length(moveDir) > 0.001f)
		{
			horizontalVel = glm::normalize(moveDir) * currentSpeed;
		}

		// 4. Character controller movement & jump
		if (HasComponent<CharacterControllerComponent>())
		{
			auto& cc = GetComponent<CharacterControllerComponent>();

			if (CursorLocked && Input::IsKeyPressed(KeyCode::Space) && cc.IsGrounded)
			{
				cc.Jump(JumpForce);

				// Play jump sound if audio component exists
				if (HasComponent<AudioSourceComponent>())
				{
					GetComponent<AudioSourceComponent>().Play();
				}
			}

			cc.SimpleMove(*GetScene(), GetEntity(), horizontalVel, deltaTime);
		}

		// 5. Weapon shooting (Left Mouse Click)
		if (CursorLocked && Input::IsMouseButtonPressed(MouseCode::ButtonLeft))
		{
			ShootWeapon();
		}
	}

	void OnDestroy() override
	{
		Input::SetCursorMode(CursorMode::Normal);
	}

private:
	float m_ShootCooldown = 0.0f;

	void ShootWeapon()
	{
		// Fire rate limiter (max 5 shots/sec)
		if (m_ShootCooldown > 0.0f)
			return;

		m_ShootCooldown = 0.2f;

		// Calculate forward ray direction from camera pitch and yaw
		float pitchRad = glm::radians(Pitch);
		float yawRad = glm::radians(Yaw);

		glm::vec3 rayDir;
		rayDir.x = std::cos(pitchRad) * std::cos(yawRad);
		rayDir.y = std::sin(pitchRad);
		rayDir.z = std::cos(pitchRad) * std::sin(yawRad);
		rayDir = glm::normalize(rayDir);

		auto& tc = GetComponent<TransformComponent>();
		// Eye position: slightly below top of height
		glm::vec3 eyePos = tc.Position + glm::vec3(0.0f, 1.6f, 0.0f);

		Physics::Ray ray(eyePos, rayDir);
		Physics::RaycastHit hit;

		uint32_t shootMask = Physics::Layer_All & ~Physics::Layer_Player; // don't shoot self
		if (Physics::Raycast(*GetScene(), ray, WeaponRange, hit, shootMask, false))
		{
			std::cout << "[FPS Weapon Hit!] Entity: '" << hit.HitEntity.GetName()
			          << "' at distance: " << hit.Distance
			          << "m | Point: (" << hit.Point.x << ", " << hit.Point.y << ", " << hit.Point.z << ")" << std::endl;
		}
	}
};
