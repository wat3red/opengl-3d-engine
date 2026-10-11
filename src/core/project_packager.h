#pragma once

#include <string>
#include <cstdint>
#include <vector>

enum class AppMode {
	Editor,
	Standalone
};

struct AppConfig {
	AppMode Mode = AppMode::Editor;
	std::string GameTitle = "Doom: Hell on Earth";
	std::string StartingScene = "res/scenes/default.scene";
	std::string GameModule = "Game.dll";
	uint32_t WindowWidth = 1600;
	uint32_t WindowHeight = 900;
	bool Fullscreen = false;
	bool VSync = true;
};

struct PackageSettings {
	std::string GameName = "Doom";
	std::string ExecutableName = "Doom.exe";
	std::string StartingScene = "res/scenes/default.scene";
	std::string OutputDirectory = "dist/Doom";
	uint32_t WindowWidth = 1600;
	uint32_t WindowHeight = 900;
	bool Fullscreen = false;
	bool VSync = true;
	bool RebuildGameDll = true;
	bool CreateZipArchive = true;
};

struct PackageResult {
	bool Success = false;
	std::string ErrorMessage;
	std::string PackagePath;
	std::string ZipPath;
	size_t TotalBytes = 0;
	size_t TotalFiles = 0;
	float DurationSeconds = 0.0f;
};

class ProjectPackager {
public:
	static bool DetectAppMode(int argc, char** argv, AppConfig& outConfig);

	static bool LoadGameConfig(const std::string& filepath, AppConfig& outConfig);
	static bool SaveGameConfig(const std::string& filepath, const AppConfig& config);

	static PackageResult Package(const PackageSettings& settings);

	static void LaunchPackagedGame(const std::string& exePath);
	static void OpenFolderInExplorer(const std::string& folderPath);
};
