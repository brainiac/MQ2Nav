EQG Zone and Model Formats
======

Notes on the layout of EQG version-2 zone files and the models they place, and on
how MQ2Nav's loaders handle them. Derived by decoding the shipped client data
rather than from documentation.

The `.zon` layout below was checked against every zone file in a Live
installation. The sections after it - terrain placement, instance rotations, and
the hierarchical models - were worked out against ruinedrelic and candlemakers
specifically, with a known-good navmesh as the reference; treat their counts as
measurements of those two zones rather than of the whole set.

This covers **EQGZ** only. The v4 format (magic `EQTZP`) is a different file,
parsed by `EQGLoader::ParseTerrainProject` into the `TerrainSystem` in
`eqg_terrain_loader.cpp`.


File layout
-----------

A `.zon` lives either as a standalone file next to `<zone>.eqg`, or as a member
of the archive itself. `EQGLoader::Load` prefers the archived copy and falls
back to the loose file.

```
zon_header              28 bytes, packed
string table            list_length bytes
model list              model_count  x uint32
placeables              object_count x variable size   <-- see below
regions                 region_count x 40 bytes
lights                  light_count  x 32 bytes
```

There is no trailing data; the last light ends the file.

### Header

```c
struct zon_header            // 28 bytes
{
    char     magic[4];       // "EQGZ"
    uint32_t version;        // 2 for every zone currently shipped
    uint32_t list_length;    // byte size of the string table
    uint32_t model_count;
    uint32_t object_count;
    uint32_t region_count;
    uint32_t light_count;
};
```

The header is 28 bytes, not 32. Every `loc` field elsewhere in the file is a
byte offset measured from the **end of the header**, i.e. from absolute offset
`sizeof(zon_header)`. Getting this wrong shifts every string by one dword and
silently produces plausible-looking but incorrect names, so it is worth
asserting.

### String table and model list

The string table is `list_length` bytes of NUL-separated names. The model list
that follows is `model_count` uint32 offsets into it, naming the `.MOD` and
`.TER` files this zone uses. Each name resolves to a member of the companion
`.eqg`, which is keyed by a CRC of the lower-cased filename, so lookup is
effectively case-insensitive.

Names carry their role as a prefix: `TER_<zone>.TER` for terrain, and both
`OBJ_<name>.MOD` and `OBP_<name>.MOD` for placed objects. The two object
prefixes are not interchangeable in practice - `OBP_` is used for the small
props and decorations, which are typically non-collidable, and those are the
bulk of what `BuildCollisionMesh` legitimately skips.

### Placeables

Each placeable is a fixed 36-byte record followed, in version 2 and later, by a
variable-length block:

```c
struct zon_placable          // 36 bytes
{
    int32_t  id;             // index into the model list
    uint32_t loc;            // offset of this instance's name
    float    x, y, z;
    float    rx, ry, rz;     // radians
    float    scale;          // uniform
};

uint32_t color_count;        // version >= 2 only
uint32_t colors[color_count];
```

`colors` is baked per-instance vertex lighting, packed `0xAARRGGBB`. Its length
is the referenced model's `vert_count`, or an exact multiple of it where a zone
stores more than one lighting set per instance. The count field is
authoritative, so a loader that only wants geometry can skip
`color_count * sizeof(uint32_t)` bytes without consulting the model.

This block is the only variable-length part of the file. Because records cannot
be indexed, the placeable array has to be walked sequentially.

### Regions and lights

Both are fixed size: 40 bytes per region and 32 per light. `eqglib` calls them
`SZONArea` and `SZONLight` - see the mapping table below - and region `extend_*`
values are half-extents about the region centre.


How this was verified
---------------------

Walking `36 + 4 + color_count * 4` bytes per placeable lands exactly on the
region block, byte for byte, in **265 of 265** zone files, with regions and
lights then decoding into sensibly named data. Across those zones:

* 429,691 placeables, none with a model id outside the model list
* 26,845 model references, all resolving to real `EQGM` / `EQGT` members
* `color_count` equal to the model's vertex count, or an exact multiple of it
* 160,298 placeables (37%) carry a scale other than 1.0

