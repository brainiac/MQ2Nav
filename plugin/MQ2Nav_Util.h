//
// MQ2Nav_Util.h
//

#pragma once

#include "MQ2Navigation.h"

#include <functional>

namespace mq2nav {

//----------------------------------------------------------------------------
// Utility
inline bool ValidIngame(bool bCheckDead)
{
	// CTD prevention function
	return (GetGameState() == GAMESTATE_INGAME && GetCharInfo()->pSpawn && GetCharInfo2() && GetCharInfo() && GetCharInfo()->Stunned != 3);
}

} // namespace mq2nav

//----------------------------------------------------------------------------
// I should probably put this in some awesome shared location
class scope_guard
{
public:
	template<class Callable>
	scope_guard(Callable && undo_func) : f(std::forward<Callable>(undo_func)) {}
	scope_guard(scope_guard && other) : f(std::move(other.f)) {}
	~scope_guard() {
		if (f) f(); // must not throw
	}

	void dismiss() throw() {
		f = nullptr;
	}

	scope_guard(const scope_guard&) = delete;
	void operator=(const scope_guard&) = delete;

private:
	std::function<void()> f;
};

void ClickDoor(PDOOR pDoor);
