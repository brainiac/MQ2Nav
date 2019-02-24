//
// MQ2Navigation.cpp
//

#include "pch.h"
#include "MQ2Navigation.h"

#include "common/Logging.h"
#include "common/NavMesh.h"
#include "plugin/ImGuiRenderer.h"
#include "plugin/KeybindHandler.h"
#include "plugin/ModelLoader.h"
#include "plugin/PluginHooks.h"
#include "plugin/PluginSettings.h"
#include "plugin/NavigationPath.h"
#include "plugin/NavigationType.h"
#include "plugin/NavMeshLoader.h"
#include "plugin/NavMeshRenderer.h"
#include "plugin/RenderHandler.h"
#include "plugin/SwitchHandler.h"
#include "plugin/UiController.h"
#include "plugin/Utilities.h"
#include "plugin/Waypoints.h"

#include <fmt/format.h>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/trigonometric.hpp>
#include <imgui.h>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <imgui/misc/fonts/IconsFontAwesome.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>
#include <chrono>
#include <set>

#pragma comment (lib, "d3d9.lib")
#pragma comment (lib, "d3dx9.lib")

using namespace std::chrono_literals;

//============================================================================

std::unique_ptr<RenderHandler> g_renderHandler;
std::unique_ptr<ImGuiRenderer> g_imguiRenderer;

// ----------------------------------------
// key IDs & pointers
// Borrowed from MQ2MoveUtils, pending rewrite using new movement handler

int iAutoRun = 0;
int iForward = 0;
int iBackward = 0;
int iTurnLeft = 0;
int iTurnRight = 0;
int iStrafeLeft = 0;
int iStrafeRight = 0;
int iJumpKey = 0;
int iDuckKey = 0;
int iRunWalk = 0;

unsigned long* pulAutoRun = NULL;
unsigned long* pulForward = NULL;
unsigned long* pulBackward = NULL;
unsigned long* pulTurnLeft = NULL;
unsigned long* pulTurnRight = NULL;
unsigned long* pulStrafeLeft = NULL;
unsigned long* pulStrafeRight = NULL;

enum MOVEMENT_DIR
{
	GO_FORWARD = 1,
	GO_BACKWARD,
	GO_LEFT,
	GO_RIGHT,
	APPLY_TO_ALL,
};

void InitKeys()
{
	iForward = FindMappableCommand("forward");
	iBackward = FindMappableCommand("back");
	iAutoRun = FindMappableCommand("autorun");
	iStrafeLeft = FindMappableCommand("strafe_left");
	iStrafeRight = FindMappableCommand("strafe_right");
	iTurnLeft = FindMappableCommand("left");
	iTurnRight = FindMappableCommand("right");
	iJumpKey = FindMappableCommand("jump");
	iDuckKey = FindMappableCommand("duck");
	iRunWalk = FindMappableCommand("run_walk");

	pulAutoRun = (unsigned long *)FixOffset(__pulAutoRun_x);
	pulForward = (unsigned long *)FixOffset(__pulForward_x);
	pulBackward = (unsigned long *)FixOffset(__pulBackward_x);
	pulTurnRight = (unsigned long *)FixOffset(__pulTurnRight_x);
	pulTurnLeft = (unsigned long *)FixOffset(__pulTurnLeft_x);
	pulStrafeLeft = (unsigned long *)FixOffset(__pulStrafeLeft_x);
	pulStrafeRight = (unsigned long *)FixOffset(__pulStrafeRight_x);
}

void TrueMoveOn(MOVEMENT_DIR ucDirection)
{
	switch (ucDirection)
	{
	case GO_FORWARD:
		pKeypressHandler->CommandState[iAutoRun] = 0;
		*pulAutoRun = 0;
		pKeypressHandler->CommandState[iBackward] = 0;
		*pulBackward = 0;
		pKeypressHandler->CommandState[iForward] = 1;
		*pulForward = 1;
		break;
	case GO_BACKWARD:
		pKeypressHandler->CommandState[iAutoRun] = 0;
		*pulAutoRun = 0;
		pKeypressHandler->CommandState[iForward] = 0;
		*pulForward = 0;
		pKeypressHandler->CommandState[iBackward] = 1;
		*pulBackward = 1;
		break;
	case GO_LEFT:
		pKeypressHandler->CommandState[iAutoRun] = 0;
		*pulAutoRun = 0;
		pKeypressHandler->CommandState[iStrafeRight] = 0;
		*pulStrafeRight = 0;
		pKeypressHandler->CommandState[iStrafeLeft] = 1;
		*pulStrafeLeft = 1;
		break;
	case GO_RIGHT:
		pKeypressHandler->CommandState[iAutoRun] = 0;
		*pulAutoRun = 0;
		pKeypressHandler->CommandState[iStrafeLeft] = 0;
		*pulStrafeLeft = 0;
		pKeypressHandler->CommandState[iStrafeRight] = 1;
		*pulStrafeRight = 1;
		break;
	}
};

void TrueMoveOff(MOVEMENT_DIR ucDirection)
{
	switch (ucDirection)
	{
	case APPLY_TO_ALL:
		pKeypressHandler->CommandState[iAutoRun] = 0;
		*pulAutoRun = 0;
		pKeypressHandler->CommandState[iStrafeLeft] = 0;
		*pulStrafeLeft = 0;
		pKeypressHandler->CommandState[iStrafeRight] = 0;
		*pulStrafeRight = 0;
		pKeypressHandler->CommandState[iForward] = 0;
		*pulForward = 0;
		pKeypressHandler->CommandState[iBackward] = 0;
		*pulBackward = 0;
		break;
	case GO_FORWARD:
		pKeypressHandler->CommandState[iAutoRun] = 0;
		*pulAutoRun = 0;
		pKeypressHandler->CommandState[iForward] = 0;
		*pulForward = 0;
		break;
	case GO_BACKWARD:
		pKeypressHandler->CommandState[iAutoRun] = 0;
		*pulAutoRun = 0;
		pKeypressHandler->CommandState[iBackward] = 0;
		*pulBackward = 0;
		break;
	case GO_LEFT:
		pKeypressHandler->CommandState[iAutoRun] = 0;
		*pulAutoRun = 0;
		pKeypressHandler->CommandState[iStrafeLeft] = 0;
		*pulStrafeLeft = 0;
		break;
	case GO_RIGHT:
		pKeypressHandler->CommandState[iAutoRun] = 0;
		*pulAutoRun = 0;
		pKeypressHandler->CommandState[iStrafeRight] = 0;
		*pulStrafeRight = 0;
		break;
	}
};



//============================================================================

static void NavigateCommand(PSPAWNINFO pChar, PCHAR szLine)
{
	if (g_mq2Nav)
	{
		g_mq2Nav->Command_Navigate(szLine);
	}
}

static void ClickSwitch(CVector3 pos, EQSwitch* pSwitch)
{
	pSwitch->UseSwitch(GetCharInfo()->pSpawn->SpawnID, 0xFFFFFFFF, 0, nullptr);
}

void ClickDoor(PDOOR pDoor)
{
	CVector3 click;
	click.Y = pDoor->X;
	click.X = pDoor->Y;
	click.Z = pDoor->Z;
	
	ClickSwitch(click, (EQSwitch*)pDoor);
}

