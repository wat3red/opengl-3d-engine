#pragma once

#include <ignis/ignis.h>
#include "doom_game_state.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <string>

class DoomEnemy : public ScriptableEntity {
public:
	int Health = 80;
	float MoveSpeed = 4.5f;
	float DetectionRadius = 40.0f;
	float AttackRadius = 2.4f;
	int AttackDamage = 16;
	float AttackCooldown = 1.0f;

	bool IsDead = false;

	void OnCreate() override
	{
		++DoomGameState::DemonsRemaining;

		std::string tag = "Demon";
		if (HasComponent<TagComponent>())
		{
			tag = GetComponent<TagComponent>().Tag;
		}

		// Configure stats based on demon type
		std::string lowerTag = tag;
		for (auto& c : lowerTag) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

		if (lowerTag.find("caco") != std::string::npos)
		{
			Health = 140;
			MoveSpeed = 4.0f;
			AttackDamage = 24;
			AttackRadius = 2.8f;
			DetectionRadius = 50.0f;
			m_IsFlying = true;
		}
		else if (lowerTag.find("imp") != std::string::npos)
		{
			Health = 70;
			MoveSpeed = 5.2f;
			AttackDamage = 14;
			AttackRadius = 2.2f;
			DetectionRadius = 38.0f;
		}

		glm::vec3 boxSize(2.2f, 2.2f, 2.2f);
		glm::vec3 boxCenter(0.0f, 1.1f, 0.0f);
		if (m_IsFlying)
		{
			boxSize = glm::vec3(2.6f, 2.6f, 2.6f);
			boxCenter = glm::vec3(0.0f, 0.0f, 0.0f);
		}

		bool calibratedFromModel = false;
		if (HasComponent<MeshComponent>())
		{
			auto& mc = GetComponent<MeshComponent>();
			if (mc.ModelAsset)
			{
				const auto& aabb = mc.ModelAsset->GetAABB();
				glm::vec3 modelSize = aabb.max - aabb.min;
				glm::vec3 modelCenter = (aabb.min + aabb.max) * 0.5f;
				if (glm::length(modelSize) > 0.1f)
				{
					boxSize = modelSize * 1.15f; // Generous 15% margin matching visual demon
					boxCenter = modelCenter;
					calibratedFromModel = true;
				}
			}
		}

		if (!calibratedFromModel && HasComponent<TransformComponent>())
		{
			auto& tc = GetComponent<TransformComponent>();
			glm::vec3 s = glm::max(glm::abs(tc.Scale), glm::vec3(0.0001f));
			boxSize = boxSize / s;
			boxCenter = boxCenter / s;
		}

		if (!HasComponent<BoxColliderComponent>())
		{
			GetEntity().AddComponent<BoxColliderComponent>(boxSize, boxCenter, false, Physics::Layer_Enemy);
		}
		else
		{
			auto& bc = GetComponent<BoxColliderComponent>();
			bc.Size = boxSize;
			bc.Center = boxCenter;
			bc.Layer = Physics::Layer_Enemy;
		}

		if (!HasComponent<MaterialComponent>())
		{
			GetEntity().AddComponent<MaterialComponent>();
		}
		auto& mat = GetComponent<MaterialComponent>();
		mat.Properties.EmissionColor = glm::vec3(0.4f, 0.1f, 0.05f);
		mat.Properties.EmissionIntensity = 1.5f;

		m_AudioHurt = AudioEngine::Get().CreateSound("res/audio/demon_hurt.wav", true, false);
		m_AudioDeath = AudioEngine::Get().CreateSound("res/audio/demon_death.wav", true, false);
		m_AudioAttack = AudioEngine::Get().CreateSound("res/audio/demon_growl.wav", true, false);
	}

