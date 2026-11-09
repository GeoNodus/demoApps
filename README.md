# GeoNodus demo apps

<p align="center">
  <img src="images/boolean.png" width="32%" alt="Boolean">
  <img src="images/copyxform.png" width="32%" alt="Copy and Transform">
  <img src="images/orientalongcurve.png" width="32%" alt="Orient along Curve">
</p>

**GeoNodus** is a cross-platform C++23 shared library for **procedural geometry built from a node graph**. An
application creates a session, builds a graph of operators (generators, deformers, modeling tools, copies, curves,
Booleans, ...), connects them, sets and animates their parameters with expressions, and lets the library cook the
result in the background. The finished geometry — points, polygons, NURBS and Bezier curves, attributes and groups — is
read back through a plain C API and drawn by the application in any renderer.

This repository contains the **demo applications** of the library and the demo scenes they show: one scene for every
operator, built in code with the GeoNodus API. The library itself ships here as a binary of the demo edition.

> **Disclaimer.** GeoNodus is an independent project. It is **not affiliated with, endorsed by or connected to
> SideFX** or **Houdini** in any way, and it contains no SideFX code. Its public API only *follows the design* of the
> Houdini Engine API (HAPI): `HAPI_*` functions and types become `GN_*`, so the concepts and the calling patterns are
> familiar to anyone who has used HAPI. Houdini, Houdini Engine and HAPI are trademarks of SideFX.

## Contents

