#pragma once

#include <ignis/ignis.h>
#include "doom_game_state.h"
#include <string>
#include <algorithm>

inline void DrawRetroDoomHUD()
{
	Ignis::EnsureImGuiContext();
	if (!ImGui::GetCurrentContext())
		return;

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	if (!drawList)
		return;

	ImVec2 winPos = ImGui::GetWindowPos();
	ImVec2 winSize = ImGui::GetWindowSize();
	ImVec2 center(winPos.x + winSize.x * 0.5f, winPos.y + winSize.y * 0.5f);

	// 1. Crosshair
	const float chSize = 7.0f;
	const float chGap = 3.0f;
	ImU32 chCol = IM_COL32(230, 40, 40, 240); // Red crosshair
	drawList->AddLine(ImVec2(center.x - chSize - chGap, center.y), ImVec2(center.x - chGap, center.y), chCol, 2.0f);
	drawList->AddLine(ImVec2(center.x + chGap, center.y), ImVec2(center.x + chSize + chGap, center.y), chCol, 2.0f);
	drawList->AddLine(ImVec2(center.x, center.y - chSize - chGap), ImVec2(center.x, center.y - chGap), chCol, 2.0f);
	drawList->AddLine(ImVec2(center.x, center.y + chGap), ImVec2(center.x, center.y + chSize + chGap), chCol, 2.0f);
	drawList->AddCircleFilled(center, 1.5f, chCol);

	// 2. Hitmarker (White X on demon hit)
	if (DoomGameState::HitmarkerTimer > 0.0f)
	{
		float hmSize = 10.0f;
		ImU32 hmCol = IM_COL32(255, 255, 255, (int)(std::min(1.0f, DoomGameState::HitmarkerTimer / 0.15f) * 255.0f));
		drawList->AddLine(ImVec2(center.x - hmSize, center.y - hmSize), ImVec2(center.x + hmSize, center.y + hmSize), hmCol, 2.0f);
		drawList->AddLine(ImVec2(center.x + hmSize, center.y - hmSize), ImVec2(center.x - hmSize, center.y + hmSize), hmCol, 2.0f);
	}

	// 3. Screen Damage Flash (Red overlay)
	if (DoomGameState::DamageFlashTimer > 0.0f)
	{
		float alpha = std::min(1.0f, DoomGameState::DamageFlashTimer / 0.35f);
		drawList->AddRectFilled(winPos, ImVec2(winPos.x + winSize.x, winPos.y + winSize.y), IM_COL32(220, 20, 20, (int)(alpha * 125.0f)));
	}

	// 4. Muzzle Flash (Warm yellow/orange burst)
	if (DoomGameState::MuzzleFlashTimer > 0.0f)
	{
		float alpha = std::min(1.0f, DoomGameState::MuzzleFlashTimer / 0.08f);
		drawList->AddRectFilled(winPos, ImVec2(winPos.x + winSize.x, winPos.y + winSize.y), IM_COL32(255, 210, 80, (int)(alpha * 65.0f)));
	}

	// 5. Retro Doom Status Bar (Bottom of viewport)
	float barH = 50.0f;
	float barMargin = 15.0f;
	ImVec2 barMin(winPos.x + barMargin, winPos.y + winSize.y - barH - barMargin);
	ImVec2 barMax(winPos.x + winSize.x - barMargin, winPos.y + winSize.y - barMargin);

	drawList->AddRectFilled(barMin, barMax, IM_COL32(18, 18, 22, 235), 4.0f);
	drawList->AddRect(barMin, barMax, IM_COL32(180, 40, 40, 255), 4.0f, 0, 2.5f);

	float textY = barMin.y + 16.0f;
	float usableW = barMax.x - barMin.x;
	float colW = usableW / 5.0f;

	// HEALTH
	ImU32 hpColor = (DoomGameState::PlayerHealth > 50) ? IM_COL32(60, 225, 70, 255) :
	                (DoomGameState::PlayerHealth > 25) ? IM_COL32(245, 210, 40, 255) : IM_COL32(245, 40, 40, 255);
	std::string hpStr = "+ HEALTH: " + std::to_string(DoomGameState::PlayerHealth) + "%";
	drawList->AddText(ImVec2(barMin.x + 15.0f, textY), hpColor, hpStr.c_str());

	// ARMOR
	std::string armorStr = "SHIELD: " + std::to_string(DoomGameState::PlayerArmor) + "%";
	drawList->AddText(ImVec2(barMin.x + colW + 10.0f, textY), IM_COL32(70, 180, 255, 255), armorStr.c_str());

	// SHELLS
	std::string ammoStr = "SHELLS: " + std::to_string(DoomGameState::ShellsAmmo) + " / " + std::to_string(DoomGameState::MaxShells);
	drawList->AddText(ImVec2(barMin.x + colW * 2.0f + 10.0f, textY), IM_COL32(255, 200, 50, 255), ammoStr.c_str());

	// DEMONS
	std::string enemyStr = "DEMONS: " + std::to_string(DoomGameState::DemonsRemaining);
	drawList->AddText(ImVec2(barMin.x + colW * 3.0f + 10.0f, textY), IM_COL32(240, 60, 60, 255), enemyStr.c_str());

	// SCORE
	std::string scoreStr = "SCORE: " + std::to_string(DoomGameState::Score);
	drawList->AddText(ImVec2(barMin.x + colW * 4.0f + 10.0f, textY), IM_COL32(230, 230, 230, 255), scoreStr.c_str());

	// 6. Game Over / Victory Overlays
	if (DoomGameState::GameOver)
	{
		ImVec2 bannerMin(center.x - 220.0f, center.y - 70.0f);
		ImVec2 bannerMax(center.x + 220.0f, center.y + 70.0f);
		drawList->AddRectFilled(bannerMin, bannerMax, IM_COL32(40, 5, 5, 235), 8.0f);
		drawList->AddRect(bannerMin, bannerMax, IM_COL32(230, 30, 30, 255), 8.0f, 0, 3.0f);

		drawList->AddText(ImVec2(center.x - 85.0f, center.y - 45.0f), IM_COL32(255, 40, 40, 255), "=== YOU DIED ===");
		drawList->AddText(ImVec2(center.x - 145.0f, center.y - 15.0f), IM_COL32(220, 200, 200, 255), "DEMONS CONSUMED YOUR FLESH & SOUL");
		drawList->AddText(ImVec2(center.x - 110.0f, center.y + 18.0f), IM_COL32(255, 230, 80, 255), "Press [R] to Respawn & Restart");
	}
	else if (DoomGameState::Victory)
	{
		ImVec2 bannerMin(center.x - 220.0f, center.y - 70.0f);
		ImVec2 bannerMax(center.x + 220.0f, center.y + 70.0f);
		drawList->AddRectFilled(bannerMin, bannerMax, IM_COL32(15, 35, 15, 235), 8.0f);
		drawList->AddRect(bannerMin, bannerMax, IM_COL32(60, 230, 60, 255), 8.0f, 0, 3.0f);

		drawList->AddText(ImVec2(center.x - 105.0f, center.y - 45.0f), IM_COL32(60, 255, 80, 255), "*** VICTORY ACHIEVED ***");
		drawList->AddText(ImVec2(center.x - 135.0f, center.y - 15.0f), IM_COL32(220, 240, 220, 255), "ARENA PURGED OF ALL HELLSPAWN!");
		std::string finalScore = "Final Score: " + std::to_string(DoomGameState::Score) + "  |  Press [R] to Replay";
		drawList->AddText(ImVec2(center.x - 140.0f, center.y + 18.0f), IM_COL32(255, 230, 80, 255), finalScore.c_str());
	}
}

class DoomHUD : public ScriptableEntity {
public:
	void OnGUI() override
	{
		Ignis::EnsureImGuiContext();
		if (!ImGui::GetCurrentContext())
			return;

		DrawRetroDoomHUD();
	}
};
