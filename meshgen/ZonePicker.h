//
// ZonePicker.h
//

#pragma once

#include "meshgen/ApplicationConfig.h"
#include <bgfx/bgfx.h>

#include <map>
#include <memory>
#include <string>

class Application;

struct IMAGEDATA
{
	int width, height, bits;
	uint8_t* data;
	uint32_t textureId = 0;
};

class ZonePicker
{
public:
	ZonePicker(Application* app);
	~ZonePicker();

	void Show();
	void Draw();
	void Close();

	bool IsShowing() const { return m_isShowing; }

private:
	bool DrawExpansionGroup(const ApplicationConfig::Expansion& expansion, bool showExpansions);
	void LoadZones();
	void ClearZones();

	Application* m_app;

	// Mapping of Zones to Expansions
	struct MapInfo
	{
		std::string longName;
		std::string shortName;
		std::string expansion;
	};
	std::vector<MapInfo> m_allMaps;

	char m_filterText[64] = { 0 };

	bool m_loadNavMesh = true;
	bool m_showExpansionButtons = true;
	bool m_isShowing = true;
	bool m_setFocus = false;
	bool m_loaded = false;
	int m_selectedExpansion = -1;
	bool m_showNextDraw = false;

	std::vector<bgfx::TextureHandle> m_textures;
};
