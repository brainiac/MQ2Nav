//
// MQ2Nav_Input.h
//

#pragma once

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>


void InstallInputHooks();
void RemoveInputHooks();

void RenderInputUI();