// pch.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#endif

// Windows Header Files:
#include <windows.h>

#include <math.h>

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <stdio.h>
#include <cstdint>

#include <iostream>
#include <tchar.h>
#include <io.h>
#include <string>
#include <fstream>

#include <shlobj.h>
#include <Shlwapi.h>

#include <SDL.h>
#include <SDL_syswm.h>
