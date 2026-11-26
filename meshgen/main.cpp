//
// Main.cpp
//

#include "meshgen/Application.h"
#include "meshgen/HeadlessBuild.h"

#include <cstdio>
#include <string>
#include <Windows.h>

int main(int argc, char* argv[])
{
	if (argc > 1 && std::string(argv[1]) == "build")
	{
		// MeshGenerator is a windows program. A handle that was redirected (a pipe or a file) is
		// already connected and is left alone; any other is pointed at the console it was started
		// from, if there is one.
		const auto connected = [](DWORD which) {
			const HANDLE handle = GetStdHandle(which);
			return handle != nullptr && handle != INVALID_HANDLE_VALUE && GetFileType(handle) != FILE_TYPE_UNKNOWN;
		};
		const bool outConnected = connected(STD_OUTPUT_HANDLE);
		const bool errConnected = connected(STD_ERROR_HANDLE);
		if ((!outConnected || !errConnected) && AttachConsole(ATTACH_PARENT_PROCESS))
		{
			FILE* stream = nullptr;
			if (!outConnected)
				freopen_s(&stream, "CONOUT$", "w", stdout);
			if (!errConnected)
				freopen_s(&stream, "CONOUT$", "w", stderr);
		}
		return RunHeadlessBuild(argc, argv);
	}

	std::string startingZone;
	if (argc > 1)
		startingZone = argv[1];

	Application window(startingZone);
	return window.RunMainLoop();
}
