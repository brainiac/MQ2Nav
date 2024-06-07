//
// MQ2Navigation.cpp
//

#include "pch.h"
#include "MQ2Navigation.h"

#include "common/Logging.h"
#include "common/NavMesh.h"
#include "plugin/KeybindHandler.h"
#include "plugin/ModelLoader.h"
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
#include "imgui/ImGuiUtils.h"
#include "imgui/fonts/IconsFontAwesome.h"

#include <fmt/format.h>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/trigonometric.hpp>
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>
#include <chrono>
#include <set>

using namespace std::chrono_literals;

//============================================================================

RenderHandler* g_renderHandler = nullptr;
ImGuiRenderer* g_imguiRenderer = nullptr;

enum MOVEMENT_DIR
{
	GO_FORWARD = 1,
	GO_BACKWARD,
	GO_LEFT,
	GO_RIGHT,
	APPLY_TO_ALL,
};

void TrueMoveOn(MOVEMENT_DIR ucDirection)
{
	switch (ucDirection)
	{
	case GO_FORWARD:
		pKeypressHandler->CommandState[CMD_AUTORUN] = 0;
		pEverQuestInfo->AutoRun = 0;
		pKeypressHandler->CommandState[CMD_BACK] = 0;
		pEverQuestInfo->keyDown[CMD_BACK] = 0;
		pKeypressHandler->CommandState[CMD_FORWARD] = 1;
		pEverQuestInfo->keyDown[CMD_FORWARD]= 1;
		break;
	case GO_BACKWARD:
		pKeypressHandler->CommandState[CMD_AUTORUN] = 0;
		pEverQuestInfo->AutoRun = 0;
		pKeypressHandler->CommandState[CMD_FORWARD] = 0;
		pEverQuestInfo->keyDown[CMD_FORWARD] = 0;
		pKeypressHandler->CommandState[CMD_BACK] = 1;
		pEverQuestInfo->keyDown[CMD_BACK] = 1;
		break;
	case GO_LEFT:
		pKeypressHandler->CommandState[CMD_AUTORUN] = 0;
		pEverQuestInfo->AutoRun = 0;
		pKeypressHandler->CommandState[CMD_STRAFE_RIGHT] = 0;
		pEverQuestInfo->keyDown[CMD_STRAFE_RIGHT] = 0;
		pKeypressHandler->CommandState[CMD_STRAFE_LEFT] = 1;
		pEverQuestInfo->keyDown[CMD_STRAFE_LEFT] = 1;
		break;
	case GO_RIGHT:
		pKeypressHandler->CommandState[CMD_AUTORUN] = 0;
		pEverQuestInfo->AutoRun = 0;
		pKeypressHandler->CommandState[CMD_STRAFE_LEFT] = 0;
		pEverQuestInfo->keyDown[CMD_STRAFE_LEFT] = 0;
		pKeypressHandler->CommandState[CMD_STRAFE_RIGHT] = 1;
		pEverQuestInfo->keyDown[CMD_STRAFE_RIGHT] = 1;
		break;
	}
};