The 2x multiple on `color_count` shows up on `roost` and `xorbb` among others.

Checked again independently since: walking the whole file as header, string pool,
model list, `36 + 4 + color_count * 4` per instance, then `num_areas * 40` and
`num_lights * 32`, lands on the final byte with **zero** left over in both
ruinedrelic and candlemakers. That is worth knowing because it rules out a spare
field: there is nowhere in the 36-byte instance record, or the 28-byte header,
for a flag that might select between rotation conventions or mark an instance as
animated. Whatever distinguishes those instances is in the model, not the `.zon`.

Mapping onto `eqglib`
---------------------

The `zon_*` names used above are this document's, from the original decoding
work; they do not appear in the source. `eqglib/eqg_structs.h` calls the same
records something else, and the fields line up one for one:

| Above          | `eqglib`       | Notes                                     |
| -------------- | -------------- | ----------------------------------------- |
| `zon_header`   | `SZONHeader`   | 28 bytes; `list_length` is `string_pool_size` |
| model list     | `num_meshes`   | uint32 offsets into the string pool        |
| `zon_placable` | `SZONInstance` | `mesh_id`, `name`, `translation`, `rotation`, `scale` |
| `zon_region`   | `SZONArea`     | `extents` are half-extents about `center`  |
| `zon_light`    | `SZONLight`    |                                            |

`EQGLoader::ParseZone` reads the v2 baked-lighting block correctly - it takes the
`uint32` count and then that many colours, as `litVerts`, rather than skipping
it blind - and it hands those colours to the actor it creates for each instance.
The `.TER`/`.MOD` suffix tests use `ci_ends_with`, so the empty-name underflow
that the older `zone-utilities` loader had does not exist here.


The terrain instance's transform must NOT be applied
----------------------------------------------------

In a v2 zone, instance 0 is the terrain: it names the `.TER` mesh and carries a
translation, rotation and scale in the same record shape as every other
instance. Those values are **not** a placement for the terrain. The `.TER`
vertices are already authored in world space, and `ParseZone` skipping instance
0 - the actor-creating branch is guarded on
`header->version == 1 || instanceId > 0` - is correct, not an oversight.

Applying that transform moves the terrain out from under everything else. It is
tempting to apply it: 223 of 265 shipped zones give the terrain instance a
non-zero position, and almost all of them a `rx = -90`, so it looks like a
placement being dropped. It is not.

### The measurement

Take the `.TER` vertices straight out of the archive, apply only the `.yzx`
swizzle that `InitFromEQGData` does, and compare against a known-good navmesh
built from the real zone geometry:

|                | source                        | x                     | y                    | z                     |
| -------------- | ----------------------------- | --------------------- | -------------------- | --------------------- |
| ruinedrelic    | `.TER`, swizzle only          | `-1897.26 .. 1975.77` | `-743.50 .. 866.30`  | `-1604.38 .. 2339.08` |
|                | navmesh (ground truth)        | `-1897.26 .. 1975.77` | `-581.30 .. 3531.55` | `-1604.40 .. 2339.08` |
| candlemakers   | `.TER`, swizzle only          | `-2016.33 .. 1894.62` | `-324.03 .. 563.78`  | `-2599.96 .. 2372.01` |
|                | navmesh (ground truth)        | `-1975.47 .. 1894.62` | `-324.03 .. 450.00`  | `-2599.96 .. 2000.00` |

The horizontal bounds agree to within 0.02 units with no transform applied. They
cannot agree afterwards: ruinedrelic's terrain instance would shift the mesh by
`(138.25, -200.64, -294.89)` and rotate it 90 degrees. Where the navmesh is
narrower than the terrain (candlemakers x min, z max) it is clipped by the
build's max extents; every unclipped edge matches exactly.

The vertex counts corroborate the pairing independently: instance 0's baked
lighting block is 33,508 entries for ruinedrelic and 540,889 for candlemakers,
matching each `.TER`'s vertex count exactly. Instance 0 does describe the
terrain - the loader is right to read its colours - it just does not place it.

### One real inconsistency nearby

