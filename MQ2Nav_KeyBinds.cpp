//
// MQ2Nav_Keybinds.cpp
//

#include "MQ2Nav_KeyBinds.h"
#include "MQ2Nav_Settings.h"
#include "MQ2Nav_Util.h"

namespace mq2nav {
namespace keybinds {

//----------------------------------------------------------------------------

// global data
bool initialized_ = false;

int iAutoRun = NULL;
unsigned long* pulAutoRun = NULL;
int iForward = NULL;
unsigned long* pulForward = NULL;
int iBackward = NULL;
unsigned long* pulBackward = NULL;
int iTurnLeft = NULL;
unsigned long* pulTurnLeft = NULL;
int iTurnRight = NULL;
unsigned long* pulTurnRight = NULL;
int iStrafeLeft = NULL;
unsigned long* pulStrafeLeft = NULL;
int iStrafeRight = NULL;
unsigned long* pulStrafeRight = NULL;

int iJumpKey = NULL;
int iRunWalk = NULL;
int iDuckKey = NULL;

char* szFailedLoad[] = {
	"No Error",            // 0
	"TurnRight Address",   // 1
	"TurnRight",           // 2
	"StafeLeft",           // 3
	"StrafeRight",         // 4
	"AutoRun",             // 5
	"TurnLeft",            // 6
	"MoveForward Address", // 7
	"Forward",             // 8
	"AutoRun Mismatch",    // 9
	"Backward"             // 10
};

bool bDoMove = false;

std::function<void()> stopNavigation;

void KeybindPressed(int iKeyPressed, int iKeyDown) {
	if (!util::ValidIngame(false)) return;

	if (iKeyDown)
	{
		if (bDoMove && GetSettings().autobreak) {
			if (stopNavigation)
				stopNavigation();
		}
	}
}

void FwdWrapper(char* szName, int iKeyDown) {
	KeybindPressed(iForward, iKeyDown);
}

void BckWrapper(char* szName, int iKeyDown) {
	KeybindPressed(iBackward, iKeyDown);
}

void LftWrapper(char* szName, int iKeyDown) {
	KeybindPressed(iTurnLeft, iKeyDown);
}

void RgtWrapper(char* szName, int iKeyDown) {
	KeybindPressed(iTurnRight, iKeyDown);
}

void StrafeLftWrapper(char* szName, int iKeyDown) {
	KeybindPressed(iStrafeLeft, iKeyDown);
}

void StrafeRgtWrapper(char* szName, int iKeyDown) {
	KeybindPressed(iStrafeRight, iKeyDown);
}

void AutoRunWrapper(char* szName, int iKeyDown) {
	KeybindPressed(iAutoRun, iKeyDown);
}

void DoKeybinds() {
	if (initialized_) return;
	AddMQ2KeyBind("MUTILS_FWD", FwdWrapper);
	AddMQ2KeyBind("MUTILS_BCK", BckWrapper);
	AddMQ2KeyBind("MUTILS_LFT", LftWrapper);
	AddMQ2KeyBind("MUTILS_RGT", RgtWrapper);
	AddMQ2KeyBind("MUTILS_STRAFE_LFT", StrafeLftWrapper);
	AddMQ2KeyBind("MUTILS_STRAFE_RGT", StrafeRgtWrapper);
	AddMQ2KeyBind("MUTILS_AUTORUN", AutoRunWrapper);
	SetMQ2KeyBind("MUTILS_FWD", FALSE, pKeypressHandler->NormalKey[iForward]);
	SetMQ2KeyBind("MUTILS_BCK", FALSE, pKeypressHandler->NormalKey[iBackward]);
	SetMQ2KeyBind("MUTILS_LFT", FALSE, pKeypressHandler->NormalKey[iTurnLeft]);
	SetMQ2KeyBind("MUTILS_RGT", FALSE, pKeypressHandler->NormalKey[iTurnRight]);
	SetMQ2KeyBind("MUTILS_STRAFE_LFT", FALSE, pKeypressHandler->NormalKey[iStrafeLeft]);
	SetMQ2KeyBind("MUTILS_STRAFE_RGT", FALSE, pKeypressHandler->NormalKey[iStrafeRight]);
	SetMQ2KeyBind("MUTILS_AUTORUN", FALSE, pKeypressHandler->NormalKey[iAutoRun]);
	SetMQ2KeyBind("MUTILS_FWD", TRUE, pKeypressHandler->AltKey[iForward]);
	SetMQ2KeyBind("MUTILS_BCK", TRUE, pKeypressHandler->AltKey[iBackward]);
	SetMQ2KeyBind("MUTILS_LFT", TRUE, pKeypressHandler->AltKey[iTurnLeft]);
	SetMQ2KeyBind("MUTILS_RGT", TRUE, pKeypressHandler->AltKey[iTurnRight]);
	SetMQ2KeyBind("MUTILS_STRAFE_LFT", TRUE, pKeypressHandler->AltKey[iStrafeLeft]);
	SetMQ2KeyBind("MUTILS_STRAFE_RGT", TRUE, pKeypressHandler->AltKey[iStrafeRight]);
	SetMQ2KeyBind("MUTILS_AUTORUN", TRUE, pKeypressHandler->AltKey[iAutoRun]);
	initialized_ = true;
}

void UndoKeybinds()
{
	if (!initialized_) return;
	RemoveMQ2KeyBind("MUTILS_FWD");
	RemoveMQ2KeyBind("MUTILS_BCK");
	RemoveMQ2KeyBind("MUTILS_LFT");
	RemoveMQ2KeyBind("MUTILS_RGT");
	RemoveMQ2KeyBind("MUTILS_STRAFE_LFT");
	RemoveMQ2KeyBind("MUTILS_STRAFE_RGT");
	RemoveMQ2KeyBind("MUTILS_AUTORUN");
	initialized_ = false;
}

void FindKeys()
{
	iForward = FindMappableCommand("forward");
	iBackward = FindMappableCommand("back");
	iAutoRun = FindMappableCommand("autorun");
	iStrafeLeft = FindMappableCommand("strafe_left");
	iStrafeRight = FindMappableCommand("strafe_right");
	iTurnLeft = FindMappableCommand("left");
	iTurnRight = FindMappableCommand("right");
	iJumpKey = FindMappableCommand("jump");
	iDuckKey = FindMappableCommand("duck");
	iRunWalk = FindMappableCommand("run_walk");
}

//----------------------------------------------------------------------------

} // namespace keybinds
} // namespace mq2nav
