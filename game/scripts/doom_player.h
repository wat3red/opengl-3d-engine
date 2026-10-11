#pragma once

#include "scene/scriptable_entity.h"
#include "scene/components.h"
#include "scene/scene.h"
#include "core/input.h"
#include "physics/collision.h"
#include "audio/audio_engine.h"
#include "game/scripts/doom_game_state.h"
#include "game/scripts/doom_enemy.h"
#include "game/scripts/doom_hud.h"
#include <iostream>
#include <random>
#include <cmath>
#include <algorithm>

class DoomPlayer : public ScriptableEntity {
public:
	float WalkSpeed = 7.0f;
	float RunSpeed = 11.0f;
	float MouseSensitivity = 0.12f;
	float JumpForce = 7.0f;

	float Yaw = -90.0f;
	float Pitch = 0.0f;
	bool CursorLocked = true;

	void OnCreate() override
	{
		DoomGameState::Reset();

		Input::SetCursorMode(CursorMode::Locked);
		CursorLocked = true;

		if (!HasComponent<CharacterControllerComponent>())
		{
			auto& cc = GetEntity().AddComponent<CharacterControllerComponent>();
			cc.Radius = 0.45f;
			cc.Height = 1.8f;
		}

		m_ShotgunSfx = AudioEngine::Get().CreateSound("res/audio/shotgun_fire.wav", false, false);
		m_JumpSfx = AudioEngine::Get().CreateSound("res/audio/player_jump.wav", false, false);
	}

	void OnUpdate(float deltaTime) override
	{
		if (deltaTime <= 0.0f)
			return;

		// Timers
		if (m_ShootCooldown > 0.0f)
			m_ShootCooldown -= deltaTime;
		if (DoomGameState::HitmarkerTimer > 0.0f)
			DoomGameState::HitmarkerTimer -= deltaTime;
		if (DoomGameState::DamageFlashTimer > 0.0f)
			DoomGameState::DamageFlashTimer -= deltaTime;
		if (DoomGameState::MuzzleFlashTimer > 0.0f)
			DoomGameState::MuzzleFlashTimer -= deltaTime;

		// Restart on R if GameOver / Victory
		if ((DoomGameState::GameOver || DoomGameState::Victory) && Input::IsKeyPressed(KeyCode::R))
		{
			DoomGameState::Reset();
			GetComponent<TransformComponent>().Position = glm::vec3(0.0f, 12.0f, 58.0f);
		}

		// Cursor toggle on Escape / Left click
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

		// Mouse Look
		if (CursorLocked && !DoomGameState::GameOver)
		{
			glm::vec2 mouseDelta = Input::GetMouseDelta();
			Yaw += mouseDelta.x * MouseSensitivity;
			Pitch -= mouseDelta.y * MouseSensitivity;
			Pitch = std::clamp(Pitch, -89.0f, 89.0f);

			if (Yaw > 360.0f) Yaw -= 360.0f;
			if (Yaw < -360.0f) Yaw += 360.0f;

			auto& tc = GetComponent<TransformComponent>();
			tc.Rotation.x = Pitch;
			tc.Rotation.y = Yaw;

			if (HasComponent<CameraComponent>())
			{
				auto& cc = GetComponent<CameraComponent>();
				cc.CameraInstance.SetYawAndPitch(Yaw, Pitch);
			}
		}

		// Movement (WASD)
		if (!DoomGameState::GameOver)
		{
			float yawRad = glm::radians(Yaw);
			glm::vec3 forward(std::cos(yawRad), 0.0f, std::sin(yawRad));
			forward = glm::normalize(forward);
			glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));

			glm::vec3 moveDir(0.0f);
			if (Input::IsKeyPressed(KeyCode::W)) moveDir += forward;
			if (Input::IsKeyPressed(KeyCode::S)) moveDir -= forward;
			if (Input::IsKeyPressed(KeyCode::D)) moveDir += right;
			if (Input::IsKeyPressed(KeyCode::A)) moveDir -= right;

			float currentSpeed = Input::IsKeyPressed(KeyCode::LeftShift) ? RunSpeed : WalkSpeed;
			glm::vec3 horizontalVel(0.0f);

			if (glm::length(moveDir) > 0.001f)
			{
				horizontalVel = glm::normalize(moveDir) * currentSpeed;
			}