`InitFromEQGData` stores each vertex as `inVertex.pos.yzx` but grows the
bounding box with `m_aabb.enclose(inVertex.pos)`, the unswizzled value, so
`Terrain::m_aabb` ends up in a different frame from `Terrain::m_vertices`. The
WLD path a few hundred lines above encloses the final swizzled vertex, which
looks like the intended behaviour. `m_aabb` currently feeds only
`BuildConvexHull` in `GeometryUtils.cpp`. This is left as found - noted here
rather than changed, since it is unrelated to placement and untested.

Instance rotations
------------------

`SZONInstance::rotation` is three Euler angles in the file's own coordinate
frame. Model vertices are stored pre-swizzled by `.yzx` (`InitFromEQGData`), so
the rotation has to be moved into that same frame before it is applied.
`ParseZone` used to do that by permuting the Euler components:

```c
    orientation = glm::vec3(rotation.z, rotation.y, rotation.x).yzx;   // (ry, rx, rz)
```

Permuting components is not, in general, the same thing as changing frames. It
agrees with the real conversion only when the rotations commute - when at most
one axis is non-zero - because `glm::quat(vec3)` composes its three angles in a
fixed order and the permutation reorders them. The conversion that is always
correct conjugates the rotation by the same swizzle the vertices get:

```
    P maps (x, y, z) -> (y, z, x)
    R_world = P * R_file * transpose(P)      with R_file = quat(rz, ry, rx)
```

Almost every zone rotates its instances about a single axis, where the shortcut
happens to work:

| zone         | rotation about `rx` only | all three axes |
| ------------ | ------------------------ | -------------- |
| ruinedrelic  | 273                      | 6 of 285       |
| candlemakers | 1731                     | 5136 of 7001   |

candlemakers is the zone that exposes it. `OBJ_CaveWallBoulder_b.MOD` at
`(-284.83, -81.76, 1412.71)` with rotation `(117.86, -64.61, -67.55)` degrees is
a clear case: the mesh sits well away from its instance origin, so a wrongly
composed rotation swings a wall-sized rock below the map, leaving the
stalactites above it floating. The 49 `OBJ_CaveMound` instances near
`/loc -278, 41, 36` are another: 46 of them rotate on more than one axis, and
they are what makes the ceiling there read as solid rock rather than a hole.

### How the convention was pinned down

Two things are unknown up front - which permutation `P` is, and which order the
three stored angles compose in - so there are 6 x 6 candidates, and guessing
between them is what made the first attempt at this fail.

The constraint that settles it: single-axis instances already render correctly,
so the right candidate must reproduce the old result on **every** one of them,
and differ only on multi-axis ones. Scoring all 36 against candlemakers leaves
three:

| candidate | frame | angle order | differs on multi-axis |
| --------- | ----- | ----------- | --------------------- |
| 2         | `012` | `102`       | 0 of 5214             |
| 23        | `120` | `210`       | 3843 of 5214          |
| 25        | `201` | `021`       | 3748 of 5214          |

Candidate 2 is the old code restated - `.yzx` of `(rz, ry, rx)` is `(ry, rx,
rz)` - which is why it differs on nothing, and it doubles as a check that the
scoring is sound. Candidates 23 and 25 are inverses of each other, and 23 is the
one whose frame permutation is the `.yzx` swizzle the vertices actually get.

`eqg_loader.cpp` keeps a cheap version of that check permanently: it counts
single-axis instances whose rotation the conversion moved, and logs an error if
any did, since those were correct before and a change there means the conversion
is wrong.

### Why it could not be done with Euler angles

Computing `R_world` and handing it back through `glm::eulerAngles` fixes
candlemakers and breaks everything else - tried, and reverted. `glm::eulerAngles`
and the `glm::quat(vec3)` constructor do not agree on a composition convention,
so the round trip does not reproduce the rotation it was given.
`TransformComponent::SetRotation` in `meshgen/Components.h` still carries a
hand-written correction for a symptom of the same inconsistency.

`eqg::Actor` therefore carries a `glm::quat` alongside its Euler angles, set by
`SetRotation`, and `ZoneResourceManager::AddActor` takes that quaternion rather
than the angles. `TransformComponent` builds its matrix from a quaternion
already, and its `rotationEuler` field was write-only, so nothing downstream
needed changing. Upstream still has the reordering.

