# MQ2Nav Copilot Instructions

## Project Overview

MeshGenerator is a graphical editor for EverQuest zone files. It previews zones and generates navigation meshes used for pathfinding. This branch (`new-renderer`) focuses on implementing a new graphics engine for the editor.

The repository also contains a MacroQuest plugin (`plugin/`) but **the current focus is on MeshGenerator** (`meshgen/`).

## Architecture

### Project Structure

| Directory | Purpose |
|-----------|---------|
| `meshgen/` | **MeshGenerator editor** - main focus (x64) |
| `common/` | Shared code (NavMesh, utilities, protobuf) |
| `plugin/` | MacroQuest plugin (not current focus) |
| `cli/` | Console tool for mesh operations (x64) |
| `dependencies/` | Bundled: recast, im3d, zone-utilities |

### MeshGenerator Architecture

**Core Classes:**
- `Application` - Main application, SDL2 window management, ImGui initialization
- `Editor` - Central editor state, coordinates tools/panels/scene
- `Scene` / `Entity` - EnTT-based entity-component system for zone geometry
- `ZoneProject` / `NavMeshProject` - Project state and mesh building coordination

**Tool Pattern** (see `meshgen/NavMeshTool.h`):
```cpp
class Tool {
    virtual ToolType type() const = 0;
    virtual void handleMenu() {}
    virtual void handleClick(const glm::vec3& p, bool shift) {}
    virtual void handleRender() {}
};
```
Tools: `NavMeshTileTool`, `ConvexVolumeTool`, `OffMeshConnectionTool`, `NavMeshTesterTool`, etc.

**Panel System** (see `meshgen/PanelManager.h`):
UI panels extend `PanelWindow` with `OnImGuiRender()` and register with `PanelManager`.

**Background Tasks:**
`BackgroundTaskManager` uses **Taskflow** for async operations (mesh building, zone loading).

### Rendering Stack

- **bgfx** - Cross-platform graphics abstraction
- **SDL2** - Window and input handling
- **ImGui** - Immediate-mode UI
- **Im3D** - Debug drawing (gizmos, primitives)

## Build Instructions

**Prerequisites**: Visual Studio 2019 (16.10+)

```
# Open MQ2Nav.sln in Visual Studio
# Select x64 configuration (Release or Debug)
# Build MeshGenerator project
```

Dependencies via **vcpkg** (MSBuild integrated). First build installs automatically.
Key packages: bgfx, sdl2, entt, taskflow, imgui, protobuf, spdlog, glm.

## Code Conventions

### Logging

**MeshGenerator** (`meshgen/`) uses spdlog macros:
```cpp
SPDLOG_DEBUG("Zone changed to: {}", m_zoneShortName);
SPDLOG_INFO("Successfully loaded mesh for {}", zoneName);
SPDLOG_WARN("No nav mesh available for {}", zoneName);
SPDLOG_ERROR("Could not locate point on navmesh: {}", pos);
```

**eqglib** (`dependencies/zone-utilities/`) uses its own logging macros:
```cpp
EQG_LOG_DEBUG("Loading model: {}", modelName);
EQG_LOG_INFO("Parsed zone geometry");
EQG_LOG_WARN("Missing texture: {}", textureName);
EQG_LOG_ERROR("Failed to load archive");
```

### Math Types

- **GLM** for all vectors/matrices (`glm::vec3`, `glm::mat4`, `glm::quat`)
- Custom fmt formatters in `common/Logging.h`
- Coordinate system: conversions between EQ and navmesh coordinates happen at boundaries

### Enum Flags

Enable bitwise operations on enums:
```cpp
enum struct MyFlags : uint32_t { A = 0x01, B = 0x02 };
constexpr bool has_bitwise_operations(MyFlags) { return true; }
// Now |, &, ^ operators work
```

### Precompiled Headers

Include `pch.h` first in all `.cpp` files. `GLM_ENABLE_EXPERIMENTAL` is defined in project settings.

### Components (ECS)

Core components in `meshgen/Components.h`:
- `NameComponent`, `TagComponent` - Entity identification
- `TransformComponent` - Position, rotation, scale with matrix helpers
- `HierarchicalComponent` - Parent/child relationships

## Key Files

| File | Purpose |
|------|---------|
| `meshgen/Application.cpp` | App lifecycle, SDL/bgfx init |
| `meshgen/Editor.cpp` | Main editor UI and coordination |
| `meshgen/Scene.cpp` | EnTT registry wrapper |
| `meshgen/NavMeshBuilder.cpp` | Recast mesh generation |
| `meshgen/RenderManager.cpp` | bgfx rendering setup |
| `common/NavMesh.cpp` | Detour navmesh wrapper |
| `common/proto/NavMeshFile.proto` | Mesh file format (protobuf) |

## Rendering Primitives

For debug/editor drawing in MeshGenerator, use **bgfx directly**:
- Add vertices to `ZoneRenderManager::m_tris`/`m_triIndices` for triangles
- Add vertices to `ZoneRenderManager::m_lines` for lines
- Use `DebugDrawPolyVertex` and `DebugDrawLineVertex` structures
- Color format is ABGR packed uint32_t

Avoid these APIs (migration in progress):
- `duDebugDraw*` functions from recast (e.g., `duDebugDrawBoxWire`, `duRGBA`)
- Im3d (not fully functional yet)

## Dependencies

- **Recast/Detour** (`dependencies/recast/`) - Navmesh generation and pathfinding
- **eqglib** (`dependencies/zone-utilities/`) - Library for loading/creating EverQuest assets. Originally derived from zone-utilities; should eventually be moved to its own directory (tech debt).
- **Im3D** (`dependencies/im3d/`) - Debug drawing primitives
