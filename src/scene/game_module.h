#pragma once

#include <string>
#include "ignis/api.h"

class GameModule {
public:
	static GameModule& Get();

	bool Load(const std::string& dllPath = "Game.dll");
	void Unload();
	bool Reload();

	bool IsLoaded() const { return m_Module != nullptr; }
	const std::string& GetModuleName() const { return m_ModuleName; }
	const std::string& GetModulePath() const { return m_ModulePath; }

private:
	GameModule();
	~GameModule();

	void SetupContext();

	void* m_Module = nullptr;
	std::string m_ModulePath = "Game.dll";
	std::string m_RuntimePath = "Game_runtime.dll";
	std::string m_ModuleName = "None";
	Ignis::EngineContext m_Context{};
};
