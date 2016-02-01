//
// KeybindHandler.h
//

#pragma once

#include "NavModule.h"
#include "Signal.h"

//----------------------------------------------------------------------------

class KeybindHandler : NavModule
{
public:
	KeybindHandler();
	virtual ~KeybindHandler();

	virtual void Initialize();
	virtual void Shutdown();

	void ReloadKeybinds();

	Signal<> OnMovementKeyPressed;

private:
	void InstallKeybinds();
	void RemoveKeybinds();

	bool m_initialized = false;
};
