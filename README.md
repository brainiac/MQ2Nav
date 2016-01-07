MQ2Nav
======

**This plugin is a work in progress!!**

Navigation Mesh plugin for [MacroQuest2](http://www.macroquest2.com). This is based on the origin MQ2Navigation plugin, but has been heavily rewritten to take advantage of newer libraries that are available. The plugin also includes a 3d overlay to display navigation meshes as well as a debugging UI provided by [imgui](https://github.com/ocornut/imgui/). 

Contributions are welcome!

Building
--------

#####Requirements

* Visual Studio 2015 (Update 1)

To build the plugin you'll need to add it to your MQ2 solution:

1. Copy (or clone) the contents of the repository into your MQ2 source tree.
2. Open MacroQuest2.sln and in solution explorer right click and 'Add -> Existing Project'
3. In the bottom right of the 'Add Existing Project' dialog, change 'All Project Files' to 'Solution Files'
4. Add MQ2Nav.sln to the project

The first time you build, you'll download some NuGet packages.

The outputs will go into your Release (or Debug) folder.

###Third Party Libraries

This plugin makes use of the following libraries:

* [EQEmu zone-utilities](https://github.com/EQEmu/zone-utilities)
* [imgui](https://github.com/ocornut/imgui/)
* [glm](https://github.com/g-truc/glm)
* [RecastNavigation](https://github.com/recastnavigation/recastnavigation)
* [SDL2](https://www.libsdl.org/download-2.0.php)
* [zlib](http://www.zlib.net/)


Contributing
------------

Feel free to open pull requests or report issues.

**TODO**

Usage
-----

**TODO**

Notes
-----

Loading into a zone with MQ2Nav loaded will generate a config file with a list of dynamic objects that can be consumed by MeshGenerator to add that extra geometry to the navmesh. These kinds of objects include some POK stones, for example. Doors are filtered out of the list, but there may be some false positives.

**TODO**

TODO List
---------

Some things to do. Eventually move these to issues...

- Split overlay code out into its own plugin
- Better path planning
- Improve the debug overlay
- Improve rendering of navmesh, etc
- Switch to TileCache implementation (for dynamic obstacle support)
- Render paths with MQ2Map
- Improved door opening behavior
- Some tiles generate incorrectly using MeshGenerator (duplicate or missing)
