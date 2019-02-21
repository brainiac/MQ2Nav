MQ2Nav Changelog
================

1.3.0
-----

### Highlights & New Features

**New Off-Mesh Connections tool** (#52, #65)
Using the new MeshGenerator connections tool, create short connections between two points on the mesh. The distance is limited to adjacent tiles. Connections can be one-way or bi-directional, and pathing costs are calculated.

Off-Mesh connections can be used to bridge gaps in the mesh when certain areas are obstructed, allowing MQ2Nav to follow a path that spans a segment that lacks navmesh. While following a path, connections are represented as a purple line segment.

**Navigation Command Options** (#49, #53, #63, #64, 1323b0440700851974db53f2c573fa90b6721304)
Navigation commands can now be modified with options that modify the navigation behavior. For example, you can use the following command to get within 30 distance of the target: `/nav target distance=30`. The following options have been added:

* **distance=x** Move to within the specified distance to the destination. Using this command will enforce a line-of-sight check to ensure that the destination is visible before ending. shortcut: `dist`
* **lineofsight=off** Disables the line-of-sight check when using distance. shortcut: `los`.
* **log=level** Sets log level during navigation. Use `log=off` to completely disable navigation logging while the command is active. Valid options: `trace`, `debug`, `info`, `warning`, `error`, `fatal`, `off`. The default is `info`. Specifying a level limits logging to the given level or higher.
* **paused** Starts navigation in a paused state.
* **notrack** Disables tracking of spawn movement. When navigating to a spawn (via any of the various methods), the destination will be updated to track the spawn's movement. If you prefer not to update the destination when the target moves, specify this option.

You can specify defaults for the session by using the nav ui, or by using **/nav setopt *[options]***

**Navigate from MQ2Map** (#15)
Use MQ2Map click feature to navigate to a specific location using the locxy navigation command.

Activate this feature by setting a MQ2Map click command: `/mapclick left ctrl+shift /nav locxy %x %y` and then ctrl+shift+leftclick anywhere on the map to go there.

**Path Planning Improvements**
Moving off of the navigation mesh will no longer cause the path to fail. Additionally, trying to navigate to unreachable destinations will no longer try to navigate with an incomplete path.

### Enhancements
* Areas marked as unwalkable can now be overwritten by walkable areas. This is useful for blocking out a large area as unwalkable, and then carving an area within it as walkable. Areas are applied in the order specified in the MeshGenerator, so specify your unwalkable areas first and your walkable area after. (#16)
* Areas tool now supports reordering areas. (fab89cae26c3c029189e43cc9309a261227a407c)
* Navigation is no longer affects by keyboard state. It is now safe to type while navigation is active! (#56)
* Added command to list saved waypoints: `/nav listwp` (#40)
* Added `${Nav}` alias for `${Navigation}` (#57)
* Added two new default area types to assist mesh creators: Prefer and Avoid. These areas can be used in association with the areas or connections tools to mark areas of the map as preferred or avoided when creating paths. (57f466397bbe2c1ad9106a022e32af916a3ec131)
* Added `log=off` option to disable logging of navigation commands. (#49)
* Added new, official plugin exports for other plugins to integrate against. Old, unofficial exports still exist, but will print a warning when using them for the first time. See this wiki page for more information: https://github.com/brainiac/MQ2Nav/wiki/MQ2Nav-Exported-Functions (#13)
* Navigating to a spawn will now dynamically update the destination if the spawn moves. add the `notrack` option to disable these updates. (#64)
* Improved the display of instructions in the mesh generator. These helpful little hints are now rendered as an overlay with a grey background to make them more visible in the bottom right corner. If this gets in the way it can be disabled from the menu. (28c899477213b91ba1f4006edb40595158c6ca96)
* Settings used to generate a navmesh are now saved with the mesh. Opening the mesh again will bring back the original settings, allowing to make additional changes without causing problems. (#66)
* Zone shortname can be passed to MeshGenerator.exe via command line. This will also load the navmesh if available. (64818bdd4f1fe1cae8e45bed05dee36323653eb3)
* MQ2Nav and MeshGenerator now use a logging system to provide consistent and uniform logging. In addition to logging to the screen, logs can also be found in MQ2Nav.log or MeshGenerator.log, even if logging is silenced in the UI. (#55)
* Revamped main content area of MQ2Nav UI, giving priority to current navigation state.
* Various other UI improvements made to MeshGenerator and MQ2Nav. Feedback is welcome!
* Added a way for `/nav spawn` to work with options. Separate the spawn search query from the nav options with a | pipe symbol. For example: `/nav spawn npc banker |dist=30` (#68)
* Waypoints names are now case insensitive when used for searching. (#70)
* Added command to change option defaults (#58)
  * `/nav setopt [options]` will set option defaults that other navigation commands will use.
  * `/nav setopt dist=30` will set the default distance to 30
  * `/nav setopt log=off` will turn logging off
  * Option defaults can be reset with `/nav setopt reset`
* Added nav ui to show and modify default options.
* Updated help text for other navigation options.

### Bug Fixes
* Logging out should no longer confuse the current zone id. (#37)
* `${Navigation.PathExists}` should no longer return true if the destination is not reachable. (#48)
* Fixed several crashes including ResetDevice, RenderHandler::InvalidateDeviceObjects, and a few others. (#36, #62 101789b597b1145544a65f8e067176de4b48c0b9, d849894f24476eff343f5d8a35c4ac24d9038398)
* Moving off of the mesh will no longer cause navigation to stop. (1323b0440700851974db53f2c573fa90b6721304)
* Trying to navigate to an unreachable destination (or one that exceeds the memory limit for length) will no longer product an incomplete path. Instead, it will fail. (#41)
* Typing into text fields in the mesh generator will no longer also move the camera (6c6d04d7f9cb1fb47c0c6d1ba00d25b6f7d22734)
* Modifying the x or z coordinates of the bouding box should no longer cause problems with using the mesh. (#32)
* Fixed path length assert failure
* Fixed `/nav spawn` handling, the first search argument should no longer be ignored (1c2de7e3ad07556fd7a5062ce74f9ebcd3d49b0e)
* Fixed keyboard input in overlay ui in MQ2Nav (#71)
* Fixed log level not properly being set during macro data queries (#67)
* Fixed a shutdown crash (a022f861d6bc32d0f8fcdf68e60eeebd0fdfc44f)
* Fixed a crash on plugin load (#69)
* Fixed an issue that caused navigation path indicator to stop appearing after a number of paths. (#74)

### Packaging Notes
VolumeLines.fx is no longer needed to be bundled with the plugin. It can be excluded from redistributions moving forward. (#43)

### Updated Dependencies
* ImGui upgraded to 1.67, comes with various bug fixes and new color scheme. (#60)
* SDL2 upgraded to 2.0.9


1.2.2
-----

Updated latest EQ LIVE (2/21/2018)

**Bug Fixes**
* Don't try to initialize the plugin until we enter the game
* Use actual position and distance when calculating the camera angle when following a path.
* Fixed an issue with the automatic door opener that was causing it to ignore most RoS doors. The restrictions have been relaxed while still trying to avoid switches such as teleporters.

1.2.1
-----

**Improvements - MQ2Nav**
* Updated source to latest version of MQ2 core
* Now built using the latest Windows SDK and Visual Studio 2017.
* Destination zone name on switches (for example, pok stones) is now available to debug tools and will eventually be introduced into other features
* D3DX9_43.dll is now included in the build to avoid requiring DX9 runtime installer.

**Bug Fixes**
* Zones.ini: Add new zones for Ring of Scale expansion
* Fixed a crash when trying to load a mesh that had been created with an incorrect zone name.
* Fixed a typo in log message complaining about not finding a starting point.

1.2.0
-----

**Improvements - MQ2Nav**
* New feature: active navigation path will now be displayed on the map if MQ2Map is loaded.
  * Color, layer and toggle added to MQ2Nav settings.
* New feature: automatic door opening while following path
  * Door opening is only enabled while a path is actively being followed. Check out the "Door Handler Debug" UI to tweak behavior.
  * Current behavior is: click door every 2 seconds for any door with distance < 20. Check the "Door Handler Debug" to find tune the distance.

**Improvements - MeshGenerator**
* New feature: Maximum Zone Extents
  * Some zones have geometry that exists far outside the normal zone area, leading to issues generating the mesh for that zone. Added a new feature that will filter out geometry that exists outside these maximum boundaries on certain zones. See NavMeshData for the list of existing zones that support this feature. 
  * This feature has a checkbox to disable it in settings if you want to be a rebel and see the filtered out geometry.

**Bug Fixes**
* Zones.ini: fix shortname for Chelsith Reborn.
* Waypoints now use FloorHeight when calculating player position (if enabled in settings).


1.1.1
-----

**Bug Fixes**
* Fix crash when loading MQ2Nav while also running InnerSpace
* Fix problem selecting zones that have duplicate names in the zone picker

**Improvements**
* Use FloorHeight instead of Z for calculating spawn positions, should avoid some cases of "unable to find destination" problems.
  * This behavior can be disabled in the settings window of MQ2Nav if it causes problems


1.1.0
-----
*NOTE* This version is not compatible with navmeshes made with previous versions. You will need to rebuild your meshes!

**New Areas Tool**
* MeshGenerator can now define custom area types with cost modifiers. Access it from the Edit menu.
  * Define custom areas types, give them names, colors, and costs.
* Convex Volume tool has been given an overhaul, lets you save the volumes and edit existing volumes.
  * Convex Volumes let you mark parts of the navmesh as different area types.
  * Areas define cost modifiers or can completely exclude areas of the mesh from navigation
  * Navigation will always use the lowest cost path. Mark areas with high cost to avoid them and low cost to prefer them. Default area cost is 1.0

**Navmesh Format Changes**
* Due to many changes to the internal structure of the navmesh, old navmeshes will not load correctly. As a result, the navmesh compat version has been bumped up, so loading older meshes will give you an error.
* Navmesh files now have their own extension: .navmesh
* Navmesh files are now compressed for extra space savings
* All modifications to the navmesh are now stored in the .navmesh file, so reopening a mesh will have all the changes that were made when the navmesh was created.
* Tile size limit has been removed.

**UI Changes**
* New theme applied to the UI in both the plugin and MeshGenerator
* Completely redesigned MeshGenerator UI.
* Adjusted the defaults of a couple navmesh parameters for better path finding behavior:
  * Reduced MaxClimb to 4.0 to avoid getting stuck on the edge of tall objects
  * Increased MaxSlope to 75 to allow traversal over steeper terrain.
* Navmesh ingame path rendering has received a visual upgrade.
* Revamp the ingame Waypoints UI, fixing several bugs
  * Added option to delete and rename waypoints
  * Fixed the order of the x,y,z coordinates in waypoints UI to match /loc
* Improvements to how the in-game active navigation path line is rendered.

**Misc Changes**
* Added feature to export/import settings as json files. This can be accessed through the "mesh" menu. By default these settings files well be put into <MQ2Dir>/MQ2Nav/Settings
* MeshGenerator is now a 64-bit application
* Obligatory mention: fixed lots of bugs.
  * EOK Zones should now load all objects. Please note that performance is going to be pretty terrible, please have patience - i'm still working on improving handling large zones.
  * Fixed issues that would cause your character to get stuck running back and forth when running near edges of the navmesh

**Command Changes**
* Fixed various /nav commands and made many improvements:
  * Removed /nav x y z, replaced with:
    * eq /loc coordinates: /nav loc y x z, /nav locyxz y x z
    * eq xyz coordinates: /nav locxyz x y z
  * Fixed issues with clicking items and doors when using /nav item or /nav door
* Remove spam click behavior of /nav item/door as it didn't work right anyways
  * Added /nav spawn <text> - takes same parameters of Spawn TLO to do a spawn search
* Added new forms of /nav commands. [See the wiki](https://github.com/brainiac/MQ2Nav/wiki/Command-Reference) for more info.



1.0.0
-----

Initial Version of change log, started recording changes after this point!
* Originally numbered 2.0, but retroactively changed to 1.0 to start a new versioning sequence!
