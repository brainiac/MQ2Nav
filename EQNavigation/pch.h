// pch.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>

#define _USE_MATH_DEFINES
#include <math.h>

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <stdio.h>

#include <iostream>
#include <tchar.h>
#include <io.h>
#include <string>
#include <fstream>

#include <shlobj.h>
#include <Shlwapi.h>

#include "SDL.h"
#include "SDL_opengl.h"
#include <SDL_syswm.h>