void TrueMoveOff(MOVEMENT_DIR ucDirection)
{
	switch (ucDirection)
	{
	case APPLY_TO_ALL:
		pKeypressHandler->CommandState[CMD_AUTORUN] = 0;
		pEverQuestInfo->AutoRun = 0;
		pKeypressHandler->CommandState[CMD_STRAFE_LEFT] = 0;
		pEverQuestInfo->keyDown[CMD_STRAFE_LEFT] = 0;
		pKeypressHandler->CommandState[CMD_STRAFE_RIGHT] = 0;
		pEverQuestInfo->keyDown[CMD_STRAFE_RIGHT] = 0;
		pKeypressHandler->CommandState[CMD_FORWARD] = 0;
		pEverQuestInfo->keyDown[CMD_FORWARD] = 0;
		pKeypressHandler->CommandState[CMD_BACK] = 0;
		pEverQuestInfo->keyDown[CMD_BACK] = 0;
		break;
	case GO_FORWARD:
		pKeypressHandler->CommandState[CMD_AUTORUN] = 0;
		pEverQuestInfo->AutoRun = 0;
		pKeypressHandler->CommandState[CMD_FORWARD] = 0;
		pEverQuestInfo->keyDown[CMD_FORWARD] = 0;
		break;
	case GO_BACKWARD:
		pKeypressHandler->CommandState[CMD_AUTORUN] = 0;
		pEverQuestInfo->AutoRun = 0;
		pKeypressHandler->CommandState[CMD_BACK] = 0;
		pEverQuestInfo->keyDown[CMD_BACK] = 0;
		break;
	case GO_LEFT:
		pKeypressHandler->CommandState[CMD_AUTORUN] = 0;
		pEverQuestInfo->AutoRun = 0;
		pKeypressHandler->CommandState[CMD_STRAFE_LEFT] = 0;
		pEverQuestInfo->keyDown[CMD_STRAFE_LEFT] = 0;
		break;
	case GO_RIGHT:
		pKeypressHandler->CommandState[CMD_AUTORUN] = 0;
		pEverQuestInfo->AutoRun = 0;
		pKeypressHandler->CommandState[CMD_STRAFE_RIGHT] = 0;
		pEverQuestInfo->keyDown[CMD_STRAFE_RIGHT] = 0;
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

void ClickDoor(EQSwitch* pSwitch)
{
	CVector3 click;
	click.Y = pSwitch->X;
	click.X = pSwitch->Y;
	click.Z = pSwitch->Z;

	ClickSwitch(click, pSwitch);
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
	if (PSPAWNINFO pSpawn = (PSPAWNINFO)pLocalPlayer)
	{
		return GetSpawnPosition(pSpawn);
	}

	return {};
}

float GetMyVelocity()
{
	if (PSPAWNINFO pSpawn = (PSPAWNINFO)pLocalPlayer)
	{
		glm::vec3 veloXYZ = {
			pSpawn->SpeedX,
			pSpawn->SpeedY,
			pSpawn->SpeedZ
		};

		return glm::length(veloXYZ) * 20.0f;
	}

	return 0.0f;
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
			fmt::format_to(fmt::appender(formatted), PLUGIN_MSG_LOG("\ar") "{}", msg.payload);
			break;

		case level::trace:
		case level::debug:
			fmt::format_to(fmt::appender(formatted), PLUGIN_MSG_LOG("\a#7f7f7f") "{}", msg.payload);
			break;

		case level::warn:
			fmt::format_to(fmt::appender(formatted), PLUGIN_MSG_LOG("\ay") "{}", msg.payload);
			break;

		case level::info:
		default:
			fmt::format_to(fmt::appender(formatted), PLUGIN_MSG_LOG("\ag") "{}", msg.payload);
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

class NavAPIImpl : public nav::NavAPI
{
public:
	NavAPIImpl(MQ2NavigationPlugin* plugin)
		: m_plugin(plugin)
	{
	}

	virtual ~NavAPIImpl()
	{
	}

	virtual bool IsInitialized() override
	{
		return m_plugin->IsInitialized();
	}

	virtual bool IsNavMeshLoaded() override
	{
		return m_plugin->IsMeshLoaded();
	}

	virtual bool IsNavPathActive() override
	{
		return m_plugin->IsActive();
	}

	virtual bool IsNavPathPaused() override
	{
		return m_plugin->IsPaused();
	}

	virtual bool IsDestinationReachable(std::string_view destination) override
	{
		return m_plugin->IsInitialized() && m_plugin->CanNavigateToPoint(destination);
	}

	virtual float GetPathLength(std::string_view destination) override
	{
		if (m_plugin->IsInitialized())
			return m_plugin->GetNavigationPathLength(destination);
		return -1.0f;
	}

	virtual bool ExecuteNavCommand(std::string_view command) override
	{
		if (m_plugin->IsInitialized())
		{
			m_plugin->Command_Navigate(command);
			return true;
		}

		return false;
	}

	virtual const nav::NavCommandState* GetNavCommandState() override
	{
		if (m_plugin->IsInitialized())
			return m_plugin->GetCurrentCommandState();

		return nullptr;
	}

	virtual int RegisterNavObserver(nav::fObserverCallback callback, void* userData) override
	{
		int observerId = m_nextObserverId++;

		ObserverRecord record;
		record.callback = callback;
		record.userData = userData;
		record.observerId = observerId;
		observerRecords.push_back(record);

		return observerId;
	}

	virtual void UnregisterNavObserver(int observerId) override
	{
		observerRecords.erase(
			std::remove_if(
				observerRecords.begin(), observerRecords.end(),
				[observerId](const ObserverRecord& rec) { return rec.observerId == observerId; }),
			observerRecords.end());
	}

	//============================================================================

	void DispatchObserverEvent(nav::NavObserverEvent event, nav::NavCommandState* state)
	{
		for (const ObserverRecord& record : observerRecords)
		{
			record.callback(event, *state, record.userData);
		}
	}

private:
	MQ2NavigationPlugin* m_plugin;

	int m_nextObserverId = 1;

	struct ObserverRecord
	{
		nav::fObserverCallback callback;
		void* userData;
		int observerId;
	};
	std::vector<ObserverRecord> observerRecords;
};

NavAPIImpl* s_navAPIImpl = nullptr;

nav::NavAPI* GetNavAPI()
{
	return s_navAPIImpl;
}

void UpdateCommandState(nav::NavCommandState* state, const DestinationInfo& destinationInfo)
{
	state->command = destinationInfo.command;
	state->destination = destinationInfo.eqDestinationPos;
	state->paused = false;
	state->type = destinationInfo.type;
	state->options = destinationInfo.options;
	state->tag = destinationInfo.tag;
}

//============================================================================

#pragma region MQ2Navigation Plugin Class
MQ2NavigationPlugin::MQ2NavigationPlugin()
{
	s_navAPIImpl = new NavAPIImpl(this);
	m_currentCommandState = std::make_unique<nav::NavCommandState>();

	std::string logFile = std::string(gPathLogs) + "\\MQ2Nav.log";

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
	delete s_navAPIImpl;
	s_navAPIImpl = nullptr;
}

void MQ2NavigationPlugin::Plugin_OnPulse()
{
	if (!m_initialized)
	{
		return;
	}

	for (const auto& m : m_modules)
	{
		m.second->OnPulse();
	}

	if (m_initialized && nav::ValidIngame(true))
	{
		AttemptMovement();
		StuckCheck();
	}
}

void MQ2NavigationPlugin::Plugin_OnBeginZone()
{
	if (!m_initialized)
		return;

	// stop active path if one exists
	ResetPath();

	UpdateCurrentZone();

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

	pSwitchTarget = nullptr;
	mq::ClearGroundSpawn();
}

void MQ2NavigationPlugin::Plugin_SetGameState(DWORD GameState)
{
	if (!m_initialized)
	{
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
	if (m_endingGround == pGroundItem)
	{
		m_endingGround.Reset();

		if (m_isActive)
		{
			auto info = m_activePath->GetDestinationInfo();

			if (info->groundItem == pGroundItem)
			{
				info->groundItem = {};
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

void MQ2NavigationPlugin::Plugin_OnUpdateImGui()
{
	if (!m_initialized)
		return;

	if (auto ui = Get<UiController>())
	{
		ui->PerformUpdateUI();
	}
}

void MQ2NavigationPlugin::Plugin_Initialize()
{
	if (m_initialized)
		return;

	nav::LoadSettings();

	g_renderHandler = new RenderHandler();

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

	AddSettingsPanel("plugins/Nav", MQCallback_NavSettingsPanel);

	InitializeMQ2NavMacroData();

	auto ui = Get<UiController>();
	m_updateTabConn = ui->OnTabUpdate.Connect([=](TabPage page) { OnUpdateTab(page); });

	m_mapLine = std::make_shared<NavigationMapLine>();
	m_gameLine = std::make_shared<NavigationLine>();
	g_renderHandler->AddRenderable(m_gameLine.get());

	MQRenderCallbacks callbacks;
	callbacks.CreateDeviceObjects = MQCallback_CreateDeviceObjects;
	callbacks.InvalidateDeviceObjects = MQCallback_InvalidateDeviceObjects;
	callbacks.GraphicsSceneRender = MQCallback_GraphicsSceneRender;
	m_renderCallbacks = AddRenderCallbacks(callbacks);
}

void MQ2NavigationPlugin::MQCallback_CreateDeviceObjects()
{
	if (g_renderHandler)
	{
		g_renderHandler->CreateDeviceObjects();
	}
}

void MQ2NavigationPlugin::MQCallback_InvalidateDeviceObjects()
{
	if (g_renderHandler)
	{
		g_renderHandler->InvalidateDeviceObjects();
	}
}

void MQ2NavigationPlugin::MQCallback_GraphicsSceneRender()
{
	if (g_renderHandler)
	{
		g_renderHandler->PerformRender();
	}
}

void MQ2NavigationPlugin::MQCallback_NavSettingsPanel()
{
	if (g_mq2Nav)
	{
		g_mq2Nav->DrawNavSettingsPanel();
	}
}

void MQ2NavigationPlugin::DrawNavSettingsPanel()
{
	if (auto ui = Get<UiController>())
	{
		ui->DrawNavSettingsPanel();
	}
}

void MQ2NavigationPlugin::Plugin_Shutdown()
{
	if (!m_initialized)
		return;

	RemoveRenderCallbacks(m_renderCallbacks);
	RemoveSettingsPanel("plugins/Nav");
	
	RemoveCommand("/navigate");
	ShutdownMQ2NavMacroData();

	g_renderHandler->RemoveRenderable(m_gameLine.get());

	Stop(false);

	// shut down all of the modules
	for (const auto& m : m_modules)
	{
		m.second->Shutdown();
	}

	// delete all of the modules
	m_modules.clear();

	g_renderHandler->Shutdown();
	delete g_renderHandler;
	g_renderHandler = nullptr;

	m_initialized = false;
	spdlog::shutdown();
}

std::string MQ2NavigationPlugin::GetDataDirectory() const
{
	// the root path is where we look for all of our mesh files
	return std::string(gPathResources) + "\\MQ2Nav";
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

static void DoHelp()
{
	WriteChatf(PLUGIN_MSG "Usage:");
	WriteChatf(PLUGIN_MSG "\ag/nav [save | load]\ax - save/load settings");
	WriteChatf(PLUGIN_MSG "\ag/nav reload\ax - reload navmesh");
	WriteChatf(PLUGIN_MSG "\ag/nav ui\ax - show ui");
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
	WriteChatf(PLUGIN_MSG "\ag/nav ini <key> <value>\ax - Write a setting to the ini and reload settings");
	WriteChatf(PLUGIN_MSG "\aoNavigation Options:\ax");
	WriteChatf(PLUGIN_MSG "Options can be provided to navigation commands to alter their behavior.");
	WriteChatf(PLUGIN_MSG "  \aydistance=<num>\ax - set the distance to navigate from the destination. shortcut: \agdist\ax");
	WriteChatf(PLUGIN_MSG "  \aylineofsight=<on/off>\ax - when using distance, require visibility of target. Default is \ayon\ax. shortcut: \aglos\ax");
	WriteChatf(PLUGIN_MSG "  \aylog=<level>\ax - adjust log level for command. can be \aytrace\ax, \aydebug\ax, \ayinfo\ax, \aywarning\ax, \ayerror\ax, \aycritical\ax or \ayoff\ax. The default is \ayinfo.");
	WriteChatf(PLUGIN_MSG "  \aypaused\ax - start navigation in a paused state.");
	WriteChatf(PLUGIN_MSG "  \aynotrack\ax - disable tracking of spawn movement. By default, when navigating to a spawn, the destination location will track the spawn's position. This disables that behavior.");
	WriteChatf(PLUGIN_MSG "  \ayfacing=<forward|backward>\ax - face forward or backward while moving.");

	// NYI:
	//WriteChatf(PLUGIN_MSG "\ag/nav options set \ay[options]\ax - save options as defaults");
	//WriteChatf(PLUGIN_MSG "\ag/nav options reset\ax - reset options to default");
}

void MQ2NavigationPlugin::Command_Navigate(std::string_view line)
{
	char mutableLine[MAX_STRING] = { 0 };
	strncpy_s(mutableLine, line.data(), line.length());
	CHAR buffer[MAX_STRING] = { 0 };
	int i = 0;

	SPDLOG_DEBUG("Handling Command: {}", line);

	spdlog::level::level_enum requestedLevel = ExtractLogLevel(line, m_defaultOptions.logLevel);
	if (requestedLevel != m_defaultOptions.logLevel)
		SPDLOG_DEBUG("Log level temporarily set to: {}", spdlog::level::to_string_view(requestedLevel));

	ScopedLogLevel scopedLevel{ *m_chatSink, requestedLevel };

	GetArg(buffer, mutableLine, 1);

	// parse /nav ui
	if (!_stricmp(buffer, "ui"))
	{
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

		SetPaused(!m_isPaused);
		return;
	}

	// parse /nav stop
	if (!_stricmp(buffer, "stop"))
	{
		if (m_isActive)
		{
			Stop(false);
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
		GetArg(buffer, mutableLine, 2);
		std::string waypointName(buffer);
		GetArg(buffer, mutableLine, 3);
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

	// parse /nav ini
	if (!_stricmp(buffer, "ini"))
	{
		const char* pStr = GetNextArg(mutableLine, 1);

		if (!nav::ParseIniCommand(pStr))
		{
			SPDLOG_ERROR("Usage: /nav ini <key> <value>");
		}
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

		GetArg(buffer, mutableLine, idx);

		if (!_stricmp(buffer, "reset"))
		{
			// reset
			m_defaultOptions = NavigationOptions{};

			SPDLOG_INFO("Navigation options reset");
		}
		else
		{
			ParseOptions(mutableLine, idx, m_defaultOptions, nullptr);

			SPDLOG_INFO("Navigation options updated");
			SPDLOG_DEBUG("Navigation options updated: {}", mutableLine);
		}

		return;
	}

	scopedLevel.Release();

	// all thats left is a navigation command. leave if it isn't a valid one.
	auto destination = ParseDestination(mutableLine, requestedLevel);
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
		m_pEndingSwitch = destInfo->pSwitch;
		m_endingGround = destInfo->groundItem;
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
	else if (m_activePath->IsFailed())
	{
		UpdateCommandState(m_currentCommandState.get(), *m_activePath->GetDestinationInfo());
		s_navAPIImpl->DispatchObserverEvent(nav::NavObserverEvent::NavFailed, m_currentCommandState.get());
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

		UpdateCommandState(m_currentCommandState.get(), *m_activePath->GetDestinationInfo());
		m_currentCommandState->paused = m_isPaused;

		s_navAPIImpl->DispatchObserverEvent(nav::NavObserverEvent::NavStarted, m_currentCommandState.get());
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
			Stop(false);
		}
		else if (nav::GetSettings().autopause)
		{
			SetPaused(true);
		}
	}
}

//============================================================================

void MQ2NavigationPlugin::AttemptClick()
{
	if (!m_isActive)
		return;

	// don't execute if we've got an item on the cursor.
	if (GetPcProfile()->GetInventorySlot(InvSlot_Cursor))
		return;

	clock::time_point now = clock::now();

	// only execute every 500 milliseconds
	if (now < m_lastClick + std::chrono::milliseconds(500))
		return;
	m_lastClick = now;

	if (m_pEndingSwitch && GetDistance(m_pEndingSwitch->X, m_pEndingSwitch->Y) < 25)
	{
		ClickDoor(m_pEndingSwitch);
	}
	else if (m_endingGround && m_endingGround.Distance(pControlledPlayer) < 25)
	{
		mq::ClickMouseItem(m_endingGround, true);
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
				ExecuteCmd(CMD_JUMP, 1, 0);
				ExecuteCmd(CMD_JUMP, 0, 0);
			}

			m_stuckX = GetCharInfo()->pSpawn->X;
			m_stuckY = GetCharInfo()->pSpawn->Y;
		}
	}
}

static glm::vec3 s_lastFace;

void MQ2NavigationPlugin::Look(const glm::vec3& pos, FacingType facing)
{
	if (m_isPaused)
		return;

	PSPAWNINFO pSpawn = GetCharInfo()->pSpawn;

	gFaceAngle = glm::atan(pos.x - pSpawn->X, pos.y - pSpawn->Y) * 256.0f / glm::pi<float>();
	if (facing == FacingType::Backward) gFaceAngle += 256;
	if (gFaceAngle >= 512.0f) gFaceAngle -= 512.0f;
	if (gFaceAngle < 0.0f) gFaceAngle += 512.0f;

	pControlledPlayer->Heading = (float)gFaceAngle;

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

	if (facing == FacingType::Backward)
		pSpawn->CameraAngle = -pSpawn->CameraAngle;

	// this is a sentinel value telling MQ2 to not adjust the look angle
	gLookAngle = 10000.0f;
}

void MQ2NavigationPlugin::PressMovementKey(FacingType facing)
{
	TrueMoveOn(facing == FacingType::Forward ? GO_FORWARD : GO_BACKWARD);
}

void MQ2NavigationPlugin::MovementFinished(const glm::vec3& dest, FacingType facing)
{
	SPDLOG_INFO("Reached destination at: {}", dest.yxz());

	if (m_endingGround || m_pEndingSwitch)
	{
		Look(dest, FacingType::Forward);
		AttemptClick();
	}
	else
	{
		Look(dest, facing);
	}

	Stop(true);
}

void MQ2NavigationPlugin::AttemptMovement()
{
	if (m_requestStop)
	{
		Stop(false);
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
				m_currentCommandState->destination = info->eqDestinationPos;
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
	auto& options = m_activePath->GetDestinationInfo()->options;

	if (m_activePath->IsAtEnd())
	{
		MovementFinished(dest, options.facing);
	}
	else if (m_activePath->GetPathSize() > 0)
	{
		if (!m_isPaused)
		{
			PressMovementKey(options.facing);
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
		Look(eqPoint, options.facing);


		if (options.distance > 0)
		{
			glm::vec3 myPos = GetMyPosition();
			glm::vec3 dest = m_activePath->GetDestination();

			float distance2d = glm::distance2(m_activePath->GetDestination(), myPos);

			// check distance component and make sure that the target is visible
			if (distance2d < options.distance * options.distance
				&& (!options.lineOfSight || m_activePath->CanSeeDestination()))
			{
				MovementFinished(dest,options.facing);
			}
		}
	}
}

EQSwitch* ParseDoorTarget(char* buffer, const char* szLine, int& argIndex)
{
	char mutableLine[MAX_STRING] = { 0 };
	strcpy_s(mutableLine, szLine);
	EQSwitch* pSwitch = pSwitchTarget;

	// short circuit if the argument is "click"
	GetArg(buffer, mutableLine, argIndex);

	int len = (int)strlen(buffer);
	if (len == 0 || !_stricmp(buffer, "click"))
		return pSwitch;

	// follows similarly to DoorTarget
	if (!pSwitchMgr) return pSwitch;

	// look up door by id
	if (!_stricmp(buffer, "id"))
	{
		argIndex++;
		GetArg(buffer, mutableLine, argIndex++);

		// bad param - no id provided
		if (!buffer[0])
			return nullptr;

		int id = atoi(buffer);

		return pSwitchMgr->GetSwitchById(id);
	}

	// its not id and its not click. Its probably the name of the door!
	// find the closest door that matches. If text is 'nearest' then pick
	// the nearest door.
	float distance = FLT_MAX;
	bool searchAny = !_stricmp(buffer, "nearest");
	int id = -1;
	argIndex++;

	PlayerClient* pSpawn = pLocalPlayer;

	for (int i = 0; i < pSwitchMgr->NumEntries; i++)
	{
		EQSwitch* theSwitch = pSwitchMgr->GetSwitch(i);

		// check if name matches and if it falls within the zfilter
		if ((searchAny || !_strnicmp(theSwitch->Name, buffer, len))
			&& ((gZFilter >= 10000.0f) || ((pSwitch->Z <= pSpawn->Z + gZFilter)
				&& (pSwitch->Z >= pSpawn->Z - gZFilter))))
		{
			id = theSwitch->ID;
			float d = Get3DDistance(pSpawn->X, pSpawn->Y, pSpawn->Z,
				theSwitch->X, theSwitch->Y, theSwitch->Z);
			if (d < distance)
			{
				pSwitch = theSwitch;
				distance = d;
			}
		}
	}

	return pSwitch;
}

std::shared_ptr<DestinationInfo> MQ2NavigationPlugin::ParseDestination(std::string_view line,
	spdlog::level::level_enum logLevel)
{
	ScopedLogLevel logScope{ *m_chatSink, logLevel };

	int idx = 1;
	auto result = ParseDestinationInternal(line, idx);

	if (result->valid)
	{
		// specify default
		result->options.logLevel = logLevel;

		//--------------------------------------------------------------------
		// parse options

		NavigationArguments args;

		ParseOptions(line, idx, result->options, &args);

		result->tag = std::move(args.tag);
	}

	return result;
}

std::shared_ptr<DestinationInfo> MQ2NavigationPlugin::ParseDestinationInternal(std::string_view line, int& idx)
{
	char mutableLine[MAX_STRING] = { 0 };
	strncpy_s(mutableLine, line.data(), line.size());
	CHAR buffer[MAX_STRING] = { 0 };
	GetArg(buffer, mutableLine, idx++);

	std::shared_ptr<DestinationInfo> result = std::make_shared<DestinationInfo>();
	result->command = std::string(line);
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
		GetArg(buffer, mutableLine, idx++);
		int reqId = GetIntFromString(buffer, 0);

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
		MQSpawnSearch sSpawn;
		ClearSearchSpawn(&sSpawn);

		// Find a | starting from the right side. Split the string in two if we find it.
		// We'll send the first half to ParseSearchSpawn.
		PCHAR text = GetNextArg(mutableLine, idx - 1);

		std::string_view tempView{ text };
		size_t pos = tempView.find_last_of("|");
		if (pos != std::string_view::npos)
		{
			tempView = tempView.substr(0, pos);
		}

		CHAR szSpawnSearch[MAX_STRING];
		strncpy_s(szSpawnSearch, tempView.data(), tempView.length());

		ParseSearchSpawn(szSpawnSearch, &sSpawn);

		if (PSPAWNINFO pSpawn = SearchThroughSpawns(&sSpawn, pControlledPlayer))
		{
			result->eqDestinationPos = GetSpawnPosition(pSpawn);
			result->pSpawn = pSpawn;
			result->type = DestinationType::Spawn;
			result->valid = true;

			SPDLOG_INFO("Navigating to spawn: {} ({})", pSpawn->Name, pSpawn->SpawnID);
		}
		else
		{
			SPDLOG_ERROR("Could not find spawn matching search '{}'", line);
		}

		return result;
	}

	// parse /nav door [click [once]]
	if (!_stricmp(buffer, "door") || !_stricmp(buffer, "item"))
	{
		if (!_stricmp(buffer, "door") || !_stricmp(buffer, "switch"))
		{
			EQSwitch* theSwitch = ParseDoorTarget(buffer, mutableLine, idx);

			if (!theSwitch)
			{
				SPDLOG_ERROR("No switch found or bad switch target!");
				return result;
			}

			result->type = DestinationType::Door;
			result->pSwitch = theSwitch;
			result->eqDestinationPos = { theSwitch->X, theSwitch->Y, theSwitch->Z };
			result->valid = true;

			SPDLOG_INFO("Navigating to switch: {}", theSwitch->Name);
		}
		else
		{
			if (!mq::CurrentGroundSpawn())
			{
				SPDLOG_ERROR("No ground item target!");
				return result;
			}

			result->type = DestinationType::GroundItem;
			result->groundItem = mq::CurrentGroundSpawn();

			CVector3 position = result->groundItem.Position();

			result->eqDestinationPos = { position.X, position.Y, position.Z };
			result->valid = true;

			SPDLOG_INFO("Navigating to ground item: {} ({})", result->groundItem.DisplayName(), result->groundItem.ID());
		}

		// check for click and once
		GetArg(buffer, mutableLine, idx++);

		if (!_stricmp(buffer, "click"))
		{
			result->clickType = ClickType::Once;
		}

		return result;
	}

	// parse /nav waypoint
	if (!_stricmp(buffer, "waypoint") || !_stricmp(buffer, "wp"))
	{
		GetArg(buffer, mutableLine, idx++);

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
			const char* item = GetArg(buffer, mutableLine, idx++);
			if (!item || strlen(item) == 0)
			{
				--idx;
				break;
			}

			std::string_view sv{ item };
			auto result = std::from_chars(sv.data(), sv.data() + sv.size(), tmpDestination[i]);

			if (result.ec != std::errc{}) {
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
			SPDLOG_ERROR("Invalid location: {}", line);
		}

		return result;
	}

	SPDLOG_ERROR("Invalid nav destination: {}", line);
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

void MQ2NavigationPlugin::ParseOptions(std::string_view line, int idx, NavigationOptions& options, NavigationArguments* args)
{
	char mutableLine[MAX_STRING] = { 0 };
	strncpy_s(mutableLine, line.data(), line.length());
	CHAR buffer[MAX_STRING] = { 0 };

	// If line has a | in it, start parsing options from there.
	std::string tempString;
	size_t pos = line.find_last_of("|");
	if (pos != std::string_view::npos)
	{
		std::string_view temp = line.substr(pos + 1);
		strncpy_s(mutableLine, temp.data(), temp.length());
		idx = 1;
	}

	GetArg(buffer, mutableLine, idx++);

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
				options.distance = GetIntFromString(value, 0);
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
			else if (key == "facing")
			{
				options.facing = FacingType::Forward;
				if (value == "backward")
					options.facing = FacingType::Backward;
				else if (value != "forward")
					SPDLOG_ERROR("invalid argument for facing=<forward|backward");
			}
			else if (args && key == "tag")
			{
				args->tag = value;
			}
			else if (key == "searchradius")
			{
				options.searchRadius = GetIntFromString(value, 0);
			}
			else if (key == "searchorigin")
			{
				std::vector <std::string_view> parts = mq::split_view(value, ' ', false);
				if (parts.size() == 3)
				{
					options.searchOrigin = { GetFloatFromString(parts[0], 0), GetFloatFromString(parts[1], 0), GetFloatFromString(parts[2], 0) };
				}
				else if (parts.size() == 2)
				{
					options.searchOrigin = { GetFloatFromString(parts[0], 0), pLocalPlayer->FloorHeight, GetFloatFromString(parts[2], 0) };
				}
				else
					SPDLOG_ERROR("Invalid argument for searchorigin: {}", value);
			}
		}
		catch (const std::exception& ex)
		{
			SPDLOG_DEBUG("Failed in ParseOptions: '{}': {}", arg, ex.what());
		}

		GetArg(buffer, mutableLine, idx++);
	}
}

float MQ2NavigationPlugin::GetNavigationPathLength(const std::shared_ptr<DestinationInfo>& info)
{
	NavigationPath path(info);

	if (path.FindPath())
		return path.GetPathTraversalDistance();

	return -1;
}

float MQ2NavigationPlugin::GetNavigationPathLength(std::string_view line)
{
	float result = -1.f;

	// Find distance to path. Logging is disabled
	auto dest = ParseDestination(line, spdlog::level::off);
	if (dest->valid)
	{
		ScopedLogLevel level{ *m_chatSink, dest->options.logLevel };

		result = GetNavigationPathLength(dest);
	}

	return result;
}

bool MQ2NavigationPlugin::CanNavigateToPoint(std::string_view line)
{
	bool result = false;
	auto dest = ParseDestination(line, spdlog::level::off);
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

void MQ2NavigationPlugin::Stop(bool reachedDestination)
{
	if (m_isActive)
	{
		if (!reachedDestination)
		{
			SPDLOG_INFO("Stopping navigation");
			s_navAPIImpl->DispatchObserverEvent(nav::NavObserverEvent::NavCanceled, m_currentCommandState.get());
		}
		else
		{
			s_navAPIImpl->DispatchObserverEvent(nav::NavObserverEvent::NavDestinationReached, m_currentCommandState.get());
		}

		TrueMoveOff(APPLY_TO_ALL);
		m_isActive = false;
	}

	ResetPath();
}

void MQ2NavigationPlugin::SetPaused(bool paused)
{
	if (paused == m_isPaused)
		return;
	m_isPaused = paused;
	if (m_isPaused)
	{
		SPDLOG_INFO("Pausing Navigation");
		TrueMoveOff(APPLY_TO_ALL);
	}
	else
	{
		SPDLOG_INFO("Resuming Navigation");
	}

	m_currentCommandState->paused = m_isPaused;
	s_navAPIImpl->DispatchObserverEvent(nav::NavObserverEvent::NavPauseChanged, m_currentCommandState.get());
}

void MQ2NavigationPlugin::ResetPath()
{
	// Double check if we need to send cancel event.
	if (m_activePath && m_isActive)
	{
		s_navAPIImpl->DispatchObserverEvent(nav::NavObserverEvent::NavCanceled, m_currentCommandState.get());
	}

	if (m_mapLine)
		m_mapLine->SetNavigationPath(nullptr);
	if (m_gameLine)
		m_gameLine->SetNavigationPath(nullptr);

	m_isActive = false;
	m_isPaused = false;
	m_requestStop = false;
	m_chatSink->set_level(spdlog::level::info);

	Get<SwitchHandler>()->SetActive(false);

	m_pEndingSwitch = nullptr;
	m_endingGround.Reset();
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

		ImGui::PushFont(mq::imgui::LargeTextFont);
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
				ImGui::TextColored(ImColor(0, 255, 0), info->pSwitch->Name);
				break;

			case DestinationType::GroundItem:
				ImGui::Text("Navigating to object:"); ImGui::SameLine();
				ImGui::TextColored(ImColor(0, 255, 0), "%s (%d)", info->groundItem.DisplayName().c_str(), info->groundItem.ID());
				break;

			case DestinationType::Location:
			default:
				ImGui::Text("Navigating to location:"); ImGui::SameLine();
				{
					//ImGui::PushFont(ImGuiEx::ConsoleFont);
					const auto& pos = info->eqDestinationPos;
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

			bool tempPaused = m_isPaused;
			if (ImGui::Checkbox("Paused", &tempPaused))
			{
				SetPaused(tempPaused);
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
		ImGui::PushFont(mq::imgui::ConsoleFont);

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
