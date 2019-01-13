//
// MQ2Navigation.h
//

#pragma once

#include "common/Context.h"
#include "common/NavModule.h"
#include "common/Signal.h"
#include "plugin/MapAPI.h"

#include <MQ2Plugin.h>
#include <memory>
#include <chrono>
#include <typeinfo>
#include <unordered_map>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

#define PLUGIN_MSG "\ag[MQ2Nav]\ax "


#if !defined(GAMESTATE_ZONING)
#define GAMESTATE_ZONING 4
#endif

enum class TabPage;
enum class HookStatus;

//----------------------------------------------------------------------------

class dtNavMesh;
class MQ2NavigationPlugin;
class MQ2NavigationType;
class NavigationPath;
class NavigationMapLine;
class ModelLoader;
class RenderHandler;
class NavMeshLoader;
class UiController;
class KeybindHandler;
class ImGuiRenderer;
class NavMesh;
class SwitchHandler;

//----------------------------------------------------------------------------

enum class DestinationType
{
	None,
	Location,
	Door,
	GroundItem,
	Spawn,
	Waypoint,
};

enum class ClickType
{
	None,
	Once
};

enum class HeightType
{
	Explicit,
	Nearest
};

enum class NotifyType
{
	None,
	Errors,
	All
};

struct NavigationOptions
{
	float distance = 0.f;         // distance to target
	bool lineOfSight = true;       // does target need to be in los
};

struct DestinationInfo
{
	std::string command;
	glm::vec3 eqDestinationPos;

	DestinationType type = DestinationType::None;

	PSPAWNINFO pSpawn = nullptr;
	PDOOR pDoor = nullptr;
	PGROUNDITEM pGroundItem = nullptr;
	ClickType clickType = ClickType::None;
	HeightType heightType = HeightType::Explicit;
	NavigationOptions options;

	bool valid = false;
};

glm::vec3 GetSpawnPosition(PSPAWNINFO pSpawn);

//----------------------------------------------------------------------------

class PluginContext : public Context
{
public:
	// Log something...
	virtual void Log(LogLevel logLevel, const char* format, ...);

};

//----------------------------------------------------------------------------

class MQ2NavigationPlugin
{
public:
	MQ2NavigationPlugin();
	~MQ2NavigationPlugin();

	using clock = std::chrono::system_clock;

	void Plugin_Initialize();
	void Plugin_Shutdown();

	// standard plugin interface functions
	void Plugin_OnPulse();
	void Plugin_OnBeginZone();
	void Plugin_OnEndZone();
	void Plugin_SetGameState(DWORD GameState);
	void Plugin_OnAddGroundItem(PGROUNDITEM pGroundItem);
	void Plugin_OnRemoveGroundItem(PGROUNDITEM pGroundItem);

	bool IsInitialized() const { return m_initialized; }
	bool InitializationFailed() const { return m_initializationFailed; }

	// Handler for /navigate
	void Command_Navigate(const char* szLine);

	std::string GetDataDirectory() const;

	//------------------------------------------------------------------------
	// modules

	template <typename T, typename... Args>
	T* AddModule(Args&&... args)
	{
		auto result = m_modules.emplace(std::move(std::make_pair(
			typeid(T).hash_code(),
			std::unique_ptr<NavModule>(new T(std::forward<Args>(args)...)))));
		return static_cast<T*>(result.first->second.get());
	}

	template <typename T>
	T* Get() const
	{
		return static_cast<T*>(m_modules.at(typeid(T).hash_code()).get());
	}

	//------------------------------------------------------------------------
	// constants

	static const int MAX_POLYS = 4028;

	// minimum distance to a waypoint for moving to the next waypoint
	static const int WAYPOINT_PROGRESSION_DISTANCE = 5;

	// stopping distance at the final waypoint
	static const int ENDPOINT_STOP_DISTANCE = 15;

