#pragma once

#include <glm/glm.hpp>

struct DoomGameState {
	static inline int PlayerHealth = 100;
	static inline int PlayerMaxHealth = 100;
	static inline int PlayerArmor = 50;
	static inline int PlayerMaxArmor = 100;
	static inline int ShellsAmmo = 24;
	static inline int MaxShells = 60;

	static inline int DemonsRemaining = 0;
	static inline int Score = 0;

	static inline float HitmarkerTimer = 0.0f;
	static inline float DamageFlashTimer = 0.0f;
	static inline float MuzzleFlashTimer = 0.0f;

	static inline bool GameOver = false;
	static inline bool Victory = false;

	static inline glm::vec3 PlayerPosition{ 0.0f, 1.8f, 0.0f };
	static inline glm::vec3 PlayerForward{ 0.0f, 0.0f, -1.0f };

	static void Reset()
	{
		PlayerHealth = 100;
		PlayerMaxHealth = 100;
		PlayerArmor = 50;
		PlayerMaxArmor = 100;
		ShellsAmmo = 24;
		MaxShells = 60;
		DemonsRemaining = 0;
		Score = 0;
		HitmarkerTimer = 0.0f;
		DamageFlashTimer = 0.0f;
		MuzzleFlashTimer = 0.0f;
		GameOver = false;
		Victory = false;
		PlayerPosition = glm::vec3(0.0f, 1.8f, 0.0f);
		PlayerForward = glm::vec3(0.0f, 0.0f, -1.0f);
	}
};
