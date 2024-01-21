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

#if defined(LoadImage)
#undef LoadImage
#endif

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

#include <glm/glm.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <SDL2/SDL.h>
#include <bgfx/bgfx.h>
