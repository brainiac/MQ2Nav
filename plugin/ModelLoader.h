//
//  ModelLoader.h
//

#pragma once

#include "common/NavModule.h"
#include "common/ZoneData.h"
#include "plugin/MQ2Navigation.h"

#include "dependencies/zone-utilities/common/eqg_loader.h"
#include "dependencies/zone-utilities/common/wld_loader.h"

#include <d3d9caps.h>

#include <string>
#include <vector>
#include <map>

class ModelData;
class DoorsDebugUI;

class ModelLoader : public NavModule
{
public:
	ModelLoader();
	~ModelLoader();

	virtual void Initialize() override;
	virtual void Shutdown() override;
	virtual void OnPulse() override;

	void SetZoneId(int zoneId) override;
	void Reset();

	void OnUpdateUI(bool visible);

private:
	void RenderDoorObjectUI(EQSwitch* door, bool target = false);

	void UpdateModels();
	void DumpDoors();

private:
	int m_zoneId = 0;
	std::string m_zoneFile;
	int m_lastDoorTargetId = -1;
	int m_lastDoorTargetRenderId = -1;
	int m_loadedDoorCount = 0;

	std::map<std::string, std::shared_ptr<EQEmu::EQG::Geometry>> m_models;
	std::map<int, std::shared_ptr<ModelData>> m_modelData;

	std::unique_ptr<DoorsDebugUI> m_doorsUI;
};

void DumpDataUI(void* ptr, DWORD length);
