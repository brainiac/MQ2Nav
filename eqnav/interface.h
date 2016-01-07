#pragma once
#define _USE_MATH_DEFINES
#include <math.h>
#include "Interface.h"
#include "InputGeom.h"
#include "Sample_TileMesh.h"
#include "Windows.h"
#pragma warning (disable : 4091)
virtual class Interface
{
public:
	Interface(const char* everquest_path = NULL, const char* output_path = NULL);
	inline ~Interface() {};
	bool ShowDialog(const char* defaultZone = NULL);
	void Halt();
	void StartBuild();
	void LoadGeom(char* zoneShortName);
private:
	void DefineDirectories(bool bypassSettings = false);
};