- [What the library does](#what-the-library-does)
- [Third-party libraries](#third-party-libraries)
- [Demo apps and platforms](#demo-apps-and-platforms)
- [Quick start with the C API](#quick-start-with-the-c-api)
- [Implemented features](#implemented-features)
  - [Generators](#generators): [Sphere](#sphere), [Box](#box), [Grid](#grid), [Torus](#torus), [Circle](#circle),
    [Add](#add)
  - [Attributes](#attributes): [Attribute Create](#attribute-create), [Attribute Randomize](#attribute-randomize),
    [Attribute Blur](#attribute-blur), [Normal](#normal), [UV Texture](#uv-texture)
  - [Groups](#groups): [Group Create](#group-create), [Group Range](#group-range)
  - [Deformers](#deformers): [Transform](#transform), [Mountain](#mountain), [Bend](#bend),
    [Path Deform](#path-deform), [Match Size](#match-size), [Fuse](#fuse)
  - [Modeling](#modeling): [Clip](#clip), [PolyExtrude](#polyextrude), [PolyBevel](#polybevel),
    [Boolean](#boolean)
  - [Copies and points](#copies-and-points): [Scatter](#scatter), [Copy to Points](#copy-to-points),
    [Copy and Transform](#copy-and-transform)
  - [Curves](#curves): [Fit](#fit), [Resample](#resample), [Carve](#carve), [Join](#join),
    [Orient along Curve](#orient-along-curve)
  - [Networks and objects](#networks-and-objects): [Merge](#merge), [Null](#null), [Subnet](#subnet),
    [Output](#output), [Geo objects](#geo-objects), [Null objects](#null-objects)
  - [Geometry from the application](#geometry-from-the-application): [Input nodes](#input-nodes),
    [Input nodes and Boolean](#input-nodes-and-boolean)

## What the library does

- **Sessions and a node graph.** Every session owns its own graph with the root `/obj`. Objects (`geo`, `null`) live on
  `/obj`, geometry operators (SOPs) live inside `geo` objects and SOP subnets. Nodes are connected input to output;
  cycles are rejected.
- **Parameters as in HAPI.** Every node publishes its parameters — ints, floats, toggles, menus, strings, file paths,
  ramps, multiparms, folders and tabs with hide/disable conditions — through `gnGetParameters` and flat value arrays
  (`gnGetParmFloatValues`, `gnSetParmIntValues`, ...). User (spare) parameters can be added to any node.
- **Expressions and time.** Any parameter component can carry an expression of time (`$T`, `$F`, `$FPS`, ...) and of
  other parameters (`ch("../ctrl/radius")`). Only the nodes that depend on what changed are cooked again.
- **Asynchronous cook.** `gnCookNode` does not block: the cook runs on the job thread of the session, the application
  polls `gnGetCookState` or waits with `gnWaitCook`. Dirty propagation, skipped up-to-date nodes and copy-on-write
  geometry pages keep re-cooking cheap.
- **Geometry.** Points, vertices, polygons and polylines, NURBS and Bezier curves; point, vertex, primitive and detail
  attributes of any type (int, float, double, string); point, primitive and edge groups. Everything is read through
  HAPI-style parts, attribute and curve queries (`gnGetPartInfo`, `gnGetAttributeFloatData`, `gnGetCurveKnots`, ...).
- **Geometry from the application.** `gnCreateInputNode` and the geometry setters (`gnSetPartInfo`, `gnAddAttribute`,
  `gnSetVertexList`, `gnSetCurveInfo`, `gnCommitGeo`, ...) hand meshes and curves made by the application to the graph.
- **Saving and loading graphs.** The whole graph — nodes, parameters, expressions, connections, user parameters, input
  geometry and time — is stored as JSON (`gnSaveGraph`, `gnLoadGraph`).
- **Results checked against references.** The behavior of every operator is verified by tests against reference data,
  so the results match what users of procedural tools expect.

The demo edition of the library, included in this repository, creates at most 20 nodes per scene through
`gnCreateNode` and cannot save graphs (`gnSaveGraph` returns `GN_RESULT_DEMO_LIMIT`). Everything else works.

## Third-party libraries

GeoNodus is built on these open-source libraries (linked statically into the library):

| Library | Used for | License |
|---|---|---|
| [oneTBB](https://github.com/uxlfoundation/oneTBB) | parallel cooking of geometry, the scalable memory allocator | Apache 2.0 |
| [Manifold](https://github.com/elalish/manifold) | the robust mesh Boolean kernel of the Boolean operator | Apache 2.0 |
| [muParser](https://github.com/beltoforion/muparser) | parameter expressions | BSD 2-Clause |
| [simdjson](https://github.com/simdjson/simdjson) | reading graphs and geometry from JSON | Apache 2.0 / MIT |

## Demo apps and platforms

| Folder | Platform | Renderer |
|---|---|---|
| `win/gl` | Windows x64 and ARM64 | OpenGL 3.3 core |
| `win/vulkan` | Windows x64 and ARM64 | Vulkan 1.0 |
| `mac/metal` | macOS 12+ (Apple Silicon) | Metal (Swift, AppKit + MetalKit) |
| `iOS/metal` | iOS / iPadOS 15+ (device and simulator) | Metal (Swift, UIKit + MetalKit) |
| `scenes` | all platforms | the demo scenes in plain C++ over `geonodus.h` |

Every app has the same controls: open a graph file, pick a demo scene by node type, play the animation in real time or
scrub the timeline, orbit / zoom / pan the camera (F — frame everything). The binaries of the demo edition of the
library are in `win/lib`, `mac/lib` and `iOS/lib`.

```bash
# Windows (from a Visual Studio developer prompt)
cmake -S win/gl -B win/gl/build/release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build win/gl/build/release
win/gl/build/release/geonodus_gl.exe --scene boolean --play
```

```bash
# macOS
cmake -S mac/metal -B mac/metal/build/xcode -G Xcode
cmake --build mac/metal/build/xcode --config Release
```

On iOS open `iOS/GeoNodusMetalDemo.xcodeproj` in Xcode, choose a signing team and run.

## Quick start with the C API

The whole API is C: functions `gn*` in `geonodus.h`, types and enums `GN_*` in `geonodus_API.h`. As in HAPI,
parameters are found by name in the list returned by `gnGetParameters` and written into flat value arrays:

```cpp
#include <geonodus.h>
#include <string>
#include <vector>

// A parameter by name: GN_ParmInfo gives its id and its index in the flat int / float / string arrays
GN_ParmInfo findParm(GN_SessionId s, GN_NodeId node, const char* name)
{
    GN_NodeInfo info{};
    gnGetNodeInfo(s, node, &info);
    std::vector<GN_ParmInfo> parms(info.parmCount);
    gnGetParameters(s, node, parms.data(), 0, info.parmCount);
    for (const GN_ParmInfo& p : parms) {
        int length = 0;
        gnGetStringBufLength(s, p.nameSH, &length);   // as in HAPI: first the length, then the string
        std::string parmName(length, '\0');
        gnGetString(s, p.nameSH, parmName.data(), length);
        if (parmName.c_str() == std::string(name))
            return p;
    }
    GN_ParmInfo none{};
    none.id = -1;
    return none;
}

int main()
{
    GN_SessionId s;
    gnCreateSession(&s);

    // /obj/terrain: a grid displaced by noise
    GN_NodeId geo, grid, noise;
    gnCreateNode(s, 0, "geo", "terrain", false, &geo);        // 0 is the root /obj
    gnCreateNode(s, geo, "grid", "land", false, &grid);
    gnCreateNode(s, geo, "mountain", "noise", false, &noise);
    gnConnectNodeInput(s, noise, 0, grid, 0);
    gnSetNodeDisplay(s, noise, 1);                            // the SOP the object outputs

    const float size[] = {10, 10};
    gnSetParmFloatValues(s, grid, size, findParm(s, grid, "size").floatValuesIndex, 2);
    const int rows = 80;
    gnSetParmIntValues(s, grid, &rows, findParm(s, grid, "rows").intValuesIndex, 1);
    // The noise height follows time: $T — seconds, $F — frame, ch("path") — another parameter
    gnSetParmExpression(s, noise, "1 + 0.5 * sin($T)", findParm(s, noise, "height").id, 0);

    // Cook at t = 2 s: the call only queues the cook on the session's job thread
    gnSetTime(s, 2.0);
    GN_CookOptions options = gnCookOptions_Create();
    gnCookNode(s, geo, &options);
    gnWaitCook(s, geo, -1);

    // Read the result: the Display SOP of the object, its first part, positions and triangles
    GN_GeoInfo geoInfo;
    gnGetDisplayGeoInfo(s, geo, &geoInfo);
    GN_PartInfo part;
    gnGetPartInfo(s, geoInfo.nodeId, 0, &part);
    GN_AttributeInfo pInfo;
    gnGetAttributeInfo(s, geoInfo.nodeId, 0, "P", GN_ATTROWNER_POINT, &pInfo);
    std::vector<float> P(pInfo.count * 3);
    gnGetAttributeFloatData(s, geoInfo.nodeId, 0, "P", &pInfo, -1, P.data(), 0, pInfo.count);
    std::vector<int> vertices(part.vertexCount);
    gnGetVertexList(s, geoInfo.nodeId, 0, vertices.data(), 0, part.vertexCount);

    gnCloseSession(s);
}
```

### How the feature snippets are written

The scenes below are built with `SceneBuilder` from [`scenes/scene_builder.h`](scenes/scene_builder.h) — a thin helper
in which every call is one plain API call, so a scene reads like a description of the node graph:

| Helper | API |
|---|---|
| `b.object("name", "geo")`, `b.sop(parent, "box", "name")` | `gnCreateNode` |
| `.set("size", 2, 2, 2)` | `gnGetParameters` + `gnSetParmFloatValues` / `gnSetParmIntValues` |
| `.set("type", "polymesh")` — a menu item by its token | `gnGetParmChoiceLists` + `gnSetParmIntValues` |
| `.set("group", "0-120")` — a string | `gnSetParmStringValue` |
| `.expr("r", 1, "$T * 30")`, `.expr("t", {"x", "y", "z"})` | `gnSetParmExpression` |
| `.input(0, other)` | `gnConnectNodeInput` |
| `.display()` | `gnSetNodeDisplay` |
| `.addFloatParm(...)` | `gnAddSpareParm` |

`color(b, net, input, r, g, b)` and `merge(b, net, {...})` from [`scenes/scene_util.h`](scenes/scene_util.h) add an
Attribute Create node with a constant color `Cd` and a Merge node. The full code of every scene is in
[`scenes/`](scenes); in the apps a scene is opened from the scene list or with `--scene <node type>`.

## Implemented features

### Generators

#### Sphere

`sphere` — a polygon mesh sphere (rows × columns) or a subdivided icosahedron; with an input — the bounding sphere of
the input points.

```cpp
Node geo = b.object("spheres");
Node mesh = b.sop(geo, "sphere", "mesh_sphere")
                .set("type", "polymesh")
                .expr("rows", "rint(12 + 8 * sin($T * 1.3))")
                .expr("cols", "rint(24 + 14 * sin($T * 0.9))")
                .expr("rad", {"1.3 + 0.2 * sin($T * 2)", "ch(\"radx\")", "ch(\"radx\")"});
Node ico = b.sop(geo, "sphere", "ico_sphere").set("type", "poly").expr("freq", "rint(3.5 + 2.5 * sin($T * 0.8))");
// With an input: the sphere bounding a moving grid
Node bound = b.sop(geo, "sphere", "bounding_sphere").input(0, grid).set("type", "polymesh");
```

![Sphere](images/sphere.png)

#### Box

`box` — a box with axis divisions, size, rotation and uniform scale; with an input — the bounding box of the input.

```cpp
Node divided = b.sop(geo, "box", "divided_box")
                   .expr("divrate", {"rint(3 + 2 * sin($T))", "rint(3 + 2 * sin($T * 1.3))", "3"})
                   .expr("size", {"1.6", "1.6 + 0.6 * sin($T * 2)", "1.6"})
                   .expr("r", {"$T * 20", "$T * 35", "0"});
// The bounding box of a spinning torus, its top half clipped away
Node bbox = b.sop(geo, "box", "bounding_box").input(0, torus);
Node tray = b.sop(geo, "clip", "tray").input(0, bbox).set("clipop", "below").set("dir", 0, 1, 0);
```

![Box](images/box.png)

#### Grid

`grid` — a grid of every connectivity: quadrilaterals, triangles, alternating triangles, rows, columns, rows and
columns.

```cpp
for (const char* surftype : {"quads", "triangles", "alttriangles", "rows", "rowcol"})
    b.sop(geo, "grid", surftype)
        .set("surftype", surftype)
        .set("orient", "xy")
        .expr("rows", "rint(6 + 4 * sin($T * 0.9))")
        .expr("cols", "rint(6 + 4 * cos($T * 0.7))");
```

![Grid](images/grid.png)

#### Torus

`torus` — a torus with ring and tube radii and divisions; here also a chain of tori made with Copy and Transform.

```cpp
Node donut = b.sop(geo, "torus", "donut")
                 .expr("rad", {"1.5 + 0.3 * sin($T)", "0.5 + 0.25 * sin($T * 1.7)"})
                 .expr("rows", "rint(10 + 6 * sin($T * 0.8))")
                 .expr("cols", "rint(20 + 12 * sin($T * 0.6))");
Node link = b.sop(geo, "torus", "link").set("orient", "z").set("rad", 0.5, 0.11);
Node chain = b.sop(geo, "copyxform", "chain").input(0, link).set("ncy", 9).set("t", 0.78, 0, 0).set("r", 90, 0, 0);
```

![Torus](images/torus.png)

#### Circle

`circle` — polygon circles and arcs (closed, open arc, closed arc, sliced arc) and exact rational NURBS and Bezier
circles.

```cpp
Node pie = b.sop(geo, "circle", "pie")
               .set("arc", "slicedarc")
               .set("divs", 24)
               .expr("angle", {"$T * 20", "$T * 20 + 270 + 80 * sin($T * 1.3)"});
Node nurbs = b.sop(geo, "circle", "nurbs_circle")
                 .set("type", "nurbs")
                 .set("imperfect", 0)          // an exact circle of rational quadratic segments
                 .set("order", 3)
                 .expr("rad", {"1 + 0.4 * sin($T * 1.2)", "1 - 0.4 * sin($T * 1.2)"});
```

![Circle](images/circle.png)

#### Add

`add` — points of a multiparm (`pt0`, `pt1`, ...), each driven by its own expressions, and polygons or polylines by a
pattern of point numbers.

```cpp
Node star = b.sop(geo, "add", "star").set("points", 10);   // the number of multiparm instances
for (int k = 0; k < 10; ++k) {
    const char* radius = k % 2 == 0 ? "2.2" : "(1 + 0.4 * sin($T * 3))";
    star.expr(fmt("pt%d", k).c_str(), {fmt("%s * cos(%f + $T * 0.5)", radius, k * 0.6283).c_str(),
                                       fmt("%s * sin(%f + $T * 0.5)", radius, k * 0.6283).c_str(), nullptr});
}
star.set("prims", 1).set("prim0", "0-9").set("closed0", 1);   // one closed polygon through points 0..9
```

![Add](images/add.png)

### Attributes

#### Attribute Create

`attribcreate` — attributes of any class, type and size, one per instance of the `numattr` multiparm; values can be
written only to the elements of a group.

```cpp
Node base = b.sop(geo, "attribcreate", "base_color")
                .input(0, sphere)
                .set("name1", "Cd").set("class1", "point").set("type1", "vector").set("size1", 3)
                .expr("value1v", {"0.3 + 0.2 * sin($T)", "0.4 + 0.2 * sin($T * 1.3 + 2)", "0.7"});
// Only the points of the group "spot" (made by Group Create) get another color
Node spotColor = b.sop(geo, "attribcreate", "spot_color")
                     .input(0, spot)
                     .set("group", "spot").set("grouptype", "points")
                     .set("name1", "Cd").set("class1", "point").set("type1", "vector").set("size1", 3)
                     .expr("value1v", {"1", "0.8 + 0.2 * sin($T * 5)", "0.2"});
```

![Attribute Create](images/attribcreate.png)

#### Attribute Randomize

`attribrandomize` — random values of an attribute (set, add, min, max, multiply) within a range, with a seed.

```cpp
b.sop(geo, "attribrandomize", "mosaic_colors")
    .input(0, grid)
    .set("class", "primitive")
    .set("name", "Cd")
    .expr("seed", "rint($T * 2)")
    .set("max", 1, 0.8, 1, 1);
// Random point sizes: Copy to Points scales the copies by pscale
Node sizes = b.sop(geo, "attribrandomize", "sizes")
                 .input(0, scatteredPoints)
                 .set("name", "pscale").set("dimensions", 1)
                 .set("min", 0.04, 0, 0, 0)
                 .expr("max", 0, "0.12 + 0.08 * sin($T * 2)");
```

![Attribute Randomize](images/attribrandomize.png)

#### Attribute Blur

`attribblur` — smoothing of point attributes over the mesh: colors, or positions themselves (Laplacian or
volume-preserving).

```cpp
Node colorBlur = b.sop(geo, "attribblur", "blur_colors")
                     .input(0, noisyColors)
                     .set("attributes", "Cd")
                     .expr("iterations", "rint(12 + 12 * sin($T * 0.8))");
Node smooth = b.sop(geo, "attribblur", "smooth_terrain")
                  .input(0, roughTerrain)
                  .set("attributes", "P")
                  .set("mode", "volpreserving")
                  .expr("iterations", "rint(15 + 15 * sin($T * 0.6))");
```

![Attribute Blur](images/attribblur.png)

#### Normal

`normal` — normals of points, vertices (with a cusp angle), primitives or the detail, weighted equally, by angle or by
area. Here spikes are copied onto the points and turned along `N`.

```cpp
Node bumps = b.sop(geo, "mountain", "bumps").input(0, sphere).expr("height", "0.6 + 0.3 * sin($T)");
Node normals = b.sop(geo, "normal", "point_normals")
                   .input(0, bumps)
                   .set("type", "typepoint")
                   .set("method", "area");
Node spike = b.sop(geo, "box", "spike").set("size", 0.05, 0.05, 0.6).set("t", 0, 0, 0.3);
Node spikes = b.sop(geo, "copytopoints", "spikes").input(0, spike).input(1, normals).set("pivot", "origin");
```

![Normal](images/normal.png)

#### UV Texture

`texture` — texture coordinates by orthographic, polar, cylindrical, rows-and-columns, face and spline projections.
Here they are written into `Cd`, so the coordinates are seen as colors.

```cpp
Node polar = b.sop(geo, "texture", "polar")
                 .input(0, sphere)
                 .set("uvattrib", "Cd")
                 .set("type", "polar")
                 .expr("offset", {"0.3 * sin($T)", "0.3 * cos($T * 0.7)", "0"});
Node cylin = b.sop(geo, "texture", "cylindrical")
                 .input(0, torus)
                 .set("uvattrib", "Cd")
                 .set("type", "cylin")
                 .expr("s", {"0.7 + 0.3 * sin($T)", "0.7 + 0.3 * cos($T * 1.3)", "1"});
```

![UV Texture](images/texture.png)

### Groups

#### Group Create

`groupcreate` — point, primitive and edge groups by bounding regions (box, sphere), by normals, by edge rules and by
random chance.

```cpp
Node inBox = b.sop(geo, "groupcreate", "in_box")
                 .input(0, sphere)
                 .set("groupname", "inbox").set("grouptype", "point")
                 .set("groupbase", 0).set("groupbounding", 1).set("boundtype", "usebbox")
                 .expr("t", {"2.2 * cos($T * 0.8)", "1.2 * sin($T * 1.3)", "2.2 * sin($T * 0.8)"});
Node facing = b.sop(geo, "groupcreate", "facing")
                  .input(0, torus)
                  .set("groupname", "facing").set("grouptype", "primitive")
                  .set("groupbase", 0).set("groupnormal", 1)
                  .expr("dir", {"cos($T * 0.8)", "0.6 * sin($T * 0.7)", "sin($T * 0.8)"})
                  .expr("angle", "50 + 20 * sin($T * 1.5)");
```

![Group Create](images/groupcreate.png)

#### Group Range

`grouprange` — groups by ranges of element numbers: start/end, start and length, every N of M with an offset, equal
partitions, per connected piece.

```cpp
Node stripes = b.sop(geo, "grouprange", "n_of_m")
                   .input(0, sphere)
                   .set("groupname1", "stripes").set("grouptype1", "prims")
                   .set("selectamount1", 2)
                   .expr("selecttotal1", "rint(6 + 3 * sin($T * 0.4))")
                   .expr("selectoffset1", "rint($T * 6)");
Node window = b.sop(geo, "grouprange", "window")
                  .input(0, grid)
                  .set("groupname1", "window").set("grouptype1", "prims")
                  .set("method1", "length")
                  .expr("start1", "rint(60 + 60 * sin($T * 0.5))")
                  .expr("length1", "rint(14 + 10 * sin($T * 1.3))");
```

![Group Range](images/grouprange.png)

### Deformers

#### Transform

`xform` — translate, rotate, scale, shear, pivot and pre-transform of the whole geometry or of a group; vectors,
normals and other attributes are transformed by their type.

```cpp
Node spin = b.sop(geo, "xform", "spin_bounce_squash")
                .input(0, torus)
                .expr("t", {"0", "abs(sin($T * 3)) * 1.5", "0"})
                .expr("r", {"$T * 60", "$T * 25", "0"})
                .expr("s", {"1 + 0.25 * sin($T * 6)", "1 - 0.25 * sin($T * 6)", "1"});
Node shear = b.sop(geo, "xform", "shear").input(0, box).expr("shear", {"0.8 * sin($T * 1.5)", "0", "0.4 * sin($T)"});
// Only the first 121 points of a grid are lifted
Node lift = b.sop(geo, "xform", "lift_half")
                .input(0, grid)
                .set("group", "0-120").set("grouptype", "points")
                .expr("t", 1, "1.2 * sin($T * 1.5)");
```

![Transform](images/xform.png)

#### Mountain

`mountain` — displacement along the normal by fractal noise (Perlin, simplex, Worley, alligator, ...) that can flow in
time.

```cpp
Node land = b.sop(geo, "mountain", "terrain")
                .input(0, grid)
                .set("elementsize", 3)
                .set("oct", 6)
                .expr("height", "1.5 + 0.7 * sin($T * 0.5)")
                .expr("offset", {"$T * 0.4", "0", "$T * 0.2"});
Node rock = b.sop(geo, "mountain", "rock")
                .input(0, sphere)
                .set("basis", "worleyFA")
                .set("fractal", "none")
                .expr("height", "0.6 + 0.4 * sin($T)")
                .expr("time", "$T * 0.3");
```

![Mountain](images/mountain.png)

#### Bend

`bend` — bending, twisting, tapering and squashing inside a capture region; bending by angle or towards a goal
direction.

```cpp
Node bend = b.sop(geo, "bend", "bend_twist")
                .input(0, column)
                .set("origin", 0, 0, 0).set("dir", 0, 1, 0).set("length", 6)
                .set("upvectorcontrol", "xaxis")
                .expr("bend", "150 * sin($T * 0.8)")
                .set("enabletwist", 1)
                .expr("twist", "180 * sin($T * 0.6)");
Node taper = b.sop(geo, "bend", "bend_taper")
                 .input(0, column2)
                 .set("bendmode", "dir")
                 .expr("goaldir", {"sin($T)", "0.3", "cos($T)"})
                 .set("enabletaper", 1)
                 .expr("taper", "0.6 + 0.5 * sin($T * 1.4)");
```

![Bend](images/bend.png)

#### Path Deform

`pathdeform` — geometry laid along a curve from the second input, with ramps of taper and roll, offsets and stretching.

```cpp
Node path = b.sop(geo, "fit", "wave_path").input(0, wavePoints).set("method", 1).set("type", "nurbs");
Node snake = b.sop(geo, "pathdeform", "snake")
                 .input(0, column)
                 .input(1, path)
                 .set("curve_endunit", "fractioncurvelength")
                 .set("taper_enable", 1).set("taper_enableramp", 1)
                 .set("taper_ramp", 3)                       // three ramp keys
                 .set("taper_ramp3pos", 0.35).set("taper_ramp3value", 1.6)
                 .set("rotate_enable", 1)
                 .expr("rotate_angle", "240 * sin($T * 0.7)");
Node band = b.sop(geo, "pathdeform", "band")
                .input(0, strip)
                .input(1, circle)
                .expr("curve_posend", "0.55 + 0.35 * sin($T * 0.9)")
                .expr("curve_posoffset", "$T * 0.06");
```

![Path Deform](images/pathdeform.png)

#### Match Size

`matchsize` — fits the geometry into a target box (explicit, the second input or the origin): justify and scale per
axis, uniformly or by perimeter, area or volume.

```cpp
Node fit = b.sop(geo, "matchsize", "fit_torus")
               .input(0, torus)
               .set("justifytarget", "explicit")
               .expr("size", {"2 + 0.8 * sin($T * 1.3)", "2 + 0.8 * sin($T * 1.3 + 2)", "2"})
               .set("doscale", 1)
               .set("uniformscale", 0);
// The target box drawn as a wire frame reads the size of Match Size with ch()
Node frame = b.sop(geo, "box", "frame").set("surftype", "rowcol")
                 .expr("size", {"ch(\"../fit_torus/sizex\")", "ch(\"../fit_torus/sizey\")", "ch(\"../fit_torus/sizez\")"});
```

![Match Size](images/matchsize.png)

#### Fuse

`fuse` — merges points within a distance or snaps them to a grid; degenerate faces are removed, attributes are merged
by a chosen method.

```cpp
Node snap = b.sop(geo, "fuse", "snap_to_grid")
                .input(0, sphere)
                .set("snaptype", "gridsnap")
                .expr("gridspacing", {"0.15 + 0.25 * (1 + sin($T * 0.8))", "ch(\"gridspacingx\")", "ch(\"gridspacingx\")"});
Node fuse = b.sop(geo, "fuse", "fuse_distance")
                .input(0, torus)
                .expr("tol3d", "0.02 + 0.35 * (0.5 + 0.5 * sin($T * 0.6))");
```

![Fuse](images/fuse.png)

### Modeling

#### Clip

`clip` — cuts polygons and polylines by a plane, keeps the part above, below or both, and closes the cut with a cap.

```cpp
Node cut = b.sop(geo, "clip", "cut_with_cap")
               .input(0, sphere)
               .set("dofill", 1)                                  // cap the cut
               .expr("dir", {"sin($T * 0.7)", "0.6", "cos($T * 0.7)"})
               .expr("dist", "1.2 * sin($T * 1.1)");
Node below = b.sop(geo, "clip", "keep_below")
                 .input(0, box)
                 .set("clipop", "below")
                 .set("dir", 0.3, 1, 0.2)
                 .expr("dist", "1.8 * sin($T * 0.9)");
```

![Clip](images/clip.png)

#### PolyExtrude

`polyextrude` — extrudes faces (together or as individual elements) and open curves: distance, inset, divisions,
twist, front transform, thickness and twist ramps, extrusion along a curve, UVs of the sides.

```cpp
Node tiles = b.sop(geo, "polyextrude", "tiles")
                 .input(0, sphereWithGroup)
                 .set("group", "tiles")
                 .set("splittype", "elements")
                 .expr("dist", "0.35 + 0.3 * sin($T * 1.5)")
                 .expr("inset", "0.06 + 0.05 * sin($T * 2)");
Node tower = b.sop(geo, "polyextrude", "tower")
                 .input(0, box)
                 .set("group", "2")                                 // the top face
                 .set("divs", 16)
                 .expr("twist", "180 * sin($T * 0.6)")
                 .set("xformfront", 1)
                 .expr("scale", {"0.5 + 0.3 * sin($T * 1.2)", "1", "0.5 + 0.3 * sin($T * 1.2)"});
Node ribbon = b.sop(geo, "polyextrude", "ribbon").input(0, openArc).expr("dist", "0.8 + 0.4 * sin($T * 2)");
```

![PolyExtrude](images/polyextrude.png)

#### PolyBevel

`polybevel` — bevels edges and points: round, chamfer, crease, solid profiles, divisions, profile ramps and curves,
collision handling.

```cpp
Node round = b.sop(geo, "polybevel", "round")
                 .input(0, box)
                 .expr("offset", "0.1 + 0.45 * (0.5 + 0.5 * sin($T))")
                 .expr("divisions", "rint(1 + 4 * (0.5 + 0.5 * sin($T * 0.5)))");
Node chamfer = b.sop(geo, "polybevel", "chamfer")
                   .input(0, box2)
                   .set("filletshape", "chamfer")
                   .expr("offset", "0.05 + 0.5 * (0.5 + 0.5 * sin($T * 1.3))");
```

![PolyBevel](images/polybevel.png)

#### Boolean

`boolean` — union, intersection, subtraction and shattering of solids and surfaces (the kernel is Manifold);
attributes and groups of both inputs carry over to the result.

```cpp
Node subtract = b.sop(geo, "boolean", "subtract").input(0, box).input(1, sphere).set("booleanop", "subtract");
Node unite = b.sop(geo, "boolean", "union").input(0, torus).input(1, bar).set("booleanop", "union");
Node common = b.sop(geo, "boolean", "intersect").input(0, ball).input(1, cube).set("booleanop", "intersect");
```

![Boolean](images/boolean.png)

### Copies and points

#### Scatter

`scatter` — points over the surface by count or density (also from an attribute), relaxed apart; source primitive and
its parametric coordinates are written to the points.

```cpp
Node points = b.sop(geo, "scatter", "points")
                  .input(0, torus)
                  .expr("npts", "rint(60 + 540 * (0.5 + 0.5 * sin($T * 0.6)))")
                  .expr("seed", "rint($T / 2)");
Node dot = b.sop(geo, "sphere", "dot").set("type", "poly").set("freq", 1).set("rad", 0.12, 0.12, 0.12);
Node dots = b.sop(geo, "copytopoints", "dots").input(0, dot).input(1, points);
```

![Scatter](images/scatter.png)

#### Copy to Points

`copytopoints` — copies of the first input onto the points of the second, oriented by `N`, `up`, `orient`, scaled by
`pscale`, with attributes from the target points.

```cpp
Node normals = b.sop(geo, "normal", "normals").input(0, wavingGrid).set("type", "typepoint");
Node sizes = b.sop(geo, "attribrandomize", "sizes").input(0, normals).set("name", "pscale").set("dimensions", 1);
Node colors = b.sop(geo, "attribrandomize", "colors").input(0, sizes).set("name", "Cd");
Node brick = b.sop(geo, "box", "brick").set("size", 0.3, 0.3, 0.6).set("t", 0, 0, 0.3);
b.sop(geo, "copytopoints", "bricks")
    .input(0, brick)
    .input(1, colors)
    .set("pivot", "origin")
    .set("targetattribs", 1)              // one row of Attributes from Target...
    .set("applyattribs1", "Cd")           // ...copying Cd of the target points
    .display();
```

![Copy to Points](images/copytopoints.png)

#### Copy and Transform

`copyxform` — N copies, copy k transformed by the parameters taken k times (translate, rotate, scale, pivot).

```cpp
Node stairs = b.sop(geo, "copyxform", "stairs")
                  .input(0, step)
                  .expr("ncy", "rint(12 + 18 * (0.5 + 0.5 * sin($T * 0.5)))")
                  .set("t", 0, 0.22, 0)
                  .expr("r", 1, "18 + 6 * sin($T * 0.8)");
Node tower = b.sop(geo, "copyxform", "tower")
                 .input(0, torus)
                 .set("ncy", 10)
                 .set("t", 0, 0.65, 0)
                 .expr("scale", "0.88 + 0.06 * sin($T * 1.5)");   // copy k is scaled by scale^k
```

![Copy and Transform](images/copyxform.png)

### Curves

#### Fit

`fit` — NURBS or Bezier curves through points: global and local interpolation, breakpoints, approximation within a
tolerance.

```cpp
Node interp = b.sop(geo, "fit", "interpolate").input(0, polyline).set("method", "interp");
Node approx = b.sop(geo, "fit", "approximate")
                  .input(0, polyline)
                  .set("type", "bezier")
                  .expr("tol", "0.05 + 0.5 * (0.5 + 0.5 * sin($T * 0.7))");
```

![Fit](images/fit.png)

#### Resample

`resample` — evenly spaced points along polylines and curves, by segment length or count; polylines can be treated as
subdivision curves.

```cpp
Node byLength = b.sop(geo, "resample", "by_length")
                    .input(0, nurbsCircle)
                    .expr("length", "0.2 + 0.9 * (0.5 + 0.5 * sin($T * 0.7))");
Node smooth = b.sop(geo, "resample", "smooth")
                  .input(0, zigzag)
                  .set("treatpolysas", "subd")
                  .expr("length", "0.05 + 0.3 * (0.5 + 0.5 * sin($T * 0.9))");
```

![Resample](images/resample.png)

#### Carve

`carve` — cuts pieces of curves between parametric positions (or by arc length) and extracts points.

```cpp
Node grow = b.sop(geo, "carve", "grow")
                .input(0, nurbsCircle)
                .set("firstu", 1).set("domainu1", 0)
                .set("secondu", 1)
                .expr("domainu2", "0.03 + 0.97 * (0.5 + 0.5 * sin($T * 0.8))");
Node piece = b.sop(geo, "carve", "running_piece")
                 .input(0, spiral)
                 .set("firstu", 1).expr("domainu1", "0.4 + 0.4 * sin($T * 0.7)")
                 .set("secondu", 1).expr("domainu2", "ch(\"domainu1\") + 0.18");
Node ribbon = b.sop(geo, "polyextrude", "grow_ribbon").input(0, grow).set("dist", 0.6);
```

![Carve](images/carve.png)

#### Join

`join` — joins curves end to start into one, with a smooth blend or a straight span; nearest ends, groups of N, wrap.

```cpp
Node arcs = merge(b, geo, {arc1, arc2}, "arcs");
Node blended = b.sop(geo, "join", "blend")
                   .input(0, arcs)
                   .set("blend", 1)
                   .expr("tolerance", "0.2 + 0.6 * (0.5 + 0.5 * sin($T * 0.9))")
                   .expr("bias", "0.5 + 0.4 * sin($T * 0.5)");
Node joined = b.sop(geo, "join", "connect").input(0, merge(b, geo, {line1, line2}, "lines")).set("blend", 0);
```

![Join](images/join.png)

#### Orient along Curve

`orientalongcurve` — a frame at every point of a curve (tangent, up, roll and twists) written as `N` / `up`,
`orient` or matrices — ready for Copy to Points.

```cpp
Node points = b.sop(geo, "resample", "points").input(0, nurbsCircle).set("length", 0.35);
Node frames = b.sop(geo, "orientalongcurve", "frames")
                  .input(0, points)
                  .set("upvectortype", "y")
                  .set("applyroll", 1)
                  .expr("roll", "$T * 45")
                  .set("rollper", "fulledges")
                  .expr("fulltwists", "rint(1 + sin($T * 0.4))");
Node plate = b.sop(geo, "box", "plate").set("size", 0.08, 1.4, 0.3);
b.sop(geo, "copytopoints", "plates").input(0, plate).input(1, frames).display();
```

![Orient along Curve](images/orientalongcurve.png)

### Networks and objects

#### Merge

`merge` — geometry of up to 10 inputs combined in input order; attributes and groups of all inputs are united.

```cpp
const char* ops[] = {"sphere", "box", "torus", "circle", "grid"};
Node all = b.sop(geo, "merge", "all");
for (int k = 0; k < 5; ++k) {
    Node part = b.sop(geo, ops[k], ops[k])
                    .expr("t", {fmt("3.5 * cos($T * 0.6 + %d * 1.2566)", k).c_str(), "0",
                                fmt("3.5 * sin($T * 0.6 + %d * 1.2566)", k).c_str()});
    all.input(k, part);
}
all.display();
```

![Merge](images/merge.png)

#### Null

`null` — passes its input through. Used as a control node with user (spare) parameters that other nodes read with
`ch()`, and as the named end of a chain.

```cpp
Node ctrl = b.sop(geo, "null", "CTRL")
                .addFloatParm("radius", "Radius", 2, 0.1, 5)          // gnAddSpareParm
                .addFloatParm("height", "Noise Height", 0.5, 0, 2)
                .expr("radius", "2 + 0.4 * sin($T * 1.3)");
Node sphere = b.sop(geo, "sphere", "ball")
                  .set("type", "polymesh")
                  .expr("rad", {"ch(\"../CTRL/radius\")", "ch(\"../CTRL/radius\")", "ch(\"../CTRL/radius\")"});
Node bumps = b.sop(geo, "mountain", "bumps").input(0, sphere).expr("height", "ch(\"../CTRL/height\")");
b.sop(geo, "null", "OUT").input(0, bumps).display();
// Another object reads the same control by an absolute path
Node ring = b.sop(ringGeo, "torus", "ring").expr("rad", {"ch(\"/obj/shapes/CTRL/radius\") * 1.6", "0.2"});
```

![Null](images/null.png)

#### Subnet

`subnet` — a SOP network inside a SOP: up to 4 inputs come in through the input nodes "1".."4" inside it, the result
goes out through its Display SOP or Output SOPs.

```cpp
Node subnet = b.sop(geo, "subnet", "shape_both").input(0, torus).input(1, grid);
Node in1 = b.subnetInput(subnet, 1);     // the input nodes: gnComposeChildNodeList with GN_NODEFLAGS_SOP_SUBNET_INPUT
Node in2 = b.subnetInput(subnet, 2);
Node spin = b.sop(subnet, "xform", "spin").input(0, in1).expr("r", {"$T * 50", "$T * 30", "0"});
Node waves = b.sop(subnet, "mountain", "waves").input(0, in2).expr("time", "$T * 0.5");
merge(b, subnet, {spin, waves}, "result").display();   // the Display SOP inside is the subnet's output
subnet.display();
```

![Subnet](images/subnet.png)

#### Output

`output` — chooses which geometry goes to which output of a subnet (Output Index 0, 1, ...).

```cpp
Node split = b.sop(geo, "subnet", "split").input(0, sphere);
Node in = b.subnetInput(split, 1);
Node top = b.sop(split, "clip", "top").input(0, in).set("dofill", 1);
Node bottom = b.sop(split, "clip", "bottom").input(0, in).set("dofill", 1).set("clipop", "below");
b.sop(split, "output", "output0").input(0, top).set("outputidx", 0);
b.sop(split, "output", "output1").input(0, bottom).set("outputidx", 1);
// Outside: output 0 and output 1 of the subnet
Node up = b.sop(geo, "xform", "move_up").input(0, split, 0).expr("t", 1, "0.5 + 0.4 * sin($T * 1.5)");
Node down = b.sop(geo, "xform", "move_down").input(0, split, 1).expr("t", 1, "-0.5 - 0.4 * sin($T * 1.5)");
```

![Output](images/output.png)

#### Geo objects

`Object/geo` — an object on `/obj` holding a SOP network, with its own transform (translate, rotate, scale, pivot,
transform order). Geometry stays in object space; `gnGetObjectTransform` gives the world matrix.

```cpp
Node spinner = b.object("spinner");                        // gnCreateNode(s, 0, "geo", ...)
b.sop(spinner, "box", "shape").display();
spinner.set("t", -4.5, 0, 0).expr("r", {"$T * 40", "$T * 70", "20"}).set("scale", 1.5);

Node squasher = b.object("squasher");
b.sop(squasher, "torus", "shape").display();
squasher.set("xOrd", "trs").expr("s", {"1 + 0.4 * sin($T * 2)", "1", "1 - 0.4 * sin($T * 2)"});

// In the app: the world matrix of an object for the renderer
GN_Transform xf;
gnGetObjectTransform(s, spinner.id(), -1, GN_SRT, &xf);
float matrix[16];
gnConvertTransformQuatToMatrix(s, &xf, matrix);
```

![Geo objects](images/object_geo.png)

#### Null objects

`Object/null` — objects without geometry used as parents: input 0 of an object is its parent, the world matrix of a
child is its local matrix times the parent's.

```cpp
Node tilt = b.object("tilt", "null").expr("r", 2, "15 * sin($T * 0.5)");
Node orbit = b.object("planet_orbit", "null").input(0, tilt).expr("r", 1, "$T * 40");
Node planet = b.object("planet").input(0, orbit).set("t", 5, 0, 0).expr("r", 1, "$T * 90");
b.sop(planet, "sphere", "body").set("type", "polymesh").set("rad", 0.6, 0.6, 0.6).display();
Node moonOrbit = b.object("moon_orbit", "null").input(0, planet).expr("r", 1, "$T * 160");
Node moon = b.object("moon").input(0, moonOrbit).set("t", 1.6, 0, 0);
b.sop(moon, "sphere", "body").set("type", "poly").set("rad", 0.2, 0.2, 0.2).display();
```

![Null objects](images/object_null.png)

### Geometry from the application

#### Input nodes

`gnCreateInputNode` and the geometry setters — a mesh and a curve computed by the application and handed to the graph,
as the input nodes of HAPI. Here a star column with point colors, a string primitive attribute and a point group, and a
trefoil knot as a closed NURBS curve; procedural nodes then work on them.

```cpp
// A mesh: points, attributes, faces and a group
GN_NodeId star;
gnCreateInputNode(s, -1, &star, "star");                  // /obj/input_star/input_star
GN_PartInfo part{};
part.type = GN_PARTTYPE_MESH;
part.pointCount = pointCount; part.vertexCount = vertexCount; part.faceCount = faceCount;
gnSetPartInfo(s, star, 0, &part);

GN_AttributeInfo p{};
p.exists = true; p.owner = GN_ATTROWNER_POINT; p.storage = GN_STORAGETYPE_FLOAT;
p.count = pointCount; p.tupleSize = 3; p.typeInfo = HAPI_ATTRIBUTE_TYPE_POINT;
gnAddAttribute(s, star, 0, "P", &p);
gnSetAttributeFloatData(s, star, 0, "P", &p, positions.data(), 0, pointCount);
// ... the same for Cd (HAPI_ATTRIBUTE_TYPE_COLOR) and a string attribute with gnSetAttributeStringData
gnSetVertexList(s, star, 0, vertices.data(), 0, vertexCount);
gnSetFaceCounts(s, star, 0, faceCounts.data(), 0, faceCount);
gnAddGroup(s, star, 0, GN_GROUPTYPE_POINT, "crown");
gnSetGroupMembership(s, star, 0, GN_GROUPTYPE_POINT, "crown", crown.data(), 0, pointCount);
gnCommitGeo(s, star);                                     // the node is locked with this geometry

// A curve: a closed NURBS curve of order 4 through 24 CVs
GN_NodeId knot;
gnCreateInputNode(s, -1, &knot, "knot");
GN_PartInfo curvePart{};
curvePart.type = GN_PARTTYPE_CURVE;
curvePart.pointCount = 24; curvePart.vertexCount = 24; curvePart.faceCount = 1;
gnSetPartInfo(s, knot, 0, &curvePart);
GN_CurveInfo curve{};
curve.curveType = GN_CURVETYPE_NURBS; curve.curveCount = 1; curve.vertexCount = 24;
curve.order = 4; curve.isClosed = true;
gnSetCurveInfo(s, knot, 0, &curve);
const int cvs = 24;
gnSetCurveCounts(s, knot, 0, &cvs, 0, 1);
// ... P as above, then gnCommitGeo(s, knot)

// Procedural nodes on the input: beads on the points of the "crown" group, beads along the resampled knot
Node crown = b.sop(starGeo, "copytopoints", "crown").input(0, bead).input(1, Node(&b, star)).set("targetgroup", "crown");
Node points = b.sop(knotGeo, "resample", "points").input(0, Node(&b, knot)).expr("length", "0.35 + 0.15 * sin($T * 1.3)");
```

![Input nodes](images/input.png)

#### Input nodes and Boolean

Two solids made in code — a star prism and a torus — are input nodes inside one geo object. The torus is moved by a
Transform SOP and the Boolean operator combines them every frame; the colors of both inputs carry over into the
results.

```cpp
Node geo = b.object("input_boolean");
GN_NodeId starId, ringId;
gnCreateInputNode(s, geo.id(), &starId, "star");          // inside the object: /obj/input_boolean/input_star
gnCreateInputNode(s, geo.id(), &ringId, "ring");
commitMesh(starId, starPositions, starColors, starVertices, starFaceCounts);   // the setters and gnCommitGeo
commitMesh(ringId, ringPositions, ringColors, ringVertices, ringFaceCounts);

Node moving = b.sop(geo, "xform", "move_b")
                  .input(0, Node(&b, ringId))
                  .expr("t", {"0.9 * sin($T * 0.7)", "0.9 + 0.55 * sin($T * 1.1)", "0.3 * cos($T * 0.9)"})
                  .expr("r", {"90 + $T * 35", "$T * 20", nullptr});
for (const char* op : {"union", "intersect", "subtract"})
    b.sop(geo, "boolean", op).input(0, Node(&b, starId)).input(1, moving).set("booleanop", op);
```

![Input nodes and Boolean](images/input_boolean.png)