			if (HasComponent<CharacterControllerComponent>())
			{
				auto& cc = GetComponent<CharacterControllerComponent>();

				if (CursorLocked && Input::IsKeyPressed(KeyCode::Space) && cc.IsGrounded)
				{
					cc.Jump(JumpForce);
					if (m_JumpSfx)
					{
						m_JumpSfx->Play();
					}
				}

				cc.SimpleMove(*GetScene(), GetEntity(), horizontalVel, deltaTime);
			}

			// Update global player state for AI and weapon viewmodel
			auto& playerTc = GetComponent<TransformComponent>();
			DoomGameState::PlayerPosition = playerTc.Position;
			float pitchRad = glm::radians(Pitch);
			DoomGameState::PlayerForward = glm::normalize(glm::vec3(
				std::cos(pitchRad) * std::cos(yawRad),
				std::sin(pitchRad),
				std::cos(pitchRad) * std::sin(yawRad)
			));

			// Shotgun Fire
			if (CursorLocked && Input::IsMouseButtonPressed(MouseCode::ButtonLeft) && m_ShootCooldown <= 0.0f)
			{
				FireShotgun();
			}
		}
	}

	void OnGUI() override
	{
		DrawRetroDoomHUD();
	}

	void OnDestroy() override
	{
		Input::SetCursorMode(CursorMode::Normal);
	}

private:
	float m_ShootCooldown = 0.0f;
	std::shared_ptr<Sound> m_ShotgunSfx;
	std::shared_ptr<Sound> m_JumpSfx;

	void FireShotgun()
	{
		if (DoomGameState::ShellsAmmo < 2)
		{
			return; // Out of shells
		}

		DoomGameState::ShellsAmmo -= 2;
		m_ShootCooldown = 0.78f;
		DoomGameState::MuzzleFlashTimer = 0.08f;

		if (m_ShotgunSfx)
		{
			m_ShotgunSfx->Play();
		}

		// Recoil kick
		Pitch = std::min(89.0f, Pitch + 2.5f);

		// Calculate center ray direction
		float pitchRad = glm::radians(Pitch);
		float yawRad = glm::radians(Yaw);

		glm::vec3 baseDir;
		baseDir.x = std::cos(pitchRad) * std::cos(yawRad);
		baseDir.y = std::sin(pitchRad);
		baseDir.z = std::cos(pitchRad) * std::sin(yawRad);
		baseDir = glm::normalize(baseDir);

		glm::vec3 right = glm::normalize(glm::cross(baseDir, glm::vec3(0.0f, 1.0f, 0.0f)));
		glm::vec3 up = glm::normalize(glm::cross(right, baseDir));

		glm::vec3 eyePos = GetComponent<TransformComponent>().Position + glm::vec3(0.0f, 1.6f, 0.0f);

		// Fire 8 pellets with spread
		const int pelletCount = 8;
		const float spreadAngle = 0.045f;
		uint32_t shootMask = Physics::Layer_All & ~Physics::Layer_Player;

		for (int i = 0; i < pelletCount; ++i)
		{
			float randX = ((float)rand() / RAND_MAX - 0.5f) * spreadAngle * 2.0f;
			float randY = ((float)rand() / RAND_MAX - 0.5f) * spreadAngle * 2.0f;

			glm::vec3 pelletDir = glm::normalize(baseDir + right * randX + up * randY);
			Physics::Ray ray(eyePos, pelletDir);
			Physics::RaycastHit hit;

			if (Physics::Raycast(*GetScene(), ray, 60.0f, hit, shootMask, false))
			{
				if (hit.HitEntity && hit.HitEntity.HasComponent<NativeScriptComponent>())
				{
					auto& nsc = hit.HitEntity.GetComponent<NativeScriptComponent>();
					if (nsc.Instance && nsc.ScriptName == "DoomEnemy")
					{
						auto* enemy = static_cast<DoomEnemy*>(nsc.Instance);
						int pelletDamage = 14 + rand() % 8; // 14-21 damage per pellet
						enemy->TakeDamage(pelletDamage, pelletDir);
						DoomGameState::HitmarkerTimer = 0.15f;
					}
				}
			}
		}
	}
};