static void ClickGroundItem(PGROUNDITEM pGroundItem)
{
	CHAR szName[MAX_STRING] = { 0 };
	GetFriendlyNameForGroundItem(pGroundItem, szName, sizeof(szName));

	CHAR Command[MAX_STRING] = { 0 };
	sprintf_s(Command, "/itemtarget \"%s\"", szName);
	HideDoCommand((PSPAWNINFO)pLocalPlayer, Command, FALSE);

	sprintf_s(Command, "/click left item");
	HideDoCommand((PSPAWNINFO)pLocalPlayer, Command, FALSE);
}

glm::vec3 GetSpawnPosition(PSPAWNINFO pSpawn)
{
	if (pSpawn)
	{
		bool use_floor_height = nav::GetSettings().use_spawn_floor_height;

		return glm::vec3{ pSpawn->X, pSpawn->Y, use_floor_height ? pSpawn->FloorHeight : pSpawn->Z };
	}

	return glm::vec3{};
}

glm::vec3 GetMyPosition()
{
	auto charInfo = GetCharInfo();
	if (!charInfo)
		return {};

	return GetSpawnPosition(charInfo->pSpawn);
}

class WriteChatSink : public spdlog::sinks::base_sink<spdlog::details::null_mutex>
{
public:
	void set_enabled(bool enabled) { enabled_ = enabled; }
	bool enabled() const { return enabled_; }

protected:
	void sink_it_(const spdlog::details::log_msg& msg) override
	{
		if (!enabled_)
			return;

		using namespace spdlog;

		fmt::memory_buffer formatted;
		switch (msg.level)
		{
		case level::critical:
		case level::err:
			fmt::format_to(formatted, PLUGIN_MSG "\ar{}", msg.payload);
			break;

		case level::trace:
		case level::debug:
			fmt::format_to(formatted, PLUGIN_MSG "\a#7f7f7f{}", msg.payload);
			break;

		case level::warn:
			fmt::format_to(formatted, PLUGIN_MSG, "\ay{}", msg.payload);
			break;

		case level::info:
		default:
			fmt::format_to(formatted, PLUGIN_MSG "{}", msg.payload);
			break;
		}

		WriteChatf("%s", fmt::to_string(formatted).c_str());
	}

	void flush_() override {}

	bool enabled_ = true;
};

spdlog::level::level_enum ExtractLogLevel(std::string_view input,
	spdlog::level::level_enum defaultLevel)
{
	auto pos = input.find("log=");
	if (pos == std::string_view::npos)
		return defaultLevel;

	pos += 4;

	auto end = input.find_first_of(" \t\r\n", pos);
	std::string_view fragment = input.substr(pos, (end == std::string_view::npos) ? end : end - pos);

	int level = 0;
	for (const auto& level_str : spdlog::level::level_string_views)
	{
		if (level_str == fragment)
			return static_cast<spdlog::level::level_enum>(level);

		level++;
	}

	return defaultLevel;
}

//============================================================================

#pragma region MQ2Navigation Plugin Class
MQ2NavigationPlugin::MQ2NavigationPlugin()
{
	InitKeys();

	std::string logFile = std::string(gszINIPath) + "\\MQ2Nav.log";

	// set up default logger
	auto logger = spdlog::create<spdlog::sinks::basic_file_sink_mt>("MQ2Nav", logFile, true);
	m_chatSink = std::make_shared<WriteChatSink>();
	m_chatSink->set_level(spdlog::level::info);

	logger->sinks().push_back(m_chatSink);
#if defined(_DEBUG)
	logger->sinks().push_back(std::make_shared<spdlog::sinks::msvc_sink_mt>());
#endif
	spdlog::set_default_logger(logger);
	spdlog::set_pattern("%L %Y-%m-%d %T.%f [%n] %v (%@)");
	spdlog::flush_on(spdlog::level::debug);
}

MQ2NavigationPlugin::~MQ2NavigationPlugin()
{
}

void MQ2NavigationPlugin::Plugin_OnPulse()
{
	if (!m_initialized)
	{
		// don't try to initialize until we get into the game
		if (GetGameState() != GAMESTATE_INGAME)
			return;

		if (m_retryHooks)
		{
			m_retryHooks = false;
			Plugin_Initialize();
		}
		return;
	}

	for (const auto& m : m_modules)
	{
		m.second->OnPulse();
	}

	if (m_initialized && nav::ValidIngame(TRUE))
	{
		AttemptMovement();
		StuckCheck();
	}
}

void MQ2NavigationPlugin::Plugin_OnBeginZone()
{
	if (!m_initialized)
		return;

	UpdateCurrentZone();

	// stop active path if one exists
	ResetPath();

	for (const auto& m : m_modules)
	{
		m.second->OnBeginZone();
	}
}

void MQ2NavigationPlugin::Plugin_OnEndZone()
{
	if (!m_initialized)
		return;

	for (const auto& m : m_modules)
	{
		m.second->OnEndZone();
	}

	pDoorTarget = nullptr;
	pGroundTarget = nullptr;
}

void MQ2NavigationPlugin::Plugin_SetGameState(DWORD GameState)
{
	if (!m_initialized)
	{
		if (m_retryHooks) {
			m_retryHooks = false;
			Plugin_Initialize();
		}
		return;
	}

	if (GameState == GAMESTATE_INGAME) {
		UpdateCurrentZone();
	}
	else if (GameState == GAMESTATE_CHARSELECT) {
		SetCurrentZone(-1);
	}

	for (const auto& m : m_modules)
	{
		m.second->SetGameState(GameState);
	}
}

void MQ2NavigationPlugin::Plugin_OnAddGroundItem(PGROUNDITEM pGroundItem)
{
}

void MQ2NavigationPlugin::Plugin_OnRemoveGroundItem(PGROUNDITEM pGroundItem)
{
	// if the item we targetted for navigation has been removed, clear it out
	if (m_pEndingItem == pGroundItem)
	{
		m_pEndingItem = nullptr;

		if (m_isActive)
		{
			auto info = m_activePath->GetDestinationInfo();

			if (info->pGroundItem == pGroundItem)
			{
				info->pGroundItem = nullptr;
			}
		}
	}
}

void MQ2NavigationPlugin::Plugin_OnAddSpawn(PSPAWNINFO pSpawn)
{
}

void MQ2NavigationPlugin::Plugin_OnRemoveSpawn(PSPAWNINFO pSpawn)
{
	// if the spawn is the current destination target we should clear i.t
	if (m_isActive)
	{
		auto info = m_activePath->GetDestinationInfo();

		if (info->pSpawn == pSpawn)
		{
			info->pSpawn = nullptr;
		}
	}
}

void MQ2NavigationPlugin::Plugin_Initialize()
{
	if (m_initialized)
		return;

	HookStatus status = InitializeHooks();
	if (status != HookStatus::Success)
	{
		m_retryHooks = (status == HookStatus::MissingDevice);
		m_initializationFailed = (status == HookStatus::MissingOffset);
		return;
	}

	nav::LoadSettings();

	InitializeRenderer();

	AddModule<KeybindHandler>();

	NavMesh* mesh = AddModule<NavMesh>(GetDataDirectory());
	AddModule<NavMeshLoader>(mesh);

	AddModule<ModelLoader>();
	AddModule<NavMeshRenderer>();
	AddModule<UiController>();
	AddModule<SwitchHandler>();

	for (const auto& m : m_modules)
	{
		m.second->Initialize();
	}

	// get the keybind handler and connect it to our keypress handler
	auto keybindHandler = Get<KeybindHandler>();
	m_keypressConn = keybindHandler->OnMovementKeyPressed.Connect(
		[this]() { OnMovementKeyPressed(); });

	// initialize mesh loader's settings
	auto meshLoader = Get<NavMeshLoader>();
	meshLoader->SetAutoReload(nav::GetSettings().autoreload);

	m_initialized = true;

	Plugin_SetGameState(gGameState);

	AddCommand("/navigate", NavigateCommand);

	InitializeMQ2NavMacroData();

	auto ui = Get<UiController>();
	m_updateTabConn = ui->OnTabUpdate.Connect([=](TabPage page) { OnUpdateTab(page); });

	m_mapLine = std::make_shared<NavigationMapLine>();
	m_gameLine = std::make_shared<NavigationLine>();

	g_renderHandler->AddRenderable(m_gameLine.get());
}

