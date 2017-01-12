// requires:
// defining IMGUI_INCLUDE_IMGUI_USER_H and IMGUI_INCLUDE_IMGUI_USER_INL
// at the project level

#pragma once

#define NO_IMGUIFILESYSTEM

#pragma warning (push)
#pragma warning (disable: 4244)
#pragma warning (disable: 4305)
#pragma warning (disable: 4800)
#pragma warning (disable: 4996)

#include "addons/imgui_user.h"

#pragma warning (pop)
