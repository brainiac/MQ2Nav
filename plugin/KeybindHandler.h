//
// KeybindHandler.h
//

#pragma once

#include "common/NavModule.h"

#include <mq/base/Signal.h>

//----------------------------------------------------------------------------

class KeybindHandler : public NavModule
{
public:
	KeybindHandler();
	virtual ~KeybindHandler();

	virtual void Initialize();
	virtual void Shutdown();

	void ReloadKeybinds();

	mq::Signal<> OnMovementKeyPressed;

private:
	void InstallKeybinds();
	void RemoveKeybinds();

	bool m_initialized = false;
};
