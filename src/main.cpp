#include "engine.h"
#include <print>
#include <exception>
#include <cstdlib>

int main()
{
	try
	{
		Engine engine;
		engine.Run();
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