void MQ2NavigationPlugin::Plugin_Shutdown()
{
	if (!m_initialized)
		return;
	
	RemoveCommand("/navigate");
	ShutdownMQ2NavMacroData();

	g_renderHandler->RemoveRenderable(m_gameLine.get());

	Stop();

	// shut down all of the modules
	for (const auto& m : m_modules)
	{
		m.second->Shutdown();
	}

	// delete all of the modules
	m_modules.clear();

	ShutdownRenderer();
	ShutdownHooks();
	
	m_initialized = false;
	spdlog::shutdown();
}

std::string MQ2NavigationPlugin::GetDataDirectory() const
{
	// the root path is where we look for all of our mesh files
	return std::string(gszINIPath) + "\\MQ2Nav";
}

void MQ2NavigationPlugin::SetLogLevel(spdlog::level::level_enum level)
{
	m_chatSink->set_level(level);
}

spdlog::level::level_enum MQ2NavigationPlugin::GetLogLevel() const
{
	return m_chatSink->level();
}

//----------------------------------------------------------------------------

void MQ2NavigationPlugin::InitializeRenderer()
{
	g_renderHandler = std::make_unique<RenderHandler>();

	HWND eqhwnd = *reinterpret_cast<HWND*>(EQADDR_HWND);
	g_imguiRenderer = std::make_unique<ImGuiRenderer>(eqhwnd, g_pDevice);
}

void MQ2NavigationPlugin::ShutdownRenderer()
{
	g_imguiRenderer.reset();

	if (g_renderHandler)
	{
		g_renderHandler->Shutdown();
		g_renderHandler.reset();
	}
}

//----------------------------------------------------------------------------

static void DoHelp()
{
	WriteChatf(PLUGIN_MSG "Usage:");
	WriteChatf(PLUGIN_MSG "\ag/nav [save | load]\ax - save/load settings");
	WriteChatf(PLUGIN_MSG "\ag/nav reload\ax - reload navmesh");
	WriteChatf(PLUGIN_MSG "\ag/nav recordwaypoint|rwp \"<waypoint name>\" [\"<waypoint description>\"]\ax - create a waypoint at current location");
	WriteChatf(PLUGIN_MSG "\ag/nav listwp\ax - list waypoints");

	WriteChatf(PLUGIN_MSG "\aoNavigation Commands:\ax");
	WriteChatf(PLUGIN_MSG "\ag/nav target\ax - navigate to target");
	WriteChatf(PLUGIN_MSG "\ag/nav id #\ax - navigate to target with ID = #");
	WriteChatf(PLUGIN_MSG "\ag/nav loc[yxz] Y X Z\ax - navigate to coordinates");
	WriteChatf(PLUGIN_MSG "\ag/nav locxyz X Y Z\ax - navigate to coordinates");
	WriteChatf(PLUGIN_MSG "\ag/nav locyx Y X\ax - navigate to 2d coordinates");
	WriteChatf(PLUGIN_MSG "\ag/nav locxy X Y\ax - navigate to 2d coordinates");
	WriteChatf(PLUGIN_MSG "\ag/nav item [click]\ax - navigate to item (and click it)");
	WriteChatf(PLUGIN_MSG "\ag/nav door [item_name | id #] [click]\ax - navigate to door/object (and click it)");
	WriteChatf(PLUGIN_MSG "\ag/nav setopt [options | reset]\ax - Set the default value for options used for navigation. See \agOptions\ax below. Pass \"reset\" instead to reset them to their default values.");
	WriteChatf(PLUGIN_MSG "\ag/nav spawn <spawn search> | [options]\ax - navigate to spawn via spawn search query. If you want to provide options to navigate, like distance, you need to separate them by a | (pipe)");
	WriteChatf(PLUGIN_MSG "\ag/nav waypoint|wp <waypoint>\ax - navigate to waypoint");
	WriteChatf(PLUGIN_MSG "\ag/nav stop\ax - stop navigation");
	WriteChatf(PLUGIN_MSG "\ag/nav pause\ax - pause navigation");
	WriteChatf(PLUGIN_MSG "\aoNavigation Options:\ax");
	WriteChatf(PLUGIN_MSG "Options can be provided to navigation commands to alter their behavior.");
	WriteChatf(PLUGIN_MSG "  \aydistance=<num>\ax - set the distance to navigate from the destination. shortcut: \agdist\ax");
	WriteChatf(PLUGIN_MSG "  \aylineofsight=<on/off>\ax - when using distance, require visibility of target. Default is \ayon\ax. shortcut: \aglos\ax");
	WriteChatf(PLUGIN_MSG "  \aylog=<level>\ax - adjust log level for command. can be \aytrace\ax, \aydebug\ax, \ayinfo\ax, \aywarning\ax, \ayerror\ax, \aycritical\ax or \ayoff\ax. The default is \ayinfo.");
	WriteChatf(PLUGIN_MSG "  \aypaused\ax - start navigation in a paused state.");
	WriteChatf(PLUGIN_MSG "  \aynotrack\ax - disable tracking of spawn movement. By default, when navigating to a spawn, the destination location will track the spawn's position. This disables that behavior.");

	// NYI:
	//WriteChatf(PLUGIN_MSG "\ag/nav options set \ay[options]\ax - save options as defaults");
	//WriteChatf(PLUGIN_MSG "\ag/nav options reset\ax - reset options to default");
}

