//
// Main.cpp
//

#include "meshgen/Application.h"

int main(int argc, char* argv[])
{
	Application app;

	if (!app.Initialize(argc, argv))
	{
		return 1;
	}

	while (true)
	{
		if (!app.update())
			break;
	}

	return app.Shutdown();
}
