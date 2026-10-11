#include "engine.h"
#include "scene/game_module.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <print>
#include <exception>
#include <cstdlib>
#include <filesystem>

int main(int argc, char** argv)
{
	try
	{
		namespace fs = std::filesystem;
		std::error_code ec;

		// Ensure current working directory points to executable folder if assets are local to exe
		if (!fs::exists("shaders", ec) || !fs::exists("res", ec))
		{
			char exePath[MAX_PATH];
			if (GetModuleFileNameA(NULL, exePath, MAX_PATH))
			{
				fs::path exeDir = fs::path(exePath).parent_path();
				if (fs::exists(exeDir / "shaders", ec) && fs::exists(exeDir / "res", ec))
				{
					fs::current_path(exeDir, ec);
				}
			}
		}

		Engine engine(argc, argv);

		// Dynamically load Game module (e.g. Game.dll or configured module)
		std::string moduleName = engine.GetConfig().GameModule;
		if (moduleName.empty()) moduleName = "Game.dll";
		GameModule::Get().Load(moduleName);

		engine.Run();

		GameModule::Get().Unload();
	}
	catch (const std::exception& ex)
	{
		std::println(stderr, "[Ignis Fatal Error] {}", ex.what());
		return EXIT_FAILURE;
	}
	catch (...)
	{
		std::println(stderr, "[Ignis Fatal Error] An unhandled unknown exception occurred.");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
