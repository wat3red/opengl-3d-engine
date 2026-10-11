#include "project_packager.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <chrono>
#include <algorithm>
#include <print>

namespace fs = std::filesystem;

namespace {

std::string Trim(const std::string& str)
{
	size_t first = str.find_first_not_of(" \t\r\n");
	if (first == std::string::npos) return "";
	size_t last = str.find_last_not_of(" \t\r\n");
	return str.substr(first, (last - first + 1));
}

// Simple key-value JSON string/bool/int/float parser for game.json
bool ParseJsonValue(const std::string& content, const std::string& key, std::string& outVal)
{
	std::string searchKey = "\"" + key + "\"";
	size_t pos = content.find(searchKey);
	if (pos == std::string::npos) return false;

	size_t colon = content.find(':', pos + searchKey.length());
	if (colon == std::string::npos) return false;

	size_t start = content.find_first_not_of(" \t\r\n", colon + 1);
	if (start == std::string::npos) return false;

	if (content[start] == '"')
	{
		size_t end = content.find('"', start + 1);
		if (end != std::string::npos)
		{
			outVal = content.substr(start + 1, end - start - 1);
			return true;
		}
	}
	else
	{
		size_t end = content.find_first_of(",}\r\n", start);
		if (end != std::string::npos)
		{
			outVal = Trim(content.substr(start, end - start));
			return true;
		}
	}
	return false;
}

} // namespace

bool ProjectPackager::DetectAppMode(int argc, char** argv, AppConfig& outConfig)
{
	// 1. Check game.json in root if it exists
	if (fs::exists("game.json"))
	{
		LoadGameConfig("game.json", outConfig);
	}

	// 2. Check executable name (e.g. Doom.exe, IgnisPlayer.exe vs Ignis.exe)
	char exeBuffer[MAX_PATH];
	if (GetModuleFileNameA(NULL, exeBuffer, MAX_PATH))
	{
		std::string exeName = fs::path(exeBuffer).filename().string();
		std::string lowerName = exeName;
		for (auto& c : lowerName) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

		if (lowerName != "ignis.exe" && lowerName != "editor.exe")
		{
			outConfig.Mode = AppMode::Standalone;
			outConfig.GameTitle = fs::path(exeBuffer).stem().string();
		}
	}

	// 3. Command-line argument overrides
	for (int i = 1; i < argc; i++)
	{
		std::string arg = argv[i];
		std::string lowerArg = arg;
		for (auto& c : lowerArg) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

		if (lowerArg == "--game" || lowerArg == "-game" || lowerArg == "--standalone" || lowerArg == "-standalone")
		{
			outConfig.Mode = AppMode::Standalone;
			if (i + 1 < argc && argv[i + 1][0] != '-')
			{
				outConfig.StartingScene = argv[i + 1];
				i++;
			}
		}
		else if (lowerArg == "--editor" || lowerArg == "-editor")
		{
			outConfig.Mode = AppMode::Editor;
		}
		else if (lowerArg.ends_with(".scene"))
		{
			outConfig.Mode = AppMode::Standalone;
			outConfig.StartingScene = arg;
		}
	}

	return outConfig.Mode == AppMode::Standalone;
}

bool ProjectPackager::LoadGameConfig(const std::string& filepath, AppConfig& outConfig)
{
	std::ifstream file(filepath);
	if (!file.is_open()) return false;

	std::stringstream ss;
	ss << file.rdbuf();
	std::string content = ss.str();

	std::string val;
	if (ParseJsonValue(content, "title", val)) outConfig.GameTitle = val;
	if (ParseJsonValue(content, "startingScene", val)) outConfig.StartingScene = val;
	if (ParseJsonValue(content, "gameModule", val)) outConfig.GameModule = val;
	if (ParseJsonValue(content, "windowWidth", val)) outConfig.WindowWidth = std::stoul(val);
	if (ParseJsonValue(content, "windowHeight", val)) outConfig.WindowHeight = std::stoul(val);
	if (ParseJsonValue(content, "fullscreen", val)) outConfig.Fullscreen = (val == "true");
	if (ParseJsonValue(content, "vsync", val)) outConfig.VSync = (val == "true");
	if (ParseJsonValue(content, "standalone", val))
	{
		if (val == "true") outConfig.Mode = AppMode::Standalone;
	}

	return true;
}

