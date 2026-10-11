#pragma once

#include <ignis/ignis.h>
#include "doom_game_state.h"
#include <cmath>
#include <algorithm>
#include <string>

enum class PickupType {
	Health,
	Ammo
};

class DoomPickup : public ScriptableEntity {
public:
	PickupType Type = PickupType::Health;
	int Amount = 25;
	bool Collected = false;

	void OnCreate() override
	{
		if (HasComponent<TagComponent>())
		{
			const auto& tag = GetComponent<TagComponent>().Tag;
			std::string lowerTag = tag;
			for (auto& c : lowerTag) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

			if (lowerTag.find("ammo") != std::string::npos || lowerTag.find("shell") != std::string::npos)
			{
				Type = PickupType::Ammo;
				Amount = 12;
			}
			else
			{
				Type = PickupType::Health;
				Amount = 25;
			}
		}

		m_InitialY = GetComponent<TransformComponent>().Position.y;
		m_PickupSfx = AudioEngine::Get().CreateSound("res/audio/item_pickup.wav", false, false);

		if (!HasComponent<MaterialComponent>())
		{
			GetEntity().AddComponent<MaterialComponent>();
		}
		auto& mat = GetComponent<MaterialComponent>();
		if (Type == PickupType::Health)
		{
			mat.Properties.EmissionColor = glm::vec3(0.1f, 0.9f, 0.2f); // glowing green
			mat.Properties.EmissionIntensity = 2.0f;
		}
		else
		{
			mat.Properties.EmissionColor = glm::vec3(0.95f, 0.8f, 0.1f); // glowing yellow
			mat.Properties.EmissionIntensity = 2.0f;
		}
	}

	void OnUpdate(float deltaTime) override
	{
		if (Collected || deltaTime <= 0.0f)
			return;

		m_Time += deltaTime;
		auto& tc = GetComponent<TransformComponent>();

		// Bob and spin
		tc.Rotation.y += 120.0f * deltaTime;
		tc.Position.y = m_InitialY + std::sin(m_Time * 4.0f) * 0.18f;

		// Check distance to player
		float dist = glm::distance(tc.Position, DoomGameState::PlayerPosition);
		if (dist <= 2.0f)
		{
			bool canCollect = false;

			if (Type == PickupType::Health)
			{
				if (DoomGameState::PlayerHealth < DoomGameState::PlayerMaxHealth)
				{
					DoomGameState::PlayerHealth = std::min(DoomGameState::PlayerMaxHealth, DoomGameState::PlayerHealth + Amount);
					canCollect = true;
				}
			}
			else if (Type == PickupType::Ammo)
			{
				if (DoomGameState::ShellsAmmo < DoomGameState::MaxShells)
				{
					DoomGameState::ShellsAmmo = std::min(DoomGameState::MaxShells, DoomGameState::ShellsAmmo + Amount);
					canCollect = true;
				}
			}

			if (canCollect)
			{
				Collected = true;
				if (m_PickupSfx)
				{
					m_PickupSfx->Play();
				}
				tc.Scale = glm::vec3(0.001f);
				tc.Position.y = -50.0f;
			}
		}
	}

private:
	float m_InitialY = 0.0f;
	float m_Time = 0.0f;
	std::shared_ptr<Sound> m_PickupSfx;
};