void MQ2NavigationPlugin::Command_Navigate(const char* szLine)
{
	CHAR buffer[MAX_STRING] = { 0 };
	int i = 0;

	SPDLOG_DEBUG("Handling Command: {}", szLine);

	spdlog::level::level_enum requestedLevel = ExtractLogLevel(szLine, m_defaultOptions.logLevel);
	if (requestedLevel != m_defaultOptions.logLevel)
		SPDLOG_DEBUG("Log level temporarily set to: {}", spdlog::level::to_string_view(requestedLevel));

	ScopedLogLevel scopedLevel{ *m_chatSink, requestedLevel };

	GetArg(buffer, szLine, 1);

	// parse /nav ui
	if (!_stricmp(buffer, "ui")) {
		nav::GetSettings().show_ui = !nav::GetSettings().show_ui;
		nav::SaveSettings(false);
		return;
	}
	
	// parse /nav pause
	if (!_stricmp(buffer, "pause"))
	{
		if (!m_isActive)
		{
			SPDLOG_WARN("Navigation must be active to pause");
			return;
		}
		m_isPaused = !m_isPaused;
		if (m_isPaused)
		{
			SPDLOG_INFO("Pausing Navigation");

			TrueMoveOff(APPLY_TO_ALL);
		}
		else
		{
			SPDLOG_INFO("Resuming Navigation");
		}
		
		return;
	}

	// parse /nav stop
	if (!_stricmp(buffer, "stop"))
	{
		if (m_isActive)
		{
			Stop();
		}
		else
		{
			SPDLOG_ERROR("No navigation path currently active");
		}

		return;
	}

	// parse /nav reload
	if (!_stricmp(buffer, "reload"))
	{
		Get<NavMeshLoader>()->LoadNavMesh();
		return;
	}

	// parse /nav recordwaypoint or /nav rwp
	if (!_stricmp(buffer, "recordwaypoint") || !_stricmp(buffer, "rwp"))
	{
		GetArg(buffer, szLine, 2);
		std::string waypointName(buffer);
		GetArg(buffer, szLine, 3);
		std::string desc(buffer);

		if (waypointName.empty())
		{
			SPDLOG_INFO("Usage: /nav rwp <waypoint name> <waypoint description>");
			return;
		}

		if (PSPAWNINFO pChar = (GetCharInfo() ? GetCharInfo()->pSpawn : nullptr))
		{
			glm::vec3 loc = GetSpawnPosition(pChar);
			nav::AddWaypoint(nav::Waypoint{ waypointName, loc, desc });

			SPDLOG_INFO("Recorded waypoint: {} at {}", waypointName, loc.yxz());
		}

		return;
	}

	if (!_stricmp(buffer, "listwp"))
	{
		WriteChatf(PLUGIN_MSG "\ag%d\ax waypoint(s) for \ag%s\ax:", nav::g_waypoints.size(), GetShortZone(m_zoneId));

		for (const nav::Waypoint& wp : nav::g_waypoints)
		{
			WriteChatf(PLUGIN_MSG "  \at%s\ax: \a-w%s\ax \ay(%.2f, %.2f, %.2f)",
				wp.name.c_str(), wp.description.c_str(), wp.location.y, wp.location.x, wp.location.z);
		}
		return;
	}

	// parse /nav load
	if (!_stricmp(buffer, "load"))
	{
		nav::LoadSettings(true);
		return;
	}
	
	// parse /nav save
	if (!_stricmp(buffer, "save"))
	{
		nav::SaveSettings(true);
		return;
	}

	// parse /nav help
	if (!_stricmp(buffer, "help"))
	{
		DoHelp();

		return;
	}

	// parse /nav setopt [options]
	if (!_stricmp(buffer, "setopt"))
	{
		int idx = 2;

		GetArg(buffer, szLine, idx);

		if (!_stricmp(buffer, "reset"))
		{
			// reset
			m_defaultOptions = NavigationOptions{};

			SPDLOG_INFO("Navigation options reset");
		}
		else
		{
			ParseOptions(szLine, idx, m_defaultOptions);

			SPDLOG_INFO("Navigation options updated");
			SPDLOG_DEBUG("Navigation options updated: {}", szLine);
		}

		return;
	}

	scopedLevel.Release();
	
	// all thats left is a navigation command. leave if it isn't a valid one.
	auto destination = ParseDestination(szLine, requestedLevel);
	if (!destination->valid)
		return;

	BeginNavigation(destination);
}

//----------------------------------------------------------------------------

void MQ2NavigationPlugin::UpdateCurrentZone()
{
	int zoneId = -1;

	if (PCHARINFO pChar = GetCharInfo())
	{
		zoneId = pChar->zoneId;

		zoneId &= 0x7FFF;
		if (zoneId >= MAX_ZONES)
			zoneId = -1;
	}

	SetCurrentZone(zoneId);
}

void MQ2NavigationPlugin::SetCurrentZone(int zoneId)
{
	if (m_zoneId != zoneId)
	{
		m_zoneId = zoneId;

		if (m_zoneId == -1)
			SPDLOG_DEBUG("Resetting zone");
		else
			SPDLOG_DEBUG("Switching to zone: {}", m_zoneId);

		nav::LoadWaypoints(m_zoneId);

		for (const auto& m : m_modules)
		{
			m.second->SetZoneId(m_zoneId);
		}
	}
}

void MQ2NavigationPlugin::BeginNavigation(const std::shared_ptr<DestinationInfo>& destInfo)
{
	assert(destInfo);

	// first clear existing state
	ResetPath();

	if (!destInfo->valid)
		return;

	auto mesh = Get<NavMesh>();
	if (!mesh->IsNavMeshLoaded())
	{
		SPDLOG_ERROR("Cannot navigate - no mesh file loaded.");
		return;
	}

	m_chatSink->set_level(destInfo->options.logLevel);

	if (destInfo->clickType != ClickType::None)
	{
		m_pEndingDoor = destInfo->pDoor;
		m_pEndingItem = destInfo->pGroundItem;
	}

	m_activePath = std::make_shared<NavigationPath>(destInfo);

	// resolve the height parameter of a 2d coordinate
	if (destInfo && destInfo->heightType == HeightType::Nearest)
	{
		glm::vec3 transformed = destInfo->eqDestinationPos.xzy();
		auto heights = mesh->GetHeights(transformed);

		// disable logging for these calculations
		ScopedLogLevel logScope{ *m_chatSink, spdlog::level::off };

		// we don't actually want to calculate the path at the current height since there
		// is no guarantee it is on the mesh
		float current_distance = std::numeric_limits<float>().max();

		// create a destination location out of the given x/y coordinate and each z coordinate
		// that was hit at that location. Calculate the length of each and take the z coordinate that
		// creates the shortest path.
		for (auto height : heights)
		{
			float current_height = destInfo->eqDestinationPos.z;
			destInfo->eqDestinationPos.z = height;

			if (!m_activePath->FindPath() || m_activePath->GetPathTraversalDistance() > current_distance)
			{
				destInfo->eqDestinationPos.z = current_height;
			} // else leave it, it's a better height
		}
	}

	if (m_activePath->FindPath())
	{
		m_activePath->SetShowNavigationPaths(nav::GetSettings().show_nav_path);
		m_isActive = m_activePath->GetPathSize() > 0;
	}

	m_mapLine->SetNavigationPath(m_activePath.get());
	m_gameLine->SetNavigationPath(m_activePath.get());
	Get<SwitchHandler>()->SetActive(m_isActive);

	// check if we start paused.
	if (destInfo->options.paused)
	{
		m_isPaused = true;
	}

	if (m_isActive)
	{
		EzCommand("/squelch /stick off");
	}
}

bool MQ2NavigationPlugin::IsMeshLoaded() const
{
	if (NavMesh* mesh = Get<NavMesh>())
	{
		return mesh->IsNavMeshLoaded();
	}

	return false;
}

void MQ2NavigationPlugin::OnMovementKeyPressed()
{
	if (m_isActive)
	{
		if (nav::GetSettings().autobreak)
		{
			Stop();
		}
		else if (nav::GetSettings().autopause)
		{
			m_isPaused = true;
		}
	}
}

//============================================================================

void MQ2NavigationPlugin::AttemptClick()
{
	if (!m_isActive)
		return;

	// don't execute if we've got an item on the cursor.
	if (GetCharInfo2()->pInventoryArray && GetCharInfo2()->pInventoryArray->Inventory.Cursor)
		return;

	clock::time_point now = clock::now();

	// only execute every 500 milliseconds
	if (now < m_lastClick + std::chrono::milliseconds(500))
		return;
	m_lastClick = now;

	if (m_pEndingDoor && GetDistance(m_pEndingDoor->X, m_pEndingDoor->Y) < 25)
	{
		ClickDoor(m_pEndingDoor);
	}
	else if (m_pEndingItem && GetDistance(m_pEndingItem->X, m_pEndingItem->Y) < 25)
	{
		ClickGroundItem(m_pEndingItem);
	}
}

