// main.cpp
#include "engine.h"
#include <iostream>
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
		std::cerr << "[Ignis Fatal Error] " << ex.what() << std::endl;
		return EXIT_FAILURE;
	}
	catch (...)
	{
		std::cerr << "[Ignis Fatal Error] An unhandled unknown exception occurred." << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
