// Deformers: Transform, Mountain, Bend, Path Deform, Match Size, Fuse
#include "demo_scenes.h"
#include "scene_util.h"

namespace demo::scenes {

// Transform SOP: a torus spun, bounced and squashed (t, r, s), a box sheared, a column turned about a pivot at its
// foot, and half of the points of a grid (Group "0-120", Group Type Points) lifted — the rest stay
void sceneXform(SceneBuilder& b)
{
    Node geo = b.object("xform");

    Node torus = b.sop(geo, "torus", "ring").set("rad", 1.2, 0.4);
    Node spin = b.sop(geo, "xform", "spin_bounce_squash")
                    .input(0, torus)
                    .expr("t", {"-6", "abs(sin($T * 3)) * 1.5", "0"})
                    .expr("r", {"$T * 60", "$T * 25", "0"})
                    .expr("s", {"1 + 0.25 * sin($T * 6)", "1 - 0.25 * sin($T * 6)", "1"});
    Node torusColor = color(b, geo, spin, 0.95, 0.6, 0.3);

    Node box = b.sop(geo, "box", "block").set("size", 1.5, 1.5, 1.5).set("divrate", 3, 3, 3);
    Node shear = b.sop(geo, "xform", "shear")
                     .input(0, box)
                     .set("t", -2, 0, 0)
                     .expr("shear", {"0.8 * sin($T * 1.5)", "0", "0.4 * sin($T)"});
    Node boxColor = color(b, geo, shear, 0.4, 0.8, 0.5);

    // A column turning about its foot: the pivot p is at the bottom of the box
    Node column = b.sop(geo, "box", "column").set("size", 0.6, 3, 0.6).set("t", 0, 1.5, 0);
    Node tilt = b.sop(geo, "xform", "tilt")
                    .input(0, column)
                    .set("t", 2, -1.5, 0)
                    .set("p", 0, 0, 0)
                    .expr("r", {"35 * sin($T * 1.2)", "$T * 40", "0"});
    Node columnColor = color(b, geo, tilt, 0.35, 0.6, 0.95);

    // Group: the first 121 points (11 of 21 rows) of the grid
    Node grid = b.sop(geo, "grid", "sheet").set("size", 4, 4).set("rows", 11).set("cols", 21).set("t", 6.5, -0.5, 0);
    Node lift = b.sop(geo, "xform", "lift_half")
                    .input(0, grid)
                    .set("group", "0-120")
                    .set("grouptype", "points")
                    .expr("t", 1, "1.2 * sin($T * 1.5)")
                    .expr("r", 2, "20 * sin($T)");
    Node gridColor = color(b, geo, lift, 0.9, 0.85, 0.4);

    merge(b, geo, {torusColor, boxColor, columnColor, gridColor}).display();
}

// Mountain: Unified Noise displacement along the normal; a terrain flowing with the noise offset and time, and a
// sphere with Worley noise whose height breathes
void sceneMountain(SceneBuilder& b)
{
    Node geo = b.object("mountain");

    Node grid = b.sop(geo, "grid", "land").set("size", 10, 10).set("rows", 90).set("cols", 90).set("t", -6, 0, 0);
    Node land = b.sop(geo, "mountain", "terrain")
                    .input(0, grid)
                    .set("elementsize", 3)
                    .set("oct", 6)
                    .expr("height", "1.5 + 0.7 * sin($T * 0.5)")
                    .expr("offset", {"$T * 0.4", "0", "$T * 0.2"});
    Node landColor = color(b, geo, land, 0.45, 0.65, 0.35);

    Node sphere = b.sop(geo, "sphere", "planet")
                      .set("type", "polymesh")
                      .set("rows", 60)
                      .set("cols", 120)
                      .set("rad", 2.5, 2.5, 2.5)
                      .set("t", 4, 0, 0);
    Node rock = b.sop(geo, "mountain", "rock")
                    .input(0, sphere)
                    .set("basis", "worleyFA")
                    .set("elementsize", 1.2)
                    .set("fractal", "none")
                    .expr("height", "0.6 + 0.4 * sin($T)")
                    .expr("time", "$T * 0.3");
    Node rockColor = color(b, geo, rock, 0.75, 0.6, 0.5);

    merge(b, geo, {landColor, rockColor}).display();
}

// Bend: a column bent by an animated angle and twisted (the capture region — origin, direction and length — runs
// along the column), and a second column bent towards a goal direction and tapered
void sceneBend(SceneBuilder& b)
{
    Node geo = b.object("bend");

    Node column = b.sop(geo, "box", "column").set("size", 0.8, 6, 0.8).set("divrate", 4, 40, 4).set("t", -2.5, 3, 0);
    Node bend = b.sop(geo, "bend", "bend_twist")
                    .input(0, column)
                    .set("origin", -2.5, 0, 0)
                    .set("dir", 0, 1, 0)
                    .set("length", 6)
                    .set("upvectorcontrol", "xaxis")
                    .expr("bend", "150 * sin($T * 0.8)")
                    .set("enabletwist", 1)
                    .expr("twist", "180 * sin($T * 0.6)");
    Node bendColor = color(b, geo, bend, 0.95, 0.55, 0.3);

    Node column2 = b.sop(geo, "box", "column2").set("size", 1, 6, 1).set("divrate", 4, 40, 4).set("t", 2.5, 3, 0);
    Node bend2 = b.sop(geo, "bend", "bend_taper")
                     .input(0, column2)
                     .set("origin", 2.5, 0, 0)
                     .set("dir", 0, 1, 0)
                     .set("length", 6)
                     .set("upvectorcontrol", "xaxis")
                     .set("bendmode", "dir")
                     .expr("goaldir", {"sin($T)", "0.3", "cos($T)"})
                     .set("enabletaper", 1)
                     .expr("taper", "0.6 + 0.5 * sin($T * 1.4)");
    Node bendColor2 = color(b, geo, bend2, 0.35, 0.65, 0.95);

    merge(b, geo, {bendColor, bendColor2}).display();
}

// Path Deform: a long column laid along a waving NURBS curve (Fit through moving points) with a taper ramp and an
// animated roll, and a ribbon sliding around a circle and twisted along it, whose length grows and shrinks; the ribbon
// wraps around the closed curve (Extend on a closed curve). The roll ramp is a function of the curve fraction, so the
// twist is a whole turn (360 degrees): the roll at the end of the circle equals the roll at its start and the ribbon
// does not tear where it crosses the start of the circle
void scenePathDeform(SceneBuilder& b)
{
    Node geo = b.object("pathdeform");

    Node wave = b.sop(geo, "add", "wave_points").set("points", 7);
    for (int k = 0; k < 7; ++k)
        wave.expr(fmt("pt%d", k).c_str(),
            {fmt("%g", -6 + k * 2.0).c_str(), fmt("1.5 * sin($T * 1.2 + %d * 0.9)", k).c_str(),
                fmt("1.2 * cos($T * 0.8 + %d * 0.7)", k).c_str()});
    wave.set("prims", 1).set("prim0", "0-6").set("closed0", 0);
    Node path = b.sop(geo, "fit", "wave_path").input(0, wave).set("method", 1).set("type", "nurbs");

    Node column = b.sop(geo, "box", "column").set("size", 0.7, 0.45, 10).set("divrate", 3, 3, 80);
    Node snake = b.sop(geo, "pathdeform", "snake")
                     .input(0, column)
                     .input(1, path)
                     .set("curve_endunit", "fractioncurvelength")
                     .set("taper_enable", 1)
                     .set("taper_enableramp", 1)
                     .set("taper_ramp", 3)
                     .set("taper_ramp1value", 0.3)
                     .set("taper_ramp2pos", 1.0)
                     .set("taper_ramp2value", 0.15)
                     .set("taper_ramp3pos", 0.35)
                     .set("taper_ramp3value", 1.6)
                     .set("taper_ramp3interp", "catmull-rom")
                     .set("rotate_enable", 1)
                     .expr("rotate_angle", "240 * sin($T * 0.7)")
                     .set("rotate_enableramp", 1);
    Node snakeColor = color(b, geo, snake, 0.95, 0.6, 0.25);

    Node ring =
        b.sop(geo, "circle", "ring_path").set("divs", 64).set("orient", "zx").set("rad", 4, 4).set("t", 0, -3, 0);
    Node strip = b.sop(geo, "grid", "strip").set("orient", "zx").set("size", 1.2, 10).set("rows", 120).set("cols", 3);
    Node band = b.sop(geo, "pathdeform", "band")
                    .input(0, strip)
                    .input(1, ring)
                    .set("curve_endunit", "fractioncurvelength")
                    .expr("curve_posend", "0.55 + 0.35 * sin($T * 0.9)")
                    .expr("curve_posoffset", "$T * 0.06")
                    .set("rotate_enable", 1)
                    .set("rotate_angle", 360)
                    .set("rotate_enableramp", 1);
    Node bandColor = color(b, geo, band, 0.35, 0.65, 0.95);

    merge(b, geo, {snakeColor, bandColor}).display();
}

// Match Size: a torus, a sphere and a thin box stretched into one box (Justify Target — Location and Size, Scale to
// Fit on), whose size pulses; the target boxes are drawn as wire frames (Box with Rows and Columns connectivity) whose
// size is read from the Match Size nodes by ch()
void sceneMatchSize(SceneBuilder& b)
{
    Node geo = b.object("matchsize");
    const char* const sizes[] = {"2 + 0.8 * sin($T * 1.3)", "2 + 0.8 * sin($T * 1.3 + 2)",
        "2 + 0.8 * sin($T * 1.3 + 4)"};
    struct Shape
    {
        const char* name;
        double x, r, g, bl;
    };
    const Shape shapes[] = {{"torus", -4, 0.95, 0.6, 0.3}, {"sphere", 0, 0.4, 0.8, 0.5}, {"box", 4, 0.35, 0.6, 0.95}};
    Node parts[6];
    for (int k = 0; k < 3; ++k) {
        Node source;
        if (k == 0)
            source = b.sop(geo, "torus", "torus").set("rad", 1, 0.3);
        else if (k == 1)
            source = b.sop(geo, "sphere", "sphere").set("type", "polymesh").set("rad", 0.3, 0.3, 0.3);
        else
            source = b.sop(geo, "box", "box").set("size", 0.2, 3, 0.4).expr("r", 2, "$T * 30");
        const std::string name = fmt("fit_%s", shapes[k].name);
        Node fit = b.sop(geo, "matchsize", name.c_str())
                       .input(0, source)
                       .set("justifytarget", "explicit")
                       .set("t", shapes[k].x, 0, 0)
                       .expr("size", {sizes[k], sizes[(k + 1) % 3], sizes[(k + 2) % 3]})
                       .set("doscale", 1)
                       .set("uniformscale", 0);
        parts[2 * k] = color(b, geo, fit, shapes[k].r, shapes[k].g, shapes[k].bl);
        parts[2 * k + 1] = b.sop(geo, "box", fmt("frame_%s", shapes[k].name).c_str())
                               .set("surftype", "rowcol")
                               .set("divrate", 2, 2, 2)
                               .set("t", shapes[k].x, 0, 0)
                               .expr("size", {fmt("ch(\"../%s/sizex\")", name.c_str()).c_str(),
                                                 fmt("ch(\"../%s/sizey\")", name.c_str()).c_str(),
                                                 fmt("ch(\"../%s/sizez\")", name.c_str()).c_str()});
    }
    // The frames have no Cd: one color for all of them, so that the merge does not make them black
    Node frames = merge(b, geo, {parts[1], parts[3], parts[5]}, "frames");
    Node framesColor = color(b, geo, frames, 0.9, 0.9, 0.9);
    merge(b, geo, {parts[0], parts[2], parts[4], framesColor}).display();
}

// Fuse: points of a sphere snapped to a grid whose spacing changes (Snap To Grid; points of one cell fuse, degenerate
// faces are removed — the sphere becomes blocky), and points of a torus fused within a growing distance
void sceneFuse(SceneBuilder& b)
{
    Node geo = b.object("fuse");

    Node sphere = b.sop(geo, "sphere", "ball")
                      .set("type", "polymesh")
                      .set("rows", 40)
                      .set("cols", 80)
                      .set("rad", 2.5, 2.5, 2.5)
                      .set("t", -3.5, 0, 0);
    Node snap = b.sop(geo, "fuse", "snap_to_grid")
                    .input(0, sphere)
                    .set("snaptype", "gridsnap")
                    .expr("gridspacing",
                        {"0.15 + 0.5 * (0.5 + 0.5 * sin($T * 0.8))", "ch(\"gridspacingx\")", "ch(\"gridspacingx\")"});
    Node snapColor = color(b, geo, snap, 0.95, 0.6, 0.3);

    Node torus = b.sop(geo, "torus", "ring")
                     .set("rad", 2, 0.8)
                     .set("rows", 30)
                     .set("cols", 60)
                     .set("t", 3.5, 0, 0)
                     .expr("r", 0, "60");
    Node fuse =
        b.sop(geo, "fuse", "fuse_distance").input(0, torus).expr("tol3d", "0.02 + 0.35 * (0.5 + 0.5 * sin($T * 0.6))");
    Node fuseColor = color(b, geo, fuse, 0.4, 0.7, 0.95);

    merge(b, geo, {snapColor, fuseColor}).display();
}

} // namespace demo::scenes
