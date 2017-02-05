//
// NavModule.h
//

#pragma once

class NavModule
{
public:
	virtual ~NavModule() {}

	virtual void Initialize() {}
	virtual void Shutdown() {}

	virtual void OnPulse() {}
	virtual void OnBeginZone() {}
	virtual void OnEndZone() {}

	virtual void SetZoneId(int zoneId) {}
	virtual void SetGameState(int gameState) {}
};
