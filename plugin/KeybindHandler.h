//
// KeybindHandler.h
//

#pragma once

#include "common/NavModule.h"
#include "common/Signal.h"

//----------------------------------------------------------------------------

class KeybindHandler : public NavModule
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