	void OnUpdate(float deltaTime) override
	{
		if (deltaTime <= 0.0f)
			return;

		if (m_AttackTimer > 0.0f)
			m_AttackTimer -= deltaTime;

		if (m_HurtFlashTimer > 0.0f)
		{
			m_HurtFlashTimer -= deltaTime;
			if (m_HurtFlashTimer <= 0.0f && HasComponent<MaterialComponent>())
			{
				GetComponent<MaterialComponent>().Properties.EmissionColor = glm::vec3(0.8f, 0.1f, 0.05f);
				GetComponent<MaterialComponent>().Properties.EmissionIntensity = 1.5f;
			}
		}

		auto& tc = GetComponent<TransformComponent>();

		if (IsDead)
		{
			if (tc.Position.y > -2.0f)
			{
				tc.Position.y -= 1.5f * deltaTime;
				tc.Scale = glm::max(glm::vec3(0.001f), tc.Scale - glm::vec3(0.005f * deltaTime));
			}
			return;
		}

		glm::vec3 playerPos = DoomGameState::PlayerPosition;
		glm::vec3 toPlayer = playerPos - tc.Position;
		float distToPlayer = glm::length(toPlayer);

		if (distToPlayer <= DetectionRadius && distToPlayer > 0.01f)
		{
			float angle = glm::degrees(std::atan2(-toPlayer.z, toPlayer.x));
			tc.Rotation.y = angle;

			if (distToPlayer <= AttackRadius)
			{
				if (m_AttackTimer <= 0.0f && !DoomGameState::GameOver)
				{
					AttackPlayer();
					m_AttackTimer = AttackCooldown;
				}
			}
			else
			{
				if (m_IsFlying)
				{
					m_FlyBobTimer += deltaTime;
					glm::vec3 flyDir = glm::normalize(toPlayer);
					tc.Position += flyDir * (MoveSpeed * deltaTime);
					tc.Position.y += std::sin(m_FlyBobTimer * 3.5f) * 0.015f;
					if (tc.Position.y < 2.5f)
					{
						tc.Position.y = 2.5f;
					}
				}
				else
				{
					glm::vec3 dir = glm::normalize(glm::vec3(toPlayer.x, 0.0f, toPlayer.z));
					glm::vec3 horizMove = dir * MoveSpeed;

					if (HasComponent<CharacterControllerComponent>())
					{
						GetComponent<CharacterControllerComponent>().SimpleMove(*GetScene(), GetEntity(), horizMove, deltaTime);
					}
					else
					{
						tc.Position += horizMove * deltaTime;
					}
				}
			}
		}
	}

	void TakeDamage(int damage, const glm::vec3& knockbackDir)
	{
		if (IsDead)
			return;

		Health -= damage;
		m_HurtFlashTimer = 0.15f;

		if (HasComponent<MaterialComponent>())
		{
			GetComponent<MaterialComponent>().Properties.EmissionColor = glm::vec3(1.0f, 1.0f, 1.0f);
			GetComponent<MaterialComponent>().Properties.EmissionIntensity = 5.0f;
		}

		auto& tc = GetComponent<TransformComponent>();
		tc.Position += knockbackDir * 0.35f;

		if (Health <= 0)
		{
			Die();
		}
		else
		{
			if (m_AudioHurt)
			{
				m_AudioHurt->SetPosition(tc.Position);
				m_AudioHurt->Play();
			}
		}
	}

	void Die()
	{
		if (IsDead)
			return;

		IsDead = true;
		DoomGameState::DemonsRemaining = std::max(0, DoomGameState::DemonsRemaining - 1);
		DoomGameState::Score += 100;

		auto& tc = GetComponent<TransformComponent>();
		if (m_AudioDeath)
		{
			m_AudioDeath->SetPosition(tc.Position);
			m_AudioDeath->Play();
		}

		if (HasComponent<BoxColliderComponent>())
			GetComponent<BoxColliderComponent>().Layer = 0;
		if (HasComponent<SphereColliderComponent>())
			GetComponent<SphereColliderComponent>().Layer = 0;

		if (DoomGameState::DemonsRemaining <= 0)
		{
			DoomGameState::Victory = true;
		}
	}

	void OnDestroy() override
	{
		if (!IsDead)
		{
			DoomGameState::DemonsRemaining = std::max(0, DoomGameState::DemonsRemaining - 1);
		}
	}

private:
	float m_AttackTimer = 0.0f;
	float m_HurtFlashTimer = 0.0f;
	bool m_IsFlying = false;
	float m_FlyBobTimer = 0.0f;
	std::shared_ptr<Sound> m_AudioHurt;
	std::shared_ptr<Sound> m_AudioDeath;
	std::shared_ptr<Sound> m_AudioAttack;

	void AttackPlayer()
	{
		auto& tc = GetComponent<TransformComponent>();
		if (m_AudioAttack)
		{
			m_AudioAttack->SetPosition(tc.Position);
			m_AudioAttack->Play();
		}

		int remainingDamage = AttackDamage;
		if (DoomGameState::PlayerArmor > 0)
		{
			int armorAbsorb = std::min(DoomGameState::PlayerArmor, remainingDamage / 2);
			DoomGameState::PlayerArmor -= armorAbsorb;
			remainingDamage -= armorAbsorb;
		}

		DoomGameState::PlayerHealth = std::max(0, DoomGameState::PlayerHealth - remainingDamage);
		DoomGameState::DamageFlashTimer = 0.35f;

		static auto hurtSfx = AudioEngine::Get().CreateSound("res/audio/player_hurt.wav", false, false);
		if (hurtSfx)
		{
			hurtSfx->Play();
		}

		if (DoomGameState::PlayerHealth <= 0)
		{
			DoomGameState::GameOver = true;
		}
	}
};
