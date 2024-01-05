//
// MeshgenHandler.h
//

#pragma once

#include "common/NavModule.h"

#include <mq/Plugin.h>

class MeshgenHandler : public NavModule
{
public:
	void Initialize() override;
	void Shutdown() override;

	void OnPulse() override;

	void SetGameState(int gameState) override;

private:
	std::string m_characterInfo;
};