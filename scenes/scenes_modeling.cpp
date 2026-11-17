// Modeling: Clip, PolyExtrude, PolyBevel, Boolean, Voronoi Fracture, Skin, Subdivide
#include "demo_scenes.h"
#include "scene_util.h"

namespace demo::scenes {

// Clip: a sphere cut by a plane whose normal turns and which moves along it, the cut closed by a cap (Fill), and a
// divided box of which only the part below a sweeping plane is kept
void sceneClip(SceneBuilder& b)
{
    Node geo = b.object("clip");

    Node sphere = b.sop(geo, "sphere", "ball")
                      .set("type", "polymesh")
                      .set("rows", 32)
                      .set("cols", 64)
                      .set("rad", 2.2, 2.2, 2.2)
                      .set("t", -3, 0, 0);
    Node sphereColor = color(b, geo, sphere, 0.4, 0.65, 0.95);
    Node cut = b.sop(geo, "clip", "cut_with_cap")
                   .input(0, sphereColor)
                   .set("dofill", 1)
                   .set("origin", -3, 0, 0)
                   .expr("dir", {"sin($T * 0.7)", "0.6", "cos($T * 0.7)"})
                   .expr("dist", "1.2 * sin($T * 1.1)");

    Node box = b.sop(geo, "box", "block")
                   .set("size", 3, 3, 3)
                   .set("divrate", 8, 8, 8)
                   .set("t", 3, 0, 0)
                   .expr("r", 1, "$T * 20");
    Node boxColor = color(b, geo, box, 0.95, 0.65, 0.3);
    Node below = b.sop(geo, "clip", "keep_below")
                     .input(0, boxColor)
                     .set("clipop", "below")
                     .set("origin", 3, 0, 0)
                     .set("dir", 0.3, 1, 0.2)
                     .expr("dist", "1.8 * sin($T * 0.9)");

    merge(b, geo, {cut, below}).display();
}

// PolyExtrude: every other face of a sphere (Group Range) extruded as individual elements with inset and an animated
// distance; the top face of a box extruded with divisions, twist and a Front Transform scale — a twisted tower; an
// open polyline (a circle arc) extruded into a ribbon
void scenePolyExtrude(SceneBuilder& b)
{
    Node geo = b.object("polyextrude");

    Node sphere = b.sop(geo, "sphere", "ball")
                      .set("type", "polymesh")
                      .set("rows", 12)
                      .set("cols", 24)
                      .set("rad", 1.8, 1.8, 1.8)
                      .set("t", -4.5, 0, 0);
    Node sphereColor = color(b, geo, sphere, 0.4, 0.6, 0.9);
    Node every = b.sop(geo, "grouprange", "every_other")
                     .input(0, sphereColor)
                     .set("groupname1", "tiles")
                     .set("grouptype1", "prims")
                     .set("selectamount1", 1)
                     .set("selecttotal1", 2);
    Node tiles = b.sop(geo, "polyextrude", "tiles")
                     .input(0, every)
                     .set("group", "tiles")
                     .set("splittype", "elements")
                     .expr("dist", "0.35 + 0.3 * sin($T * 1.5)")
                     .expr("inset", "0.06 + 0.05 * sin($T * 2)");

    // Box with one face per side: primitive 2 is the +Y face
    Node box = b.sop(geo, "box", "base").set("size", 1.6, 0.4, 1.6).set("divrate", 2, 2, 2).set("t", 0, -2, 0);
    Node boxColor = color(b, geo, box, 0.95, 0.6, 0.3);
    Node tower = b.sop(geo, "polyextrude", "tower")
                     .input(0, boxColor)
                     .set("group", "2")
                     .set("divs", 16)
                     .expr("dist", "3.5 + 0.8 * sin($T)")
                     .expr("twist", "180 * sin($T * 0.6)")
                     .set("xformfront", 1)
                     .expr("scale", {"0.5 + 0.3 * sin($T * 1.2)", "1", "0.5 + 0.3 * sin($T * 1.2)"});

    Node arc = b.sop(geo, "circle", "arc")
                   .set("arc", "openarc")
                   .set("divs", 40)
                   .set("orient", "zx")
                   .set("rad", 2, 2)
                   .set("t", 4.5, 0, 0)
                   .expr("angle", {"0", "200 + 140 * sin($T * 0.7)"});
    Node arcColor = color(b, geo, arc, 0.5, 0.9, 0.5);
    Node ribbon = b.sop(geo, "polyextrude", "ribbon").input(0, arcColor).expr("dist", "0.8 + 0.4 * sin($T * 2)");

    merge(b, geo, {tiles, tower, ribbon}).display();
}

// PolyBevel: all edges of a box rounded (Fillet Shape Round) with an animated distance and number of divisions, and a
// box with chamfered edges
void scenePolyBevel(SceneBuilder& b)
{
    Node geo = b.object("polybevel");

    Node box = b.sop(geo, "box", "round_box")
                   .set("size", 2.5, 2.5, 2.5)
                   .set("divrate", 2, 2, 2)
                   .set("t", -2.5, 0, 0)
                   .expr("r", {"$T * 15", "$T * 25", "0"});
    Node round = b.sop(geo, "polybevel", "round")
                     .input(0, box)
                     .expr("offset", "0.1 + 0.45 * (0.5 + 0.5 * sin($T))")
                     .expr("divisions", "rint(1 + 4 * (0.5 + 0.5 * sin($T * 0.5)))");
    Node roundColor = color(b, geo, round, 0.95, 0.6, 0.3);

    Node box2 = b.sop(geo, "box", "chamfer_box")
                    .set("size", 2, 3, 2)
                    .set("divrate", 2, 2, 2)
                    .set("t", 2.5, 0, 0)
                    .expr("r", 1, "-$T * 20");
    Node chamfer = b.sop(geo, "polybevel", "chamfer")
                       .input(0, box2)
                       .set("filletshape", "chamfer")
                       .expr("offset", "0.05 + 0.5 * (0.5 + 0.5 * sin($T * 1.3))");
    Node chamferColor = color(b, geo, chamfer, 0.4, 0.7, 0.95);

    merge(b, geo, {roundColor, chamferColor}).display();
}

// Boolean: a box minus a sphere flying through it (Subtract), the union of a torus and a bar, the intersection of a
// sphere and a turning box. Inputs A and B: input 0 and input 1
void sceneBoolean(SceneBuilder& b)
{
    Node geo = b.object("boolean");

    Node box = b.sop(geo, "box", "block").set("size", 3, 3, 3).set("divrate", 2, 2, 2);
    Node hole = b.sop(geo, "sphere", "cutter")
                    .set("type", "polymesh")
                    .set("rows", 24)
                    .set("cols", 48)
                    .set("rad", 1.4, 1.4, 1.4)
                    // The sphere slides over the front face of the box (+Z, towards the camera)
                    .expr("t", {"1.2 * sin($T * 0.8)", "0.7 * sin($T * 1.3)", "1.7 + 0.5 * cos($T)"});
    Node subtract = b.sop(geo, "boolean", "subtract").input(0, box).input(1, hole).set("booleanop", "subtract");
    Node subtractColor = color(b, geo, subtract, 0.95, 0.6, 0.3);

    Node torus = b.sop(geo, "torus", "ring")
                     .set("rad", 1.4, 0.4)
                     .set("rows", 16)
                     .set("cols", 32)
                     .set("t", -5.5, 0, 0)
                     .expr("r", 0, "$T * 30");
    Node bar = b.sop(geo, "box", "bar")
                   .set("size", 4, 0.5, 0.5)
                   .set("divrate", 2, 2, 2)
                   .set("t", -5.5, 0, 0)
                   .expr("r", 2, "$T * 45");
    Node unite = b.sop(geo, "boolean", "union").input(0, torus).input(1, bar).set("booleanop", "union");
    Node unionColor = color(b, geo, unite, 0.4, 0.8, 0.5);

    Node ball = b.sop(geo, "sphere", "ball")
                    .set("type", "polymesh")
                    .set("rows", 24)
                    .set("cols", 48)
                    .set("t", 5.5, 0, 0)
                    .set("rad", 1.5, 1.5, 1.5);
    Node cube = b.sop(geo, "box", "cube")
                    .set("divrate", 2, 2, 2)
                    .set("t", 5.5, 0, 0)
                    .set("size", 2.3, 2.3, 2.3)
                    .expr("r", {"$T * 30", "$T * 40", "0"});
    Node intersect = b.sop(geo, "boolean", "intersect").input(0, ball).input(1, cube).set("booleanop", "intersect");
    Node intersectColor = color(b, geo, intersect, 0.4, 0.6, 0.95);

    merge(b, geo, {subtractColor, unionColor, intersectColor}).display();
}

// Voronoi Fracture: a sphere broken into cells around points packed inside it (Points from Volume, the spacing
// breathes, so the number of pieces changes) and a box broken around points scattered over its surface (their count
// grows); every cell point gets a random color, which Copy Cell Point Attributes passes to the faces of its piece.
// Cut Plane Offset moves the cut planes towards the cell points, so gaps open between the pieces
void sceneVoronoiFracture(SceneBuilder& b)
{
    Node geo = b.object("voronoifracture");

    Node ball = b.sop(geo, "sphere", "ball")
                    .set("type", "polymesh")
                    .set("rows", 24)
                    .set("cols", 48)
                    .set("rad", 2, 2, 2)
                    .set("t", -2.8, 0, 0)
                    .expr("r", 1, "$T * 12");
    Node cells = b.sop(geo, "pointsfromvolume", "cells")
                     .input(0, ball)
                     .expr("particlesep", "0.9 + 0.3 * sin($T * 0.5)")
                     .set("jitterscale", 1);
    Node cellColors = b.sop(geo, "attribrandomize", "cell_colors")
                          .input(0, cells)
                          .set("name", "Cd")
                          .set("min", 0.2, 0.3, 0.5, 0);
    Node ballPieces = b.sop(geo, "voronoifracture", "ball_pieces")
                          .input(0, ball)
                          .input(1, cellColors)
                          .set("copycellattribs", 1)
                          .set("primattributes", "Cd")
                          .expr("cutplaneoffset", "0.025 + 0.025 * sin($T * 1.2)");

    Node block = b.sop(geo, "box", "block")
                     .set("size", 3, 3, 3)
                     .set("divrate", 2, 2, 2)
                     .set("t", 2.8, 0, 0)
                     .expr("r", {"$T * 9", "$T * 15", "0"});
    Node shards = b.sop(geo, "scatter", "shards")
                      .input(0, block)
                      .expr("npts", "rint(6 + 34 * (0.5 + 0.5 * sin($T * 0.6)))");
    Node shardColors = b.sop(geo, "attribrandomize", "shard_colors")
                           .input(0, shards)
                           .set("name", "Cd")
                           .set("min", 0.5, 0.3, 0.1, 0);
    Node blockPieces = b.sop(geo, "voronoifracture", "block_pieces")
                           .input(0, block)
                           .input(1, shardColors)
                           .set("copycellattribs", 1)
                           .set("primattributes", "Cd")
                           .expr("cutplaneoffset", "0.025 + 0.025 * sin($T * 1.2 + 1.5)");

    merge(b, geo, {ballPieces, blockPieces}).display();
}

// Skin: a vase skinned through four horizontal rings with different point counts (Skin inserts points into the
// smaller rings so that every row of the surface has as many points as the largest one), the rings breathing in turn;
// and a patch spanned by a net of curves: three waving U cross-sections on the first input and three V cross-sections
// on the second, the surface passing between both families
void sceneSkin(SceneBuilder& b)
{
    Node geo = b.object("skin");

    const int divs[] = {8, 12, 6, 10};
    Node rings[4];
    for (int k = 0; k < 4; ++k) {
        rings[k] = b.sop(geo, "circle", fmt("ring%d", k + 1).c_str())
                       .set("type", "poly")
                       .set("orient", "zx")
                       .set("divs", divs[k])
                       .set("t", -2.8, -1.8 + k * 1.2, 0)
                       .expr("rad", {fmt("%.2f + 0.3 * sin($T * 1.6 - %d)", 1.0 + 0.35 * (k % 2), k).c_str(),
                                        fmt("%.2f + 0.3 * sin($T * 1.6 - %d)", 1.0 + 0.35 * (k % 2), k).c_str()});
    }
    Node ringSet = merge(b, geo, {rings[0], rings[1], rings[2], rings[3]}, "rings");
    Node vase = b.sop(geo, "skin", "vase").input(0, ringSet);
    Node vaseColor = color(b, geo, vase, 0.95, 0.6, 0.35);

    // U cross-sections: rows along x at z = -1.5, 0, 1.5; V cross-sections: columns along z at x = 1, 2.75, 4.5
    Node uCurves = b.sop(geo, "add", "u_curves").set("points", 15).set("prims", 3);
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 5; ++c)
            uCurves.expr(fmt("pt%d", r * 5 + c).c_str(),
                {fmt("%.3f", 1.0 + c * 0.875).c_str(), fmt("0.6 * sin($T * 1.3 + %.2f)", c * 0.9 + r * 1.7).c_str(),
                    fmt("%.2f", -1.5 + r * 1.5).c_str()});
        uCurves.set(fmt("prim%d", r).c_str(), fmt("%d-%d", r * 5, r * 5 + 4).c_str()).set(fmt("closed%d", r).c_str(), 0);
    }
    Node vCurves = b.sop(geo, "add", "v_curves").set("points", 12).set("prims", 3);
    for (int c = 0; c < 3; ++c) {
        for (int r = 0; r < 4; ++r)
            vCurves.expr(fmt("pt%d", c * 4 + r).c_str(),
                {fmt("%.3f", 1.0 + c * 1.75).c_str(), fmt("0.8 * cos($T * 0.9 + %.2f)", r * 1.1 - c).c_str(),
                    fmt("%.2f", -1.5 + r * 1.0).c_str()});
        vCurves.set(fmt("prim%d", c).c_str(), fmt("%d-%d", c * 4, c * 4 + 3).c_str()).set(fmt("closed%d", c).c_str(), 0);
    }
    Node patch = b.sop(geo, "skin", "patch").input(0, uCurves).input(1, vCurves).set("surftype", "alttriangles");
    Node patchColor = color(b, geo, patch, 0.4, 0.8, 0.6);

    merge(b, geo, {vaseColor, patchColor}).display();
}