void MQ2NavigationPlugin::StuckCheck()
{
	if (m_isPaused)
		return;

	if (!nav::GetSettings().attempt_unstuck)
		return;

	clock::time_point now = clock::now();

	// check every 100 ms
	if (now > m_stuckTimer + std::chrono::milliseconds(100))
	{
		m_stuckTimer = now;
		if (GetCharInfo())
		{
			if (GetCharInfo()->pSpawn->SpeedMultiplier != -10000
				&& FindSpeed(GetCharInfo()->pSpawn)
				&& (GetDistance(m_stuckX, m_stuckY) < FindSpeed(GetCharInfo()->pSpawn) / 600)
				&& !GetCharInfo()->pSpawn->mPlayerPhysicsClient.Levitate
				&& !GetCharInfo()->pSpawn->UnderWater
				&& !GetCharInfo()->Stunned
				&& m_isActive)
			{
				MQ2Globals::ExecuteCmd(iJumpKey, 1, 0);
				MQ2Globals::ExecuteCmd(iJumpKey, 0, 0);
			}

			m_stuckX = GetCharInfo()->pSpawn->X;
			m_stuckY = GetCharInfo()->pSpawn->Y;
		}
	}
}

static glm::vec3 s_lastFace;

void MQ2NavigationPlugin::LookAt(const glm::vec3& pos)
{
	if (m_isPaused)
		return;

	PSPAWNINFO pSpawn = GetCharInfo()->pSpawn;
	
	gFaceAngle = glm::atan(pos.x - pSpawn->X, pos.y - pSpawn->Y) * 256.0f / glm::pi<float>();
	if (gFaceAngle >= 512.0f) gFaceAngle -= 512.0f;
	if (gFaceAngle < 0.0f) gFaceAngle += 512.0f;

	((PSPAWNINFO)pCharSpawn)->Heading = (FLOAT)gFaceAngle;

	// This is a sentinel value telling MQ2 to not adjust the face angle
	gFaceAngle = 10000.0f;

	s_lastFace = pos;

	if (pSpawn->UnderWater == 5 || pSpawn->FeetWet == 5)
	{
		glm::vec3 spawnPos{ pSpawn->X, pSpawn->Y, pSpawn->Z };
		float distance = glm::distance(spawnPos, pos);

		pSpawn->CameraAngle = glm::atan(pos.z - pSpawn->Z, distance) * 256.0f / glm::pi<float>();
	}
	else if (pSpawn->mPlayerPhysicsClient.Levitate == 2)
	{
		if (pos.z < pSpawn->FloorHeight)
			pSpawn->CameraAngle = -45.0f;
		else if (pos.z > pSpawn->Z + 5)
			pSpawn->CameraAngle = 45.0f;
		else
			pSpawn->CameraAngle = 0.0f;
	}
	else
		pSpawn->CameraAngle = 0.0f;

	// this is a sentinel value telling MQ2 to not adjust the look angle
	gLookAngle = 10000.0f;
}

void MQ2NavigationPlugin::AttemptMovement()
{
	if (m_requestStop)
	{
		Stop();
	}

	if (m_isActive)
	{
		clock::time_point now = clock::now();

		if (now - m_pathfindTimer > std::chrono::milliseconds(PATHFINDING_DELAY_MS))
		{
			auto info = m_activePath->GetDestinationInfo();
			if (info->type == DestinationType::Spawn
				&& info->pSpawn != nullptr
				&& info->options.track)
			{
				info->eqDestinationPos = GetSpawnPosition(info->pSpawn);
			}
			
			// update path
			m_activePath->UpdatePath(false, true);
			m_isActive = m_activePath->GetPathSize() > 0;

			m_pathfindTimer = now;
		}
	}

	Get<SwitchHandler>()->SetActive(m_isActive);

	// if no active path, then leave
	if (!m_isActive) return;

	const glm::vec3& dest = m_activePath->GetDestination();
	const auto& destInfo = m_activePath->GetDestinationInfo();

	if (m_activePath->IsAtEnd())
	{
		SPDLOG_INFO("Reached destination at: {}", dest.yxz());

		LookAt(dest);

		if (m_pEndingItem || m_pEndingDoor)
		{
			AttemptClick();
		}

		Stop();
	}
	else if (m_activePath->GetPathSize() > 0)
	{
		if (!m_isPaused)
		{
			TrueMoveOn(GO_FORWARD);
		}

		glm::vec3 nextPosition = m_activePath->GetNextPosition();

		float distanceFromNextPosition = GetDistance(nextPosition.x, nextPosition.z);

		if (distanceFromNextPosition < WAYPOINT_PROGRESSION_DISTANCE)
		{
			m_activePath->Increment();

			if (!m_activePath->IsAtEnd())
			{
				m_activePath->UpdatePath(false, true);
				nextPosition = m_activePath->GetNextPosition();
			}
		}

		if (m_currentWaypoint != nextPosition)
		{
			m_currentWaypoint = nextPosition;
			SPDLOG_DEBUG("Moving towards: {}", nextPosition.xzy());
		}

		glm::vec3 eqPoint(nextPosition.x, nextPosition.z, nextPosition.y);
		LookAt(eqPoint);

		auto& options = m_activePath->GetDestinationInfo()->options;

		if (options.distance > 0)
		{
			glm::vec3 myPos = GetMyPosition();
			glm::vec3 dest = m_activePath->GetDestination();

			float distance2d = glm::distance2(m_activePath->GetDestination(), myPos);

			// check distance component and make sure that the target is visible
			if (distance2d < options.distance * options.distance
				&& (!options.lineOfSight || m_activePath->CanSeeDestination()))
			{
				// TODO: Copied from above; de-duplicate
				SPDLOG_INFO("Reached destination at: {}", dest.yxz());

				LookAt(dest);

				if (m_pEndingItem || m_pEndingDoor)
				{
					AttemptClick();
				}

				Stop();
			}
		}
	}
}

PDOOR ParseDoorTarget(char* buffer, const char* szLine, int& argIndex)
{
	PDOOR pDoor = pDoorTarget;

	// short circuit if the argument is "click"
	GetArg(buffer, szLine, argIndex);

	int len = strlen(buffer);
	if (len == 0 || !_stricmp(buffer, "click"))
		return pDoor;

	// follows similarly to DoorTarget
	if (!ppSwitchMgr || !pSwitchMgr) return pDoor;
	PDOORTABLE pDoorTable = (PDOORTABLE)pSwitchMgr;

	// look up door by id
	if (!_stricmp(buffer, "id"))
	{
		argIndex++;
		GetArg(buffer, szLine, argIndex++);

		// bad param - no id provided
		if (!buffer[0])
			return nullptr;

		int id = atoi(buffer);
		for (int i = 0; i < pDoorTable->NumEntries; i++)
		{
			if (pDoorTable->pDoor[i]->ID == id)
				return pDoorTable->pDoor[i];
		}

		return nullptr;
	}

	// its not id and its not click. Its probably the name of the door!
	// find the closest door that matches. If text is 'nearest' then pick
	// the nearest door.
	float distance = FLT_MAX;
	bool searchAny = !_stricmp(buffer, "nearest");
	int id = -1;
	argIndex++;

	PSPAWNINFO pSpawn = GetCharInfo()->pSpawn;

	for (int i = 0; i < pDoorTable->NumEntries; i++)
	{
		PDOOR door = pDoorTable->pDoor[i];
		// check if name matches and if it falls within the zfilter
		if ((searchAny || !_strnicmp(door->Name, buffer, len))
			&& ((gZFilter >= 10000.0f) || ((pDoor->Z <= pSpawn->Z + gZFilter)
				&& (pDoor->Z >= pSpawn->Z - gZFilter))))
		{
			id = door->ID;
			float d = Get3DDistance(pSpawn->X, pSpawn->Y, pSpawn->Z,
				door->X, door->Y, door->Z);
			if (d < distance)
			{
				pDoor = door;
				distance = d;
			}
		}
	}

	return pDoor;
}

