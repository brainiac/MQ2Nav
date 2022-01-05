MQ2Nav
======

[![Build Status](https://jenkins.mmobugs.com/buildStatus/icon?job=MQ2Nav-Release)](https://github.com/brainiac/MQ2Nav)

NavMesh based navigation and pathfinding plugin for [MacroQuest2](http://www.macroquest2.com). Navigation is based on the RecastNavigation library. The plugin also includes an ingame overlay to display navigation meshes as well as a debugging UI provided by [imgui](https://github.com/ocornut/imgui/).

This project consists of two parts: The MQ2 Plugin (MQ2Nav.dll), and the Mesh Generator (MeshGenerator.exe)

The MeshGenerator tool is used for generating and editing the properties of navmeshes

Screenshots
-----------

* See the [wiki](https://github.com/brainiac/MQ2Nav/wiki/Screenshots) for some screenshots and features


Building
--------

##### Requirements

* Visual Studio 2019 (16.10+)

* MacroQuest (next)

This build of MQ2Nav requires the new MacroQuest.

MQ2Nav is a unique plugin. You do not want to add it to your solution. Instead you will build it in a separate solution apart from MacroQuest2.sln.

1. Check out the sources into <your macroquest source folder>/plugins/MQ2Nav. MQ2Nav should be placed in the /plugins folder in the root of the checkout. This folder is dedicated to your plugins.
2. Open MacroQuest.sln and build
3. Open MQ2Nav\MQ2Nav.sln and select the configuration (most people will want Release)
4. **build MQ2Nav.dll:** select the Win32 architecture and build
5. **build MeshGenerator.exe:** select the x64 architecture and build

The first time you build, you'll build the vcpkg dependencies.

### Third Party Libraries

This plugin makes use of the following libraries:

* [EQEmu zone-utilities](https://github.com/EQEmu/zone-utilities)
* [imgui](https://github.com/ocornut/imgui)
* [google protocol buffers](https://github.com/google/protobuf)
* [glm](http://glm.g-truc.net)
* [rapidjson](http://rapidjson.org)
* [RecastNavigation](https://github.com/recastnavigation/recastnavigation)
* [SDL2](http://libsdl.org/)
* [zlib](http://zlib.net/)


Contributing
------------

Contributions are welcome! Feel free to open pull requests or report issues.

**TODO**

Usage
-----

See the [wiki](https://github.com/brainiac/MQ2Nav/wiki) for more info

Notes
-----

Loading into a zone with MQ2Nav loaded will generate a config file with a list of dynamic objects that can be consumed by MeshGenerator to add that extra geometry to the navmesh. These kinds of objects include some POK stones, for example. Doors are filtered out of the list, but there may be some false positives.


TODO List
---------

Some things to do. Eventually move these to issues...

- Split overlay code out into its own plugin
- Better path planning
- Switch to TileCache implementation (for dynamic obstacle support)
- Render paths with MQ2Map
- Improved door opening behavior
- Some tiles generate incorrectly using MeshGenerator (duplicate or missing)
