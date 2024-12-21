//
// ZonePicker.h
//

#pragma once

#include "meshgen/ApplicationConfig.h"
#include <bgfx/bgfx.h>

#include <map>
#include <memory>
#include <optional>
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
	ZonePicker() = default;
	~ZonePicker();

	void Show();
	void Close();

	// Draw the picker. Returns true if a map was selected
	bool Draw();

	bool IsShowing() const { return m_isShowing; }

	bool ShouldLoadNavMesh() const { return m_loadNavMesh; }
	const std::string& GetSelectedZone() const { return m_selectedZone; }

private:
	void LoadZones();
	void ClearZones();

	// Mapping of Zones to Expansions
	struct MapInfo
	{
		std::string longName;
		std::string shortName;
		std::string expansion;
	};
	std::vector<MapInfo> m_allMaps;

	bool DrawZoneList(const char* str_id, std::ranges::input_range auto&& input_range,
		bool enterPressed, int* baseIndex = nullptr);
	void CheckSelection();

	char m_filterText[64] = { 0 };

	bool m_loadNavMesh = true;
	bool m_showExpansionButtons = true;
	bool m_isShowing = true;
	bool m_setFocus = false;
	bool m_loaded = false;
	bool m_showNextDraw = false;

	std::vector<bgfx::TextureHandle> m_textures;

	int m_selectedExpansion = -1;
	std::string m_selectedZone;
	int m_selectedIndex = -1;
	int m_lastSelectedIndex = -1;
	int m_lastItemCount = -1;
	bool m_foundSelected = false;
	std::pair<bool, int> m_toggleTreeNodeState = { false, -1 };
};
