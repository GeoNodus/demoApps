// Modeling: Clip, PolyExtrude, PolyBevel, Boolean
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

} // namespace demo::scenes
