//
// PluginAPI.h
//

#pragma once

#include <mq/Plugin.h>
#include <glm/vec3.hpp>

// Exports functions that can be used by other plugins to interface with MQ2Nav

#ifdef NAV_API_EXPORTS
#define NAV_API extern "C" __declspec(dllexport)
#define NAV_OBJECT __declspec(dllexport)
#else
#define NAV_API extern "C" __declspec(dllimport)
#define NAV_OBJECT __declspec(dllimport)
#endif

//----------------------------------------------------------------------------

namespace nav {

//----------------------------------------------------------------------------

// defines a type of destination that is being navigated to.
enum class DestinationType
{
	None,
	Location,
	Door,
	GroundItem,
	Spawn,
	Waypoint,
};

// defines the facing mode for the nav command.
enum class FacingType
{
	Forward,
	Backward,
};

// defines the options that can be used to create a navigation command.
struct NavigationOptions
{
	float distance = 0.f;          // distance to target to stop at
	bool lineOfSight = true;       // does target need to be in los
	bool paused = false;           // pathing is paused
	bool track = true;             // if spawn is to be tracked
	FacingType facing = FacingType::Forward; //  Forward = normal, Backward = move along path facing backward.

	// set a new default log level while the path is running. info is the default.
	spdlog::level::level_enum logLevel = spdlog::level::info;
};

// Represents the state of a nav command. This state is only valid during a
// callback from MQ2Nav. Do not store or otherwise make copies of this data
// as the information it points to may go out of scope.

// This state is read-only. Modifying it will have no impact on the navigation
// command that it represents.

struct NavCommandState
{
	// the original command string.
	std::string_view command;

	// the position of the destination.
	glm::vec3 destination;

	// indicates if the command is paused
	bool paused;

	// the destination type
	DestinationType type;

	// options used to create the command.
	NavigationOptions options;

	// The tag value used in the nav command if one was provided
	std::string_view tag;
};

//----------------------------------------------------------------------------

// Observer event - Indicates the type of event that occurred in the observer.
enum class NavObserverEvent
{
	NavStarted,                  // when navigation is started
	NavPauseChanged,             // when navigation is paused or unpaused by the user
	NavCanceled,                 // when navigation is canceled, either by starting a new command or by /nav stop
	NavDestinationReached,       // when navigation reaches its destination.
};

// Observer function - Registering an observer function will allow your plugin to
// get notified of changes in current navigation state.

using fObserverCallback = void(*)(nav::NavObserverEvent eventType, const nav::NavCommandState& commandState, void* userData);

//----------------------------------------------------------------------------

// The main interface to Nav. Acquire it by calling GetNavAPIFromPlugin(). Be sure
// to stop using it if you unload the plugin.
class NavAPI
{
public:
	virtual ~NavAPI() {}

	//----------------------------------------------------------------------------
	// Original Nav api exposed through the NavAPI interface

	// Check if MQ2Nav is initialized
	virtual bool IsInitialized() = 0;

	// Check if a mesh is loaded
	virtual bool IsNavMeshLoaded() = 0;

	// Check if a navigation command is running.
	virtual bool IsNavPathActive() = 0;

	// check if a navigation command is paused.
	virtual bool IsNavPathPaused() = 0;

	// Check if a path is possible to the specified destination
	virtual bool IsDestinationReachable(std::string_view destination) = 0;

	// Calculate the length of the path to the specified destination
	virtual float GetPathLength(std::string_view destination) = 0;

	// Execute a nav command
	virtual bool ExecuteNavCommand(std::string_view command) = 0;

	//----------------------------------------------------------------------------

	// Retrieves current command state. Do not copy this value or store it for later.
	// If no command is active, returns nullptr.
	virtual const NavCommandState* GetNavCommandState() = 0;

	// Register an observer to get notifications of nav command state changes.
	// Returns the observer id. Use this to unregister before unloading your plugin.
	virtual int RegisterNavObserver(fObserverCallback callback, void* userData) = 0;

	// Unregisters an observer that was registered with RegisterNavObserver
	virtual void UnregisterNavObserver(int observerId) = 0;
};

} // namespace nav

// Retrieve the pointer to the nav api. You are free to store this pointer in your plugin.
// If MQ2Nav is unloaded, you need to clear the pointer. Check for plugin being unloaded
// in the OnUnloadPlugin callback.

NAV_API nav::NavAPI* GetNavAPI();

// Convenience function for getting the api from the plugin.
namespace nav {

inline nav::NavAPI* GetNavAPIFromPlugin()
{
	using navAPIptr = nav::NavAPI* (*)();

	navAPIptr navAPIFunc = (navAPIptr)GetPluginProc("MQ2Nav", "GetNavAPI");
	if (navAPIFunc)
	{
		return navAPIFunc();
	}

	return nullptr;
}

} // namespace nav
