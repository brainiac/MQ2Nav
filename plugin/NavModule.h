//
// NavModule.h
//

#pragma once

class NavModule
{
public:
	virtual ~NavModule() {}

	virtual void Initialize() = 0;
	virtual void Shutdown() = 0;

	virtual void OnPulse() {}
	virtual void OnBeginZone() {}
	virtual void OnEndZone() {}

	virtual void SetZoneId(int zoneId) {}
	virtual void SetGameState(int gameState) {}
};