std::shared_ptr<DestinationInfo> MQ2NavigationPlugin::ParseDestination(const char* szLine,
	spdlog::level::level_enum logLevel)
{
	ScopedLogLevel logScope{ *m_chatSink, logLevel };

	int idx = 1;
	auto result = ParseDestinationInternal(szLine, idx);

	if (result->valid)
	{
		// specify default
		result->options.logLevel = logLevel;

		//--------------------------------------------------------------------
		// parse options

		ParseOptions(szLine, idx, result->options);
	}

	return result;
}

std::shared_ptr<DestinationInfo> MQ2NavigationPlugin::ParseDestinationInternal(
	const char* szLine, int& idx)
{
	CHAR buffer[MAX_STRING] = { 0 };
	GetArg(buffer, szLine, idx++);

	std::shared_ptr<DestinationInfo> result = std::make_shared<DestinationInfo>();
	result->command = szLine;
	result->options = m_defaultOptions;

	if (!GetCharInfo() || !GetCharInfo()->pSpawn)
	{
		SPDLOG_ERROR("Can only navigate when in game!");
		return result;
	}

	// parse /nav target
	if (!_stricmp(buffer, "target"))
	{
		if (pTarget)
		{
			PSPAWNINFO target = (PSPAWNINFO)pTarget;

			result->pSpawn = target;
			result->eqDestinationPos = GetSpawnPosition(target);
			result->valid = true;
			result->isTarget = true;
			result->type = DestinationType::Spawn;

			SPDLOG_INFO("Navigating to target: {}", target->Name);
		}
		else
		{
			SPDLOG_ERROR("You need a target");
		}

		return result;
	}
	
	// parse /nav id #
	if (!_stricmp(buffer, "id"))
	{
		GetArg(buffer, szLine, idx++);
		DWORD reqId = 0;

		try { reqId = boost::lexical_cast<DWORD>(buffer); }
		catch (const boost::bad_lexical_cast&)
		{
			SPDLOG_ERROR("Bad spawn id!");
			return result;
		}

		PSPAWNINFO target = (PSPAWNINFO)GetSpawnByID(reqId);
		if (!target)
		{
			SPDLOG_ERROR("Could not find spawn matching id {}", reqId);
			return result;
		}

		SPDLOG_INFO("Navigating to spawn: {} ({})", target->Name, target->SpawnID);

		result->eqDestinationPos = GetSpawnPosition(target);
		result->pSpawn = target;
		result->type = DestinationType::Spawn;
		result->valid = true;

		return result;
	}

	// parse /nav spawn <text>
	if (!_stricmp(buffer, "spawn"))
	{
		SEARCHSPAWN sSpawn;
		ClearSearchSpawn(&sSpawn);

		// Find a | starting from the right side. Split the string in two if we find it.
		// We'll send the first half to ParseSearchSpawn.
		PCHAR text = GetNextArg(szLine, idx - 1);

		std::string_view tempView{ text };
		size_t pos = tempView.find_last_of("|");
		if (pos != std::string_view::npos)
		{
			tempView = tempView.substr(0, pos);
		}

		CHAR szSpawnSearch[MAX_STRING];
		strncpy_s(szSpawnSearch, tempView.data(), tempView.length());

		ParseSearchSpawn(szSpawnSearch, &sSpawn);

		if (PSPAWNINFO pSpawn = SearchThroughSpawns(&sSpawn, (PSPAWNINFO)pCharSpawn))
		{
			result->eqDestinationPos = GetSpawnPosition(pSpawn);
			result->pSpawn = pSpawn;
			result->type = DestinationType::Spawn;
			result->valid = true;

			SPDLOG_INFO("Navigating to spawn: {} ({})", pSpawn->Name, pSpawn->SpawnID);
		}
		else
		{
			SPDLOG_ERROR("Could not find spawn matching search '{}'", szLine);
		}

		return result;
	}

	// parse /nav door [click [once]]
	if (!_stricmp(buffer, "door") || !_stricmp(buffer, "item"))
	{
		if (!_stricmp(buffer, "door"))
		{
			PDOOR theDoor = ParseDoorTarget(buffer, szLine, idx);

			if (!theDoor)
			{
				SPDLOG_ERROR("No door found or bad door target!");
				return result;
			}

			result->type = DestinationType::Door;
			result->pDoor = theDoor;
			result->eqDestinationPos = { theDoor->X, theDoor->Y, theDoor->Z };
			result->valid = true;

			SPDLOG_INFO("Navigating to door: {}", theDoor->Name);
		}
		else
		{
			if (!pGroundTarget)
			{
				SPDLOG_ERROR("No ground item target!");
				return result;
			}

			result->type = DestinationType::GroundItem;
			result->pGroundItem = pGroundTarget;
			result->eqDestinationPos = { pGroundTarget->X, pGroundTarget->Y, pGroundTarget->Z };
			result->valid = true;

			SPDLOG_INFO("Navigating to ground item: {}", pGroundTarget->Name);
		}

		// check for click and once
		GetArg(buffer, szLine, idx++);

		if (!_stricmp(buffer, "click"))
		{
			result->clickType = ClickType::Once;
		}

		return result;
	}

	// parse /nav waypoint
	if (!_stricmp(buffer, "waypoint") || !_stricmp(buffer, "wp"))
	{
		GetArg(buffer, szLine, idx++);

		nav::Waypoint wp;
		if (nav::GetWaypoint(buffer, wp))
		{
			result->type = DestinationType::Waypoint;
			result->eqDestinationPos = { wp.location.x, wp.location.y, wp.location.z };
			result->valid = true;
			result->waypoint = wp.name;

			SPDLOG_INFO("Navigating to waypoint: {}", buffer);
		}
		else
		{
			SPDLOG_ERROR("Waypoint '{}' not found!", buffer);
		}

		return result;
	}

	// parse /nav x y z
	if (!_stricmp(buffer, "loc")
		|| !_stricmp(buffer, "locxyz")
		|| !_stricmp(buffer, "locyxz")
		|| !_stricmp(buffer, "locxy")
		|| !_stricmp(buffer, "locyx"))
	{
		glm::vec3 tmpDestination;
		bool noflip = !_stricmp(buffer, "locxyz") || !_stricmp(buffer, "locxy");
		if (!_stricmp(buffer, "locyx") || !_stricmp(buffer, "locxy"))
		{
			PCHARINFO pChar = GetCharInfo();

			// this will get overridden if we provide a height arg
			tmpDestination[2] = pChar->pSpawn->Z;
			result->heightType = HeightType::Nearest;
		}

		int i = 0;
		for (; i < 3; ++i)
		{
			char* item = GetArg(buffer, szLine, idx++);
			if (!item || strlen(item) == 0)
			{
				--idx;
				break;
			}

			try { tmpDestination[i] = boost::lexical_cast<double>(item); }
			catch (const boost::bad_lexical_cast&) {
				--idx;
				break;
			}
		}

		if (i == 3 || (i == 2 && result->heightType == HeightType::Nearest))
		{
			if (result->heightType == HeightType::Nearest)
			{
				SPDLOG_INFO("Navigating to loc: {}, Nearest to {}", tmpDestination.xy(), tmpDestination.z);
			}
			else
			{
				SPDLOG_INFO("Navigating to loc: {}", tmpDestination);
			}

			// swap the x/y coordinates for silly eq coordinate system
			if (!noflip)
			{
				tmpDestination = tmpDestination.yxz();
			}

			result->type = DestinationType::Location;
			result->eqDestinationPos = tmpDestination;
			result->valid = true;
		}
		else
		{
			SPDLOG_ERROR("Invalid location: {}", szLine);
		}

		return result;
	}

	SPDLOG_ERROR("Invalid nav destination: {}", szLine);
	return result;
}

