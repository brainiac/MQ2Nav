# MeshTool

`MeshTool.exe` works on navmesh files from the command line. Run `MeshTool <command> -h` for a command's options.

| Command | What it does |
|---|---|
| `convert <input> [output] [--version N]` | Converts a navmesh from one file version to another. |
| `validate <mesh> [--json]` | Loads a navmesh and reports its version, tiles, polygons, walkable area, off-mesh connections and convex volumes. |
| `export <mesh> <output.json> [--json]` | Writes the navmesh's off-mesh connections, convex volumes, area types and settings to a JSON file. Never overwrites an existing file. |
| `diff <old> <new> [--tolerance N] [--strict] [--json]` | Compares two navmeshes. See below. |

## diff

`diff` is for checking a rebuilt mesh against the one it replaces, so hand-made work is not lost without anyone noticing.
It reports:

- off-mesh connections lost, added or changed, matched by their end points (either way round for two-way connections);
- convex volumes lost, added or changed, matched by outline and height range, whatever vertex the outline starts at;
- area types lost, added or changed, matched by id;
- changed build settings;
- tile and polygon counts, and the tiles whose geometry changed.

Points match when they are within `--tolerance` (default 0.5). A mesh whose tiles were edited by hand shows changed
tiles after a full rebuild even when every connection and volume survived.

Exit codes: `0` nothing was lost, `1` error, `2` connections, volumes or area types were lost. With `--strict`, `2` also
when settings or tile geometry changed.

```
MeshTool diff resources\MQ2Nav\<zone>.navmesh out\MQ2Nav\<zone>.navmesh
```

## JSON output

With `--json`, each command prints one JSON object on stdout instead of text. Errors print
`{"ok":false,"error":"..."}`. In the output, locations (`startLoc`, `endLoc`, `firstVertLoc`) are in `/loc` order (Y, X,
Z). The file `export` writes uses mesh axes, `[x, y, z]` with y up, as the navmesh stores them.
