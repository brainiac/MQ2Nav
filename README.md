MacroQuest Navigation Plugin
======

This is a navmesh-powered navigation and pathfinding plugin for [MacroQuest](https://gitlab.com/macroquest/next/mqnext) "Next". Navigation is based on the RecastNavigation library.

This project consists of two parts: The MacroQuest Plugin (MQ2Nav.dll), and the Mesh Generator (MeshGenerator.exe)

The MeshGenerator tool is used for generating and editing the properties of navmeshes

Screenshots
-----------

* See the [wiki](https://github.com/brainiac/MQ2Nav/wiki/Screenshots) for some screenshots and features


Building
--------

##### Requirements

* Visual Studio 2019 (16.10+)

* [MacroQuest](https://gitlab.com/macroquest/next/mqnext) (next)
    * _Note: Support for MacroQuest2 legacy is discontinued and will no longer be receiving updates_

MQ2Nav is a unique plugin. The project files have a lot of parts, so unless you know what you are doing, you usually do not want to add it to your solution. Insteada, you can open up the MQ2Nav.sln file and build it on its own. Make sure that you build the core distribution in MacroQuest.sln first.

1. **Check out the sources into <your macroquest source folder>/plugins/MQ2Nav**. MQ2Nav should be placed in the /plugins folder in the root of the checkout. This folder is dedicated to your plugins.
2. Open MacroQuest.sln and build
3. Open MQ2Nav/MQ2Nav.sln and select the configuration (most people will want Release)
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
* [SDL2](http://libsdl.org/) (for the mesh generator)
* [zlib](http://zlib.net/)


Contributing
------------

Contributions are welcome! Feel free to open pull requests or report issues.


Usage
-----

See the [wiki](https://github.com/brainiac/MQ2Nav/wiki) for more info


Notes
-----

Loading into a zone with MQ2Nav loaded will generate a config file with a list of dynamic objects that can be consumed by MeshGenerator to add that extra geometry to the navmesh. These kinds of objects include some POK stones, for example. Doors are filtered out of the list, but there may be some false positives.
