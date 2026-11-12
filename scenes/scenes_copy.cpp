// Copies and points: Scatter, Points from Volume, Copy to Points, Copy and Transform
#include "demo_scenes.h"
#include "scene_util.h"

namespace demo::scenes {

// Scatter: points over the surface of a torus, their number growing and shrinking (Force Total Count) and relaxed
// apart; small spheres are copied onto them to make the points visible
void sceneScatter(SceneBuilder& b)
{
    Node geo = b.object("scatter");

    Node torus =
        b.sop(geo, "torus", "ring").set("rad", 3, 1).set("rows", 24).set("cols", 48).expr("r", 0, "20 * sin($T * 0.5)");
    Node torusColor = color(b, geo, torus, 0.3, 0.4, 0.55);
    Node points = b.sop(geo, "scatter", "points")
                      .input(0, torus)
                      .expr("npts", "rint(60 + 540 * (0.5 + 0.5 * sin($T * 0.6)))")
                      .expr("seed", "rint($T / 2)");
    Node dot = b.sop(geo, "sphere", "dot").set("type", "poly").set("freq", 1).set("rad", 0.12, 0.12, 0.12);
    Node dots = b.sop(geo, "copytopoints", "dots").input(0, dot).input(1, points);
    Node dotsColor = color(b, geo, dots, 1, 0.8, 0.3);

    merge(b, geo, {torusColor, dotsColor}).display();
}

// Points from Volume: a turning torus filled with points (tetrahedral packing, Sparse Volume) whose spacing and jitter
// breathe; balls of the radius pscale (Add Scale Attribute) are copied onto them, and the cut-away half shows the
// packing inside
void scenePointsFromVolume(SceneBuilder& b)
{
    Node geo = b.object("pointsfromvolume");

    Node torus = b.sop(geo, "torus", "ring")
                     .set("rad", 2.2, 0.9)
                     .set("rows", 24)
                     .set("cols", 48)
                     .expr("r", 0, "90 + 25 * sin($T * 0.4)");
    Node fill = b.sop(geo, "pointsfromvolume", "fill")
                    .input(0, torus)
                    .set("inittype", "tetrahedral")
                    .expr("particlesep", "0.3 + 0.08 * sin($T * 0.8)")
                    .expr("jitterscale", "0.25 + 0.25 * sin($T * 1.3)")
                    .set("addscale", 1)
                    .set("radiusscale", 0.45);
    Node ball = b.sop(geo, "sphere", "ball").set("type", "poly").set("freq", 1);
    Node balls = b.sop(geo, "copytopoints", "balls").input(0, ball).input(1, fill);
    Node half = b.sop(geo, "clip", "half").input(0, balls).set("dir", 0, 0, 1);
    Node ballsColor = color(b, geo, half, 0.95, 0.55, 0.25);
    Node shell = b.sop(geo, "clip", "shell").input(0, torus).set("dir", 0, 0, -1);
    Node shellColor = color(b, geo, shell, 0.3, 0.4, 0.55);

    merge(b, geo, {ballsColor, shellColor}).display();
}

// Copy to Points: boxes copied onto the points of a waving grid; +Z of every copy follows the point normal N (Normal
// SOP), the size — the random pscale, the color — Cd of the target points (Attributes from Target: the "targetattribs"
// multiparm, one row "Cd")
void sceneCopyToPoints(SceneBuilder& b)
{
    Node geo = b.object("copytopoints");

    Node grid = b.sop(geo, "grid", "field").set("size", 10, 10).set("rows", 16).set("cols", 16);
    Node wave =
        b.sop(geo, "mountain", "wave").input(0, grid).set("height", 1.6).set("elementsize", 4).expr("time", "$T * 0.5");
    Node normals = b.sop(geo, "normal", "normals").input(0, wave).set("type", "typepoint");
    Node sizes = b.sop(geo, "attribrandomize", "sizes")
                     .input(0, normals)
                     .set("name", "pscale")
                     .set("dimensions", 1)
                     .expr("min", 0, "0.4 + 0.3 * sin($T * 2)")
                     .set("max", 1.2, 1, 1, 1);
    Node colors = b.sop(geo, "attribrandomize", "colors").input(0, sizes).set("name", "Cd").set("min", 0.2, 0.3, 0.5, 0);
    Node box = b.sop(geo, "box", "brick").set("size", 0.3, 0.3, 0.6).set("t", 0, 0, 0.3);
    b.sop(geo, "copytopoints", "bricks")
        .input(0, box)
        .input(1, colors)
        .set("pivot", "origin")
        .set("targetattribs", 1)
        .set("applyattribs1", "Cd")
        .display();
}

// Copy and Transform: a spiral staircase — a step copied with a rise and a turn per copy (the transform of copy k is
// the parameters times k), the number of copies and the turn animated; and a tower of tori that shrink by a uniform
// scale per copy (scale^k)
void sceneCopyXform(SceneBuilder& b)
{
    Node geo = b.object("copyxform");

    Node step = b.sop(geo, "box", "step").set("size", 2, 0.15, 0.7).set("t", 1.4, 0, 0);
    Node stairs = b.sop(geo, "copyxform", "stairs")
                      .input(0, step)
                      .expr("ncy", "rint(12 + 18 * (0.5 + 0.5 * sin($T * 0.5)))")
                      .set("t", 0, 0.22, 0)
                      .expr("r", 1, "18 + 6 * sin($T * 0.8)");
    Node stairsColor = color(b, geo, stairs, 0.85, 0.65, 0.4);

    Node torus = b.sop(geo, "torus", "ring").set("rad", 1.5, 0.3).set("t", -6, 0, 0);
    Node tower = b.sop(geo, "copyxform", "tower")
                     .input(0, torus)
                     .set("ncy", 10)
                     .set("t", 0, 0.65, 0)
                     .set("p", -6, 0, 0)
                     .expr("r", 1, "15 * sin($T)")
                     .expr("scale", "0.88 + 0.06 * sin($T * 1.5)");
    Node towerColor = color(b, geo, tower, 0.4, 0.7, 0.95);

    merge(b, geo, {stairsColor, towerColor}).display();
}

} // namespace demo::scenes