static bool to_bool(std::string str)
{
	std::transform(str.begin(), str.end(), str.begin(), ::tolower);
	if (str == "on") return true;
	if (str == "off") return false;
	std::istringstream is(str);
	bool b;
	is >> std::boolalpha >> b;
	return b;
}

void MQ2NavigationPlugin::ParseOptions(const char* szLine, int idx, NavigationOptions& options)
{
	CHAR buffer[MAX_STRING] = { 0 };

	// If line has a | in it, start parsing options from there.
	std::string tempString;
	std::string_view tempView{ szLine };
	size_t pos = tempView.find_last_of("|");
	if (pos != std::string_view::npos)
	{
		tempString = std::string{ tempView.substr(pos + 1) };
		szLine = tempString.c_str();
		idx = 1;
	}

	GetArg(buffer, szLine, idx++);

	while (strlen(buffer) > 0)
	{
		std::string_view arg{ buffer };

		try
		{
			std::string key, value;

			auto eqPos = arg.find_first_of("=", 0);
			if (eqPos != std::string_view::npos)
			{
				key = arg.substr(0, eqPos);
				value = arg.substr(eqPos + 1);
			}
			else
			{
				key = std::string{ arg };
			}

			std::transform(key.begin(), key.end(), key.begin(), ::tolower);

			if (key == "distance" || key == "dist")
			{
				options.distance = boost::lexical_cast<int>(value);
			}
			else if (key == "los" || key == "lineofsight")
			{
				options.lineOfSight = to_bool(std::string{ value });
			}
			else if (key == "log")
			{
				// this was already parsed previously, but whatever we'll do it again!
				options.logLevel = spdlog::level::from_str(std::string{ value });
			}
			else if (key == "paused")
			{
				options.paused = true;
			}
			else if (key == "notrack")
			{
				options.track = false;
			}
		}
		catch (const std::exception& ex)
		{
			SPDLOG_DEBUG("Failed in ParseOptions: '{}': {}", arg, ex.what());
		}

		GetArg(buffer, szLine, idx++);
	}
}

float MQ2NavigationPlugin::GetNavigationPathLength(const std::shared_ptr<DestinationInfo>& info)
{
	NavigationPath path(info);

	if (path.FindPath())
		return path.GetPathTraversalDistance();

	return -1;
}

float MQ2NavigationPlugin::GetNavigationPathLength(const char* szLine)
{
	float result = -1.f;

	// Find distance to path. Logging is disabled
	auto dest = ParseDestination(szLine, spdlog::level::off);
	if (dest->valid)
	{
		ScopedLogLevel level{ *m_chatSink, dest->options.logLevel };

		result = GetNavigationPathLength(dest);
	}

	return result;
}

bool MQ2NavigationPlugin::CanNavigateToPoint(const char* szLine)
{
	bool result = false;
	auto dest = ParseDestination(szLine, spdlog::level::off);
	if (dest->valid)
	{
		ScopedLogLevel level{ *m_chatSink, dest->options.logLevel };

		NavigationPath path(dest);

		if (path.FindPath())
		{
			result = path.GetPathSize() > 0;
		}
	}

	return result;
}

void MQ2NavigationPlugin::Stop()
{
	if (m_isActive)
	{
		SPDLOG_INFO("Stopping navigation");

		TrueMoveOff(APPLY_TO_ALL);
	}

	ResetPath();
}

void MQ2NavigationPlugin::ResetPath()
{
	if (m_mapLine)
		m_mapLine->SetNavigationPath(nullptr);
	if (m_gameLine)
		m_gameLine->SetNavigationPath(nullptr);

	m_isActive = false;
	m_isPaused = false;
	m_requestStop = false;
	m_chatSink->set_level(spdlog::level::info);

	Get<SwitchHandler>()->SetActive(false);

	m_pEndingDoor = nullptr;
	m_pEndingItem = nullptr;
	m_activePath.reset();
}

#pragma endregion

//----------------------------------------------------------------------------

void RenderNavigationOptions(NavigationOptions& opts)
{
	ImGui::PushID(&opts);

	// log level
	ImGui::Combo("Log level", (int*)& opts.logLevel,
		[](void* data, int idx, const char** out_text) -> bool
	{
		*out_text = spdlog::level::level_string_views[idx].data();
		return true;
	}, nullptr, 7);

	ImGui::InputFloat("Destination distance", &opts.distance);
	ImGui::Checkbox("Destination LoS", &opts.lineOfSight);
	ImGui::Checkbox("Track spawn destination movement", &opts.track);

	ImGui::Checkbox("Path following paused", &opts.paused);

	ImGui::PopID();
}

