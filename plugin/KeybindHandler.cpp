//
// KeybindHandler.cpp
//

#include "KeybindHandler.h"

#include "plugin/MQ2Navigation.h"

#include <string>
#include <vector>

//----------------------------------------------------------------------------

namespace
{
	KeybindHandler* s_handler = nullptr;

	std::vector<std::string> BindList = {
		"forward",
		"back",
		"left",
		"right",
		"strafe_left",
		"strafe_right",
		"autorun",
		"jump",
		"run_walk",
		"duck"
	};

	void NavKeyPressed(char*, int keyDown)
	{
		if (keyDown && s_handler)
		{
			s_handler->OnMovementKeyPressed();
		}
	}
}

KeybindHandler::KeybindHandler()
{
	s_handler = this;
}

KeybindHandler::~KeybindHandler()
{
	s_handler = nullptr;
}

void KeybindHandler::Initialize()
{
	InstallKeybinds();
}

void KeybindHandler::Shutdown()
{
	RemoveKeybinds();
}

void KeybindHandler::InstallKeybinds()
{
	if (m_initialized) return;

	char bindName[32] = { 0 };
	for (auto& bind : BindList)
	{
		strcpy_s(bindName, "NAVKEY_");
		strcat_s(bindName, bind.c_str());

		int keybind = FindMappableCommand(bind.c_str());

		// bind to both the normal and alt key so we get notified when the
		// key has been pressed.
		AddMQ2KeyBind(bindName, NavKeyPressed);
		SetMQ2KeyBind(bindName, FALSE, pKeypressHandler->NormalKey[keybind]);
		SetMQ2KeyBind(bindName, TRUE, pKeypressHandler->AltKey[keybind]);
	}

	m_initialized = true;
}

void KeybindHandler::RemoveKeybinds()
{
	if (!m_initialized) return;

	char bindName[32] = { 0 };
	for (auto& bind : BindList)
	{
		strcpy_s(bindName, "NAVKEY_");
		strcat_s(bindName, bind.c_str());

		// Remove the custom key binding
		RemoveMQ2KeyBind(bindName);
	}

	m_initialized = false;
}

void KeybindHandler::ReloadKeybinds()
{
	RemoveKeybinds();
	InstallKeybinds();
}

//----------------------------------------------------------------------------