	// how often to update the path (in milliseconds)
	static const int PATHFINDING_DELAY_MS = 200;

	//----------------------------------------------------------------------------

	bool IsActive() const { return m_isActive; }
	bool IsPaused() const { return m_isPaused; }
	bool IsMeshLoaded() const;

	// Check if a point is pathable (given a coordinate string)
	bool CanNavigateToPoint(const char* szLine);

	// Check how far away a point is (given a coordinate string)
	float GetNavigationPathLength(const char* szLine);

	// Parse a destination command from string
	std::shared_ptr<DestinationInfo> ParseDestination(const char* szLine,
		NotifyType notify = NotifyType::Errors);

	void ParseOptions(const char* szLine, int index,
		NavigationOptions& target);

	// Begin navigating to a point
	void BeginNavigation(const std::shared_ptr<DestinationInfo>& dest);

	// Get the map line object
	std::shared_ptr<NavigationMapLine> GetMapLine() const { return m_mapLine; }

private:
	void InitializeRenderer();
	void ShutdownRenderer();

	void UpdateCurrentZone();
	void SetCurrentZone(int zoneId);

	void OnUpdateTab(TabPage tabId);

	std::shared_ptr<DestinationInfo> ParseDestinationInternal(const char* szLine,
		int& argIndex,
		NotifyType notify);

	//----------------------------------------------------------------------------

	float GetNavigationPathLength(const std::shared_ptr<DestinationInfo>& pos);

	void AttemptClick();

	void StuckCheck();

	void LookAt(const glm::vec3& pos);

	void AttemptMovement();
	void Stop();

	void OnMovementKeyPressed();

private:
	std::unique_ptr<PluginContext> m_context;

	std::shared_ptr<NavigationPath> m_activePath;

	// todo: factor out the navpath rendering and map line into
	//       modules based on active path.
	std::shared_ptr<NavigationMapLine> m_mapLine;

	Signal<>::ScopedConnection m_uiConn;

	bool m_initialized = false;
	int m_zoneId = -1;

	bool m_retryHooks = false;
	bool m_initializationFailed = false;

	// ending criteria (pick up item / click door)
	PDOOR m_pEndingDoor = nullptr;
	PGROUNDITEM m_pEndingItem = nullptr;

	// whether the current path is active or not
	bool m_isActive = false;
	glm::vec3 m_currentWaypoint;

	// if paused, path will not be followed
	bool m_isPaused = false;

	clock::time_point m_lastClick = clock::now();

	clock::time_point m_stuckTimer = clock::now();
	float m_stuckX = 0;
	float m_stuckY = 0;

	clock::time_point m_pathfindTimer = clock::now();

	Signal<>::ScopedConnection m_keypressConn;
	Signal<TabPage>::ScopedConnection m_updateTabConn;

	std::unordered_map<size_t, std::unique_ptr<NavModule>> m_modules;
	NavigationOptions m_defaultOptions;
};

extern std::unique_ptr<MQ2NavigationPlugin> g_mq2Nav;

extern std::unique_ptr<RenderHandler> g_renderHandler;
extern std::unique_ptr<ImGuiRenderer> g_imguiRenderer;

//----------------------------------------------------------------------------

class NavigationMapLine
{
public:
	NavigationMapLine();

	bool GetEnabled() const { return m_enabled; };
	void SetEnabled(bool enabled);

	void SetNavigationPath(NavigationPath* path);

	void Clear();

	uint32_t GetColor() const;
	void SetColor(uint32_t color);

	void SetLayer(int layer);
	int GetLayer() const;

private:
	void RebuildLine();

private:
	NavigationPath* m_path = nullptr;
	bool m_enabled = true;
	Signal<>::ScopedConnection m_updateConn;

	nav::MapLine m_mapLine;
	nav::MapCircle m_mapCircle;
};

//----------------------------------------------------------------------------

void ClickDoor(PDOOR pDoor);

//============================================================================
