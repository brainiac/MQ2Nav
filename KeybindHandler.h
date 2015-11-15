//
// KeybindHandler.h
//

#pragma once

#include "MQ2Navigation.h"
#include "Signal.h"

//----------------------------------------------------------------------------

class KeybindHandler
{
public:
	KeybindHandler();
	~KeybindHandler();

	void ReloadKeybinds();

	Signal<> OnMovementKeyPressed;

private:
	void InstallKeybinds();
	void RemoveKeybinds();

	bool m_initialized = false;
};

extern std::unique_ptr<KeybindHandler> g_keybindHandler;
