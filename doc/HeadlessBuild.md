# Building a navmesh without the GUI

```
MeshGenerator.exe build --zone <zone> --eq-path <EverQuest folder> --stage-root <folder> [options]
```

Builds `<stage-root>\MQ2Nav\<zone>.navmesh` with the same code the GUI uses, loads the result back with MQ2Nav's own
loader, and only then reports success. It never overwrites a file. Progress goes to stderr and the result is one JSON
object on stdout. Run `MeshGenerator.exe build --help` for every option.

- **Settings.** `--settings-from <mesh>` starts from another mesh's build settings. Single settings go on top:
  `--tile-size`, `--cell-size`, `--cell-height`, `--agent-height`, `--agent-radius`, `--max-climb`, `--max-slope`,
  `--region-min-size`, `--region-merge-size`, `--edge-max-len`, `--edge-max-error`, `--verts-per-poly`,
  `--detail-sample-dist` and `--detail-sample-max-error`. The limits follow the GUI sliders.
- **Keeping hand-made work.** `--carry-from <mesh>` copies another mesh's off-mesh connections, convex volumes and area
  types into the build. `--fixes <file>` reads what `MeshTool export` writes. `--connections <file>` and
  `--volumes <file>` take JSON arrays in mesh axes (`[x, y, z]`, y up):

  ```json
  [{"name": "ledge", "start": [0, -63.8, 0], "end": [25, -63.4, 0], "bidirectional": false}]
  [{"name": "lava", "verts": [[-10, -80, -10], [10, -80, -10], [10, -80, 10]], "hmin": -80, "hmax": -40, "area": 6}]
  ```

- **Region rebuild.** `--base <mesh>` loads an existing mesh and rebuilds only the tiles touched by
  `--box "Y1,X1:Y2,X2"` (a `/loc` box) and by any new connections or volumes. Settings come from the base mesh. Grid
  cells with no tile in the base are refused, with a list.
- **Other.** `--threads N` caps the build threads. `--mesh-version 4|5` picks the file version; carried and region
  builds keep the input mesh's version. `--mesh-root` points to the MacroQuest resources folder used to find
  `MQ2Nav\<zone>_doors.json` and defaults to `--eq-path`. `--no-max-extents` skips the built-in zone bounds.

The result also lists off-mesh connections whose ends are no longer on the mesh.

MeshGenerator is a windows program, so a shell does not wait for it on its own. From cmd, use
`start /wait "" MeshGenerator.exe build ...` to wait and get the exit code. From PowerShell, pipe the output
(`| ConvertFrom-Json` or `| Out-String`).

Exit codes: `0` success, `2` invalid arguments, `3` missing input, `4` build or write error, `5` the built mesh did not
load back, `70` unexpected failure.
