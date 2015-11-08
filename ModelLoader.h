//
//  ModelLoader.h
//

#pragma once

#include "MQ2Navigation.h"
#include "Signal.h"
#include "ZoneData.h"

#include "dependencies/zone-utilities/common/eqg_loader.h"
#include "dependencies/zone-utilities/common/s3d_loader.h"

#include <d3dx9.h>
#include <d3d9caps.h>

#include <string>
#include <vector>
#include <map>

class ModelData;

class ModelLoader
{
public:
	ModelLoader();
	~ModelLoader();

	void Process();

	void SetZoneId(int zoneId);
	void Reset();

	void OnUpdateUI();

	void Initialize();
	void Shutdown();

private:
	void RenderDoorObjectUI(PDOOR door, bool target = false);

private:
	Signal<>::ScopedConnection m_uiConn;
	int m_zoneId = 0;
	std::string m_zoneFile;
	DWORD m_lastDoorTargetId = 0;

	std::unique_ptr<ZoneData> m_zoneData;

	std::map<std::string, std::shared_ptr<EQEmu::EQG::Geometry>> m_models;
	std::map<int, std::shared_ptr<ModelData>> m_modelData;
};

void DumpDataUI(void* ptr, DWORD length);