bool ProjectPackager::SaveGameConfig(const std::string& filepath, const AppConfig& config)
{
	std::ofstream file(filepath);
	if (!file.is_open()) return false;

	file << "{\n";
	file << "\t\"title\": \"" << config.GameTitle << "\",\n";
	file << "\t\"startingScene\": \"" << config.StartingScene << "\",\n";
	file << "\t\"gameModule\": \"" << config.GameModule << "\",\n";
	file << "\t\"windowWidth\": " << config.WindowWidth << ",\n";
	file << "\t\"windowHeight\": " << config.WindowHeight << ",\n";
	file << "\t\"fullscreen\": " << (config.Fullscreen ? "true" : "false") << ",\n";
	file << "\t\"vsync\": " << (config.VSync ? "true" : "false") << ",\n";
	file << "\t\"standalone\": true\n";
	file << "}\n";

	return true;
}

PackageResult ProjectPackager::Package(const PackageSettings& settings)
{
	auto startTime = std::chrono::high_resolution_clock::now();
	PackageResult result;
	std::error_code ec;

	fs::path outDir = fs::absolute(settings.OutputDirectory, ec);
	result.PackagePath = outDir.string();

	// 1. Build Game.dll if requested
	if (settings.RebuildGameDll)
	{
		std::string msbuildPath = "D:\\Visual Studio Community 2026\\MSBuild\\Current\\Bin\\MSBuild.exe";
		std::string projectPath = fs::absolute("projects/Doom/DoomGame.vcxproj", ec).string();

		if (fs::exists(msbuildPath, ec) && fs::exists(projectPath, ec))
		{
			std::string cmd = "\"" + msbuildPath + "\" \"" + projectPath + "\" /p:Configuration=Release /p:Platform=x64 /m /v:q";
			STARTUPINFOA si = { sizeof(si) };
			PROCESS_INFORMATION pi;
			if (CreateProcessA(NULL, cmd.data(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
			{
				WaitForSingleObject(pi.hProcess, 60000);
				DWORD exitCode = 1;
				GetExitCodeProcess(pi.hProcess, &exitCode);
				CloseHandle(pi.hProcess);
				CloseHandle(pi.hThread);

				if (exitCode != 0)
				{
					result.Success = false;
					result.ErrorMessage = "MSBuild failed for game project with exit code " + std::to_string(exitCode);
					return result;
				}
			}
		}
	}

	// 2. Prepare destination directory
	if (fs::exists(outDir, ec))
	{
		fs::remove_all(outDir, ec);
	}
	fs::create_directories(outDir, ec);
	if (ec)
	{
		result.Success = false;
		result.ErrorMessage = "Failed to create output directory: " + outDir.string();
		return result;
	}

	// 3. Locate engine binary and copy as standalone executable
	fs::path srcExe = "x64/Release/Ignis.exe";
	if (!fs::exists(srcExe, ec))
	{
		srcExe = "Ignis.exe";
	}
	if (!fs::exists(srcExe, ec))
	{
		result.Success = false;
		result.ErrorMessage = "Could not find engine executable (Ignis.exe)!";
		return result;
	}

	std::string targetExeName = settings.ExecutableName.empty() ? (settings.GameName + ".exe") : settings.ExecutableName;
	if (!targetExeName.ends_with(".exe")) targetExeName += ".exe";

	fs::copy_file(srcExe, outDir / targetExeName, fs::copy_options::overwrite_existing, ec);
	if (ec)
	{
		result.Success = false;
		result.ErrorMessage = "Failed to copy executable to " + (outDir / targetExeName).string();
		return result;
	}

	// 4. Copy Game.dll
	fs::path srcDll = "Game.dll";
	if (!fs::exists(srcDll, ec))
	{
		srcDll = "projects/Doom/bin/Game.dll";
	}
	if (fs::exists(srcDll, ec))
	{
		fs::copy_file(srcDll, outDir / "Game.dll", fs::copy_options::overwrite_existing, ec);
	}
	else
	{
		result.Success = false;
		result.ErrorMessage = "Could not find Game.dll!";
		return result;
	}

	// 5. Recursively copy shaders
	if (fs::exists("shaders", ec))
	{
		fs::create_directories(outDir / "shaders", ec);
		for (const auto& entry : fs::recursive_directory_iterator("shaders", ec))
		{
			auto relPath = fs::relative(entry.path(), "shaders", ec);
			auto dstPath = outDir / "shaders" / relPath;
			if (entry.is_directory())
			{
				fs::create_directories(dstPath, ec);
			}
			else if (entry.is_regular_file())
			{
				fs::create_directories(dstPath.parent_path(), ec);
				fs::copy_file(entry.path(), dstPath, fs::copy_options::overwrite_existing, ec);
			}
		}
	}
	else
	{
		result.Success = false;
		result.ErrorMessage = "Missing shaders directory!";
		return result;
	}

	// 6. Recursively copy res
	if (fs::exists("res", ec))
	{
		fs::create_directories(outDir / "res", ec);
		for (const auto& entry : fs::recursive_directory_iterator("res", ec))
		{
			// Skip backups and temp files
			std::string pathStr = entry.path().generic_string();
			if (pathStr.find("/backups/") != std::string::npos || pathStr.ends_with(".bak") || pathStr.ends_with(".tmp"))
			{
				continue;
			}

			auto relPath = fs::relative(entry.path(), "res", ec);
			auto dstPath = outDir / "res" / relPath;
			if (entry.is_directory())
			{
				fs::create_directories(dstPath, ec);
			}
			else if (entry.is_regular_file())
			{
				fs::create_directories(dstPath.parent_path(), ec);
				fs::copy_file(entry.path(), dstPath, fs::copy_options::overwrite_existing, ec);
			}
		}
	}

	// 7. Write game.json configuration manifest
	AppConfig manifest;
	manifest.GameTitle = settings.GameName;
	manifest.StartingScene = settings.StartingScene;
	manifest.GameModule = "Game.dll";
	manifest.WindowWidth = settings.WindowWidth;
	manifest.WindowHeight = settings.WindowHeight;
	manifest.Fullscreen = settings.Fullscreen;
	manifest.VSync = settings.VSync;
	manifest.Mode = AppMode::Standalone;

	SaveGameConfig((outDir / "game.json").string(), manifest);

	// 8. Count files and calculate total size
	size_t totalBytes = 0;
	size_t totalFiles = 0;
	for (const auto& entry : fs::recursive_directory_iterator(outDir, ec))
	{
		if (entry.is_regular_file())
		{
			totalFiles++;
			totalBytes += entry.file_size(ec);
		}
	}
	result.TotalBytes = totalBytes;
	result.TotalFiles = totalFiles;

	// 9. Create ZIP archive if requested
	if (settings.CreateZipArchive)
	{
		fs::path zipFile = outDir.parent_path() / (settings.GameName + "_v1.0_Windows_x64.zip");
		result.ZipPath = zipFile.string();

		if (fs::exists(zipFile, ec))
		{
			fs::remove(zipFile, ec);
		}

		std::string psCmd = "powershell -Command \"Compress-Archive -Path '" + outDir.string() + "\\*' -DestinationPath '" + zipFile.string() + "' -Force\"";
		STARTUPINFOA si = { sizeof(si) };
		PROCESS_INFORMATION pi;
		if (CreateProcessA(NULL, psCmd.data(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
		{
			WaitForSingleObject(pi.hProcess, 120000);
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
		}
	}

	auto endTime = std::chrono::high_resolution_clock::now();
	result.DurationSeconds = std::chrono::duration<float>(endTime - startTime).count();
	result.Success = true;

	std::println("[ProjectPackager] Successfully packaged game '{}' to '{}' ({} files, {:.1f} MB in {:.1f}s)",
		settings.GameName, result.PackagePath, result.TotalFiles,
		result.TotalBytes / (1024.0f * 1024.0f), result.DurationSeconds);

	return result;
}

void ProjectPackager::LaunchPackagedGame(const std::string& exePath)
{
	if (!fs::exists(exePath)) return;

	std::string dir = fs::path(exePath).parent_path().string();
	ShellExecuteA(NULL, "open", exePath.c_str(), NULL, dir.c_str(), SW_SHOWNORMAL);
}

void ProjectPackager::OpenFolderInExplorer(const std::string& folderPath)
{
	if (!fs::exists(folderPath)) return;
	ShellExecuteA(NULL, "open", folderPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
}
