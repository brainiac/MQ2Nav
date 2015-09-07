//
// MQ2Nav_KeyBinds.h
//

#pragma once

#include "MQ2Navigation.h"

#include <functional>

namespace mq2nav {
namespace keybinds {

	extern bool bDoMove;

	extern std::function<void()> stopNavigation;

	void KeybindPressed(int iKeyPressed, int iKeyDown);

	void FwdWrapper(char* szName, int iKeyDown);

	void BckWrapper(char* szName, int iKeyDown);

	void LftWrapper(char* szName, int iKeyDown);

	void RgtWrapper(char* szName, int iKeyDown);

	void StrafeLftWrapper(char* szName, int iKeyDown);

	void StrafeRgtWrapper(char* szName, int iKeyDown);

	void AutoRunWrapper(char* szName, int iKeyDown);

	void DoKeybinds();
	void UndoKeybinds();

	void FindKeys();

} // namespace keybinds
} // namespace mq2nav
