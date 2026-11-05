// Attributes: Attribute Create, Attribute Randomize, Attribute Blur, Normal, UV Texture
#include "demo_scenes.h"
#include "scene_util.h"

namespace demo::scenes {

// Attribute Create: an attribute per instance of the "numattr" multiparm (name#, class#, type#, size#, value#v).
// A sphere gets a point color that changes with time and a second attribute (a float "mask", unused by the
// renderer) in the same node; a moving spot — the points of a Group Create group — is recolored by an Attribute Create
// limited to the group; a box gets primitive colors by a Group Range checkerboard
void sceneAttribCreate(SceneBuilder& b)
{
    Node geo = b.object("attribcreate");

    Node sphere =
        b.sop(geo, "sphere", "ball").set("type", "polymesh").set("rows", 32).set("cols", 64).set("rad", 2, 2, 2);
    Node base = b.sop(geo, "attribcreate", "base_color")
                    .input(0, sphere)
                    .set("numattr", 2)
                    .set("name1", "Cd")
                    .set("class1", "point")
                    .set("type1", "vector")
                    .set("size1", 3)
                    .expr("value1v",
                        {"0.3 + 0.2 * sin($T)", "0.4 + 0.2 * sin($T * 1.3 + 2)", "0.7 + 0.2 * sin($T * 0.7 + 4)"})
                    .set("name2", "mask")
                    .set("class2", "point")
                    .set("type2", "float")
                    .set("size2", 1)
                    .expr("value2v", 0, "0.5 + 0.5 * sin($T)");
    Node spot = b.sop(geo, "groupcreate", "spot")
                    .input(0, base)
                    .set("groupname", "spot")
                    .set("grouptype", "point")
                    .set("groupbase", 0)
                    .set("groupbounding", 1)
                    .set("boundtype", "usebsphere")
                    .expr("size", {"2.4 + 0.6 * sin($T * 2)", "ch(\"sizex\")", "ch(\"sizex\")"})
                    // The spot sweeps the half of the sphere that faces the camera
                    .expr("t", {"2.2 * cos(0.785 + 1.2 * sin($T * 0.8))", "1.2 * sin($T * 1.1)",
                                   "2.2 * sin(0.785 + 1.2 * sin($T * 0.8))"});
    Node spotColor = b.sop(geo, "attribcreate", "spot_color")
                         .input(0, spot)
                         .set("group", "spot")
                         .set("grouptype", "points")
                         .set("name1", "Cd")
                         .set("class1", "point")
                         .set("type1", "vector")
                         .set("size1", 3)
                         .expr("value1v", {"1", "0.8 + 0.2 * sin($T * 5)", "0.2"});

    spotColor.display();

    // Primitive colors: every second face of the box. A separate object: a merge with the sphere would give the box
    // the point Cd of the sphere (default black), which wins over the primitive one
    Node boxGeo = b.object("attribcreate_prims");
    Node box = b.sop(boxGeo, "box", "cube")
                   .set("size", 2.5, 2.5, 2.5)
                   .set("divrate", 5, 5, 5)
                   .set("t", 5, 0, 0)
                   .expr("r", 1, "$T * 25");
    Node boxBase = primColor(b, boxGeo, box, 0.25, 0.3, 0.4);
    Node checker = b.sop(boxGeo, "grouprange", "every_second")
                       .input(0, boxBase)
                       .set("groupname1", "odd")
                       .set("grouptype1", "prims")
                       .set("selectamount1", 1)
                       .set("selecttotal1", 2)
                       .expr("selectoffset1", "rint($T * 2)");
    b.sop(boxGeo, "attribcreate", "odd_color")
        .input(0, checker)
        .set("group", "odd")
        .set("grouptype", "prims")
        .set("name1", "Cd")
        .set("class1", "primitive")
        .set("type1", "vector")
        .set("size1", 3)
        .expr("value1v", {"0.95", "0.5 + 0.3 * sin($T)", "0.3"})
        .display();
}

// Attribute Randomize: random primitive colors of a grid with the seed stepping twice a second, and random point
// sizes (pscale) and colors of points scattered over a torus — small spheres are copied onto them
void sceneAttribRandomize(SceneBuilder& b)
{
    Node geo = b.object("randomize");

    Node grid = b.sop(geo, "grid", "mosaic").set("size", 8, 8).set("rows", 16).set("cols", 16).set("t", -5, 0, 0);
    b.sop(geo, "attribrandomize", "mosaic_colors")
        .input(0, grid)
        .set("class", "primitive")
        .set("name", "Cd")
        .expr("seed", "rint($T * 2)")
        .expr("min", {"0.1", "0.2 + 0.2 * sin($T)", "0.4", nullptr})
        .set("max", 1, 0.8, 1, 1)
        .display();

    // Point colors (copied from the scattered points) — a separate object, not to mix with primitive colors
    Node dotsGeo = b.object("randomize_points");
    Node torus = b.sop(dotsGeo, "torus", "ring").set("rad", 2.2, 0.6).set("t", 4, 0, 0).expr("r", 0, "$T * 15");
    Node points = b.sop(dotsGeo, "scatter", "points").input(0, torus).set("npts", 250).set("relaxpoints", 0);
    Node sizes = b.sop(dotsGeo, "attribrandomize", "sizes")
                     .input(0, points)
                     .set("name", "pscale")
                     .set("dimensions", 1)
                     .expr("seed", "rint($T * 3)")
                     .set("min", 0.04, 0, 0, 0)
                     .expr("max", 0, "0.12 + 0.08 * sin($T * 2)");
    Node colors =
        b.sop(dotsGeo, "attribrandomize", "colors").input(0, sizes).set("name", "Cd").expr("seed", "rint($T * 3)");
    Node dot = b.sop(dotsGeo, "sphere", "dot").set("type", "poly").set("freq", 2);
    b.sop(dotsGeo, "copytopoints", "copies")
        .input(0, dot)
        .input(1, colors)
        .set("targetattribs", 1)
        .set("applyattribs1", "Cd")
        .display();
}

// Attribute Blur: random point colors blurred by an animated number of iterations, and the positions of a noisy
// terrain (Attribute = P) smoothed by it
void sceneAttribBlur(SceneBuilder& b)
{
    Node geo = b.object("blur");

    Node grid = b.sop(geo, "grid", "color_grid").set("size", 8, 8).set("rows", 60).set("cols", 60).set("t", -4.5, 0, 0);
    Node noise = b.sop(geo, "attribrandomize", "noise_colors").input(0, grid).set("name", "Cd").set("seed", 3);
    Node colorBlur = b.sop(geo, "attribblur", "blur_colors")
                         .input(0, noise)
                         .set("attributes", "Cd")
                         .expr("iterations", "rint(12 + 12 * sin($T * 0.8))");

    Node terrain =
        b.sop(geo, "grid", "terrain_grid").set("size", 8, 8).set("rows", 60).set("cols", 60).set("t", 4.5, 0, 0);
    Node rough = b.sop(geo, "mountain", "rough")
                     .input(0, terrain)
                     .set("height", 1.2)
                     .set("elementsize", 1.2)
                     .set("rough", 0.8)
                     .set("oct", 6);
    Node smooth = b.sop(geo, "attribblur", "smooth_terrain")
                      .input(0, rough)
                      .set("attributes", "P")
                      .set("mode", "volpreserving")
                      .expr("iterations", "rint(15 + 15 * sin($T * 0.6))");
    Node terrainColor = color(b, geo, smooth, 0.55, 0.7, 0.4);

    merge(b, geo, {colorBlur, terrainColor}).display();
}

// Normal: point normals of a sphere deformed by moving noise; Copy to Points turns +Z of every spike along N, so the
// spikes show the normals following the surface
void sceneNormal(SceneBuilder& b)
{
    Node geo = b.object("normals");

    Node sphere =
        b.sop(geo, "sphere", "ball").set("type", "polymesh").set("rows", 18).set("cols", 36).set("rad", 2.2, 2.2, 2.2);
    Node bumps = b.sop(geo, "mountain", "bumps")
                     .input(0, sphere)
                     .set("elementsize", 1.5)
                     .expr("height", "0.6 + 0.3 * sin($T)")
                     .expr("time", "$T * 0.4");
    Node normals = b.sop(geo, "normal", "point_normals").input(0, bumps).set("type", "typepoint").set("method", "area");
    Node surface = color(b, geo, normals, 0.35, 0.45, 0.6);

    Node spike = b.sop(geo, "box", "spike").set("size", 0.05, 0.05, 0.6).set("t", 0, 0, 0.3).set("divrate", 2, 2, 2);
    Node spikes = b.sop(geo, "copytopoints", "spikes").input(0, spike).input(1, normals).set("pivot", "origin");
    Node spikeColor = color(b, geo, spikes, 1, 0.8, 0.3);

    merge(b, geo, {surface, spikeColor}).display();
}

// UV Texture: projections written into Cd instead of uv (the UV Attribute parameter), so the coordinates are seen as
// colors: polar on a sphere with a moving offset, cylindrical on a torus with an animated scale, rows and columns on a
// grid with a sliding window of the coordinates
void sceneTexture(SceneBuilder& b)
{
    Node geo = b.object("texture");

    Node sphere = b.sop(geo, "sphere", "ball")
                      .set("type", "polymesh")
                      .set("rows", 24)
                      .set("cols", 48)
                      .set("rad", 1.8, 1.8, 1.8)
                      .set("t", -4.5, 0, 0);
    Node polar = b.sop(geo, "texture", "polar")
                     .input(0, sphere)
                     .set("uvattrib", "Cd")
                     .set("type", "polar")
                     .expr("offset", {"0.3 * sin($T)", "0.3 * cos($T * 0.7)", "0"});

    Node torus =
        b.sop(geo, "torus", "ring").set("rad", 1.6, 0.6).set("rows", 24).set("cols", 48).expr("r", 0, "$T * 20");
    Node cylin = b.sop(geo, "texture", "cylindrical")
                     .input(0, torus)
                     .set("uvattrib", "Cd")
                     .set("type", "cylin")
                     .expr("s", {"0.7 + 0.3 * sin($T)", "0.7 + 0.3 * cos($T * 1.3)", "1"});

    Node grid = b.sop(geo, "grid", "panel")
                    .set("size", 3.5, 3.5)
                    .set("rows", 12)
                    .set("cols", 12)
                    .set("orient", "xy")
                    .set("t", 4.5, 0, 0);
    Node rowcol = b.sop(geo, "texture", "rows_columns")
                      .input(0, grid)
                      .set("uvattrib", "Cd")
                      .set("type", "rowcol")
                      // uv · scale + offset stays within 0..1: a window of the coordinates slides over them
                      .expr("s", {"0.6 + 0.4 * sin($T)", "0.6 + 0.4 * cos($T * 0.8)", "1"})
                      .expr("offset", {"(1 - ch(\"sx\")) * (0.5 + 0.5 * sin($T * 1.7))",
                                          "(1 - ch(\"sy\")) * (0.5 + 0.5 * cos($T * 1.3))", "0"});

    merge(b, geo, {polar, cylin, rowcol}).display();
}

} // namespace demo::scenes
