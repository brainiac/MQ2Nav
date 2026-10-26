//
// Main.cpp
//

#include "meshgen/Application.h"
#include "meshgen/HeadlessCli.h"

int main(int argc, char* argv[])
{
	if (argc > 1)
	{
		const std::string command = argv[1];
		if (command == "build" || command == "validate"
			|| command == "--help" || command == "-h")
		{
			return RunHeadlessCli(argc, argv);
		}
	}

	std::string startingZone;
	if (argc > 1)
		startingZone = argv[1];

	Application window(startingZone);
	return window.RunMainLoop();
}