// Subdivide: a cube smoothed by OpenSubdiv Catmull-Clark whose edges sharpen and soften in time (Override Crease
// Weight: a rounded blob at 0, the cube itself at 3 and above); a low-poly torus roughened by noise and smoothed by
// OpenSubdiv Loop (triangles); and a box with pulsing extruded faces subdivided by Houdini Catmull-Clark
void sceneSubdivide(SceneBuilder& b)
{
    Node geo = b.object("subdivide");

    Node cage = b.sop(geo, "box", "cage").set("size", 2.4, 2.4, 2.4).set("t", -3.8, 0, 0);
    Node creased = b.sop(geo, "subdivide", "creased")
                       .input(0, cage)
                       .set("iterations", 3)
                       .set("overridecrease", 1)
                       .expr("creaseweight", "1.6 + 1.6 * sin($T * 1.2)");
    Node cubeColor = color(b, geo, creased, 0.95, 0.6, 0.3);

    Node ring = b.sop(geo, "torus", "ring").set("rows", 8).set("cols", 12).set("rad", 1.3, 0.55);
    Node rough = b.sop(geo, "mountain", "rough")
                     .input(0, ring)
                     .set("elementsize", 0.8)
                     .expr("height", "0.6 + 0.3 * sin($T * 0.8)")
                     .expr("offset", {"$T * 0.3", "0", "0"});
    Node loop = b.sop(geo, "subdivide", "loop").input(0, rough).set("algorithm", "osdloop").set("iterations", 2);
    Node ringColor = color(b, geo, loop, 0.45, 0.8, 0.5);

    Node block = b.sop(geo, "box", "block").set("size", 2, 2, 2).set("divrate", 3, 3, 3).set("t", 3.8, 0, 0);
    Node studs = b.sop(geo, "polyextrude", "studs")
                     .input(0, block)
                     .set("group", "0 2 4 6 8 10 12 14 16 18 20 22")
                     .set("splittype", "elements")
                     .set("inset", 0.12)
                     .expr("dist", "0.25 + 0.2 * sin($T * 2)");
    Node houdini =
        b.sop(geo, "subdivide", "houdini").input(0, studs).set("algorithm", "houdini").set("iterations", 2);
    Node blockColor = color(b, geo, houdini, 0.4, 0.65, 0.95);

    merge(b, geo, {cubeColor, ringColor, blockColor}).display();
}

} // namespace demo::scenes
