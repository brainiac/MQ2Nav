//
// ZonePicker.h
//

#pragma once

#include "meshgen/EQConfig.h"
#include <bgfx/bgfx.h>

#include <map>
#include <memory>
#include <string>

struct IMAGEDATA
{
	int width, height, bits;
	uint8_t* data;
	uint32_t textureId = 0;
};

class ZonePicker
{
public:
	ZonePicker(const EQConfig& eqConfig, bool batchMode = false);
	~ZonePicker();

	bool Show(bool focus);

	bool ShouldLoadNavMesh() const { return m_loadNavMesh; }
	std::string GetSelectedZone() const { return m_selectedZone; }

private:
	bool DrawExpansionGroup(const EQConfig::Expansion& expansion, bool showExpansions);

	const EQConfig& m_eqConfig;

	// Mapping of Zones to Expansions
	struct MapInfo
	{
		std::string longName;
		std::string shortName;
		std::string expansion;
	};
	std::vector<MapInfo> m_allMaps;

	std::string m_eqDirectory;
	char m_filterText[64] = { 0 };

	std::string m_selectedZone;

	bool m_loadNavMesh = true;
	bool m_showExpansionButtons = true;
	bool m_batchMode = false;
	int m_selectedExpansion = -1;

	std::vector<bgfx::TextureHandle> m_textures;
};