Animated instances: `.ani` files and hierarchical models
--------------------------------------------------------

Nothing in the `.zon` marks an instance as animated. The distinction lives in the
model: a `.mod` whose `SEQMHeader.num_bones` is non-zero is loaded as a
*hierarchical* model rather than a simple one, and a companion
`<model>_default.ani` in the same archive carries its motion. ruinedrelic has
five - `obj_rubblefloata/b/c` and `obj_magicbridgea/b` - and candlemakers has
none, which is why only the former showed the problems below.

In game these are the pieces of rubble that drift and tumble within an invisible
volume and rebound off it. `eqglib` does not read any of that: `EQGLoader::ParseFile`
matches `.ani` and returns success without parsing a byte, so the animation is
neither loaded nor applied and the models are only ever drawn in their bind pose.
For a navmesh tool that is reasonable as far as it goes - but note the geometry
does move in game, so a navmesh built over it describes a surface that will not
always be there. That may still be wanted; off-mesh connections can be anchored
to the polygons it produces.

### The EQGM layout, including bones

`SEQMHeader` is **28 bytes**, not 24 - it carries `num_bones` after `num_faces`.
Sections follow in this order:

```
SEQMHeader                28 bytes
string pool               string_pool_size bytes
materials                 num_materials x (16 + num_params * 12)
vertices                  num_vertices x 44   (32 when version <= 2)
faces                     num_faces x 20
bones                     num_bones x 56      (SEQMBoneData)
skin data                 num_vertices x variable (weight count, then weights)
```

Because the skin block is variable length, the vertex offset cannot be derived by
subtracting the fixed-size tail from the file size - that only works for models
with no bones, and silently produces garbage for the rest. Walk the material
block instead, and check that every face index is below `num_vertices`.

The bones of these models are mostly not deformation bones.
`obj_rubblefloata.mod` has 21: `ROOT_BONE` and `Point001`..`Point020`, every one
with an identity quaternion and unit scale, differing only in pivot. They are
attachment locators spread across the rock, roughly 402 x 374 units for a mesh
whose own extent is 400 x 354 x 59.

### Four things that were wrong with the hierarchical path

All four are fixed here, as is the rotation problem above. Worth
recording together, because they compounded and each one masked the next - the
600x scale error hid the rest until it was out of the way:

* `ResourceManager::CreateHierarchicalActor` forwarded `boundingRadius` and
  `scale` to the constructor in the wrong order, so every hierarchical actor used
  its bounding radius as its scale factor - about 269 in place of 0.446 for the
  rubble, some 600x too large. `CreateSimpleActor` has it right.
* `UpdateDefaultPoseBoneMatrices` accumulates from `GetDefaultPoseMatrix()`, but
  the EQG bone constructor initialises that to `inverse(m_mtx)` rather than the
  local matrix, and composes child * parent rather than parent * child. What it
  produces is not a world pose, and skinning by it displaces each vertex by
  roughly its bone's negated pivot - smearing the rubble across the 400-unit
  spread of its attachment points, 538 units of mesh reported as 1101.
* `InitSkinFromEQMData` stores raw file positions, unlike `SimpleModelDefinition`
  which stores them `.yzx` swizzled. The bone matrices had been carrying that
  permutation, so removing the skinning transform dropped the coordinate change
  with it and stood the slabs on edge.
* `SetCollisionMesh` is only ever called from the WLD loader, so an EQG
  hierarchical model had no collision data, `m_hasCollision` stayed false,
  `HierarchicalActor::IsCollidable` returned false, no `CollisionComponent` was
  attached, and `BuildCollisionMesh` never saw it. The models were missing from
  the collision mesh entirely, with nothing logged.

Since MeshGenerator never poses these models, the bind pose is the only pose, and
the skinning transform reduces to the file-to-world permutation. The pre-eqglib
loader had no hierarchical path at all - it drew each `.mod` as plain geometry at
the instance transform - which is why it placed them correctly all along, and why
it is the right reference when this code misbehaves.
