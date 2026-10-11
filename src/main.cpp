#include "engine.h"
#include "scene/game_module.h"
#include <print>
#include <exception>
#include <cstdlib>

int main()
{
	try
	{
		// Dynamically load Game module (e.g. Game.dll)
		GameModule::Get().Load("Game.dll");

		Engine engine;
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