void MQ2NavigationPlugin::OnUpdateTab(TabPage tabId)
{
	if (tabId == TabPage::Navigation)
	{
		ImGui::TextColored(ImColor(255, 255, 0), "Type /nav ui to toggle this window");

		auto navmeshRenderer = Get<NavMeshRenderer>();
		navmeshRenderer->OnUpdateUI();

		// Render default options
		if (ImGui::CollapsingHeader("Navigation Options (Defaults)"))
		{
			RenderNavigationOptions(m_defaultOptions);
		}

		// myPos = EQ coordinate
		glm::vec3 myPos = GetMyPosition();

		{
			// print /LOC coordinate (flipped yx)
			glm::vec3 locMyPos = EQtoLOC(myPos);
			ImGui::Text("Current Position:"); ImGui::SameLine();
			{
				//ImGui::PushFont(ImGuiEx::ConsoleFont);
				ImGui::Text("%.2f %.2f %.2f", locMyPos.x, locMyPos.y, locMyPos.z);
				//ImGui::PopFont();
			}
		}

		ImGui::NewLine();

		ImGui::PushFont(ImGuiEx::LargeTextFont);
		ImGui::TextColored(ImColor(29, 171, 255), "Navigation State");
		ImGui::PopFont();
		ImGui::Separator();

		if (m_isActive)
		{
			auto info = m_activePath->GetDestinationInfo();

			ImGui::TextColored(ImColor(255, 255, 0), "Active:");
			ImGui::SameLine();
			//ImGui::PushFont(ImGuiEx::ConsoleFont);
			ImGui::TextColored(ImColor(255, 255, 0), ("/nav " + info->command).c_str());
			//ImGui::PopFont();

			switch (info->type)
			{
			case DestinationType::Waypoint:
				ImGui::Text("Navigating to waypoint:"); ImGui::SameLine();
				ImGui::TextColored(ImColor(0, 255, 0), info->waypoint.c_str());
				break;

			case DestinationType::Spawn:
				if (info->isTarget)
					ImGui::Text("Navigating to target:");
				else
					ImGui::Text("Navigating to spawn:");
				ImGui::SameLine();

				if (info->pSpawn)
					ImGui::TextColored(ImColor(0, 255, 0), info->pSpawn->Name);
				else
					ImGui::TextColored(ImColor(255, 0, 0), "*despawned*");

				break;

			case DestinationType::Door:
				ImGui::Text("Navigating to object:"); ImGui::SameLine();
				ImGui::TextColored(ImColor(0, 255, 0), info->pDoor->Name);
				break;

			case DestinationType::GroundItem:
				ImGui::Text("Navigating to object:"); ImGui::SameLine();
				ImGui::TextColored(ImColor(0, 255, 0), info->pGroundItem->Name);
				break;

			case DestinationType::Location:
			default:
				ImGui::Text("Navigating to location:"); ImGui::SameLine();
				{
					//ImGui::PushFont(ImGuiEx::ConsoleFont);
					auto& pos = info->eqDestinationPos;
					ImGui::TextColored(ImColor(0, 255, 0), "%.2f %.2f %.2f", pos.x, pos.y, pos.z);
					//ImGui::PopFont();
				}
				break;
			}

			float traversalDist = m_activePath->GetPathTraversalDistance();
			ImGui::Text("Distance to target: %.2f", traversalDist);

			if (ImGuiEx::ColoredButton("Stop Navigation", ImVec2(0, 0), 0.0))
			{
				m_requestStop = true;
			}
			ImGui::SameLine();

			if (ImGui::Checkbox("Paused", &m_isPaused))
			{
				if (m_isPaused)
				{
					TrueMoveOff(APPLY_TO_ALL);
				}
			}

			ImGui::NewLine();

			// target = EQ coordinate
			const glm::vec3& eqDestinationPos = info->eqDestinationPos;

			{
				// print /LOC coordinate (flipped yx)
				glm::vec3 locMyPos = EQtoLOC(eqDestinationPos);
				ImGui::Text("Target position:"); ImGui::SameLine();
				{
					//ImGui::PushFont(ImGuiEx::ConsoleFont);
					ImGui::Text("%.2f %.2f %.2f", locMyPos.x, locMyPos.y, locMyPos.z);
					//ImGui::PopFont();
				}
			}

			float distance = glm::distance(myPos, eqDestinationPos);
			ImGui::Text("Straight distance: %.2f", distance);

			ImGui::NewLine();

			if (ImGui::CollapsingHeader("Navigation Options (Active Path)"))
			{
				ImGui::TextColored(ImColor(29, 171, 255), "Navigation Options");
				ImGui::Separator();

				RenderNavigationOptions(info->options);
			}

			ImGui::NewLine();
			RenderPathList();
		}
		else
		{
			ImGui::TextColored(ImColor(127, 127, 127), "No active navigation");
		}
	}
}

void MQ2NavigationPlugin::RenderPathList()
{
	int count = (m_activePath ? m_activePath->GetPathSize() : 0);
	ImGui::TextColored(ImColor(29, 171, 255), "Path Nodes (%d)", count);

	// Show minimum of 3, max of ~100px worth of lines.
	float spacing = ImGui::GetTextLineHeightWithSpacing() + 2;

	ImGui::BeginChild("PathNodes", ImVec2(0,
		spacing * std::clamp<float>(count, 3, 6)), true);
	ImGui::Columns(3);
	ImGui::SetColumnWidth(0, 30);
	ImGui::SetColumnWidth(1, 30);

	for (int i = 0; i < count; ++i)
	{
		auto node = m_activePath->GetNode(i);
		auto pos = toEQ(std::get<glm::vec3>(node));
		auto flags = std::get<uint8_t>(node);

		bool isCurrent = i == m_activePath->GetPathIndex();
		bool isStart = flags & DT_STRAIGHTPATH_START;
		bool isLink = flags & DT_STRAIGHTPATH_OFFMESH_CONNECTION;
		bool isEnd = flags & DT_STRAIGHTPATH_END;

		if (isCurrent)
		{
			// Green right arrow for current node
			ImGui::TextColored(ImColor(0, 255, 0), ICON_FA_ARROW_CIRCLE_RIGHT);
		}

		ImGui::NextColumn();

		ImColor color(255, 255, 255);

		if (isLink)
		{
			color = ImColor(66, 150, 250);
			ImGui::TextColored(color, ICON_FA_LINK);
		}
		else if (isStart)
		{
			ImGui::TextColored(ImColor(255, 255, 0), ICON_FA_CIRCLE_O);
		}
		else if (isEnd)
		{
			ImGui::TextColored(ImColor(255, 0, 0), ICON_FA_MAP_MARKER);
		}
		else
		{
			ImGui::TextColored(ImColor(127, 127, 127), ICON_FA_ELLIPSIS_V);
		}

		ImGui::NextColumn();
		ImGui::PushFont(ImGuiEx::ConsoleFont);

		ImGui::TextColored(color, "%04d: %.2f, %.2f, %.2f", i,
			pos[0], pos[1], pos[2]);

		ImGui::PopFont();
		ImGui::NextColumn();

	}

	ImGui::Columns(2);
	ImGui::EndChild();
}

//----------------------------------------------------------------------------

NavigationMapLine::NavigationMapLine()
{
	m_enabled = nav::GetSettings().map_line_enabled;
	m_mapLine.SetColor(nav::GetSettings().map_line_color);
	m_mapLine.SetLayer(nav::GetSettings().map_line_layer);
	m_mapCircle.SetColor(nav::GetSettings().map_line_color);
	m_mapCircle.SetLayer(nav::GetSettings().map_line_layer);
}

void NavigationMapLine::SetEnabled(bool enabled)
{
	if (m_enabled == enabled)
		return;

	m_enabled = enabled;

	if (m_enabled)
	{
		RebuildLine();
	}
	else
	{
		Clear();
	}
}

void NavigationMapLine::SetNavigationPath(NavigationPath* path)
{
	if (m_path == path)
		return;

	m_path = path;
	m_updateConn.Disconnect();
	
	if (m_path)
	{
		m_updateConn = m_path->PathUpdated.Connect([this]() { RebuildLine(); });
		RebuildLine();
	}
	else
	{
		Clear();
	}
}

void NavigationMapLine::Clear()
{
	m_mapLine.Clear();
	m_mapCircle.Clear();
}

uint32_t NavigationMapLine::GetColor() const
{
	return m_mapLine.GetColor();
}

void NavigationMapLine::SetColor(uint32_t color)
{
	m_mapLine.SetColor(color);
	m_mapCircle.SetColor(color);
}

void NavigationMapLine::SetLayer(int layer)
{
	m_mapLine.SetLayer(layer);
	m_mapCircle.SetLayer(layer);
}

int NavigationMapLine::GetLayer() const
{
	return m_mapLine.GetLayer();
}

void NavigationMapLine::RebuildLine()
{
	Clear();

	if (!m_enabled || !m_path)
		return;

	int length = m_path->GetPathSize();
	if (length == 0)
	{
		return;
	}

	for (int i = 0; i < length; ++i)
	{
		glm::vec3 point = m_path->GetPosition(i);
		m_mapLine.AddPoint(point);
	}

	auto& options = m_path->GetDestinationInfo()->options;
	if (options.distance > 0)
	{
		m_mapCircle.SetCircle(m_path->GetDestination(), options.distance);
	}
}

ScopedLogLevel::ScopedLogLevel(spdlog::sinks::sink& s, spdlog::level::level_enum l)
	: sink(s)
	, prev_level(sink.level())
{
	sink.set_level(l);
	held = true;
}

ScopedLogLevel::~ScopedLogLevel()
{
	Release();
}

void ScopedLogLevel::Release()
{
	if (held) {
		sink.set_level(prev_level);
		held = false;
	}
}

//============================================================================
