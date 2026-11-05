// Generators: Sphere, Box, Grid, Torus, Circle, Add
#include "demo_scenes.h"
#include "scene_util.h"

namespace demo::scenes {

// Sphere: Primitive Type Polygon Mesh (rows x columns) and Polygon (an icosahedron subdivided by Frequency) with their
// divisions animated, and a sphere with an input — the bounding sphere of the input points (here of a grid that moves
// and grows). The bounding sphere is cut open by Clip so that the grid inside it is seen
void sceneSphere(SceneBuilder& b)
{
    Node geo = b.object("spheres");

    Node mesh = b.sop(geo, "sphere", "mesh_sphere")
                    .set("type", "polymesh")
                    .set("t", -4, 0, 0)
                    .expr("rows", "rint(12 + 8 * sin($T * 1.3))")
                    .expr("cols", "rint(24 + 14 * sin($T * 0.9))")
                    .expr("rad", {"1.3 + 0.2 * sin($T * 2)", "ch(\"radx\")", "ch(\"radx\")"})
                    .expr("r", 1, "$T * 30");
    Node meshColor = color(b, geo, mesh, 0.95, 0.55, 0.2);

    // Frequency 1..6: 20, 80, 180, ... triangles
    Node ico = b.sop(geo, "sphere", "ico_sphere")
                   .set("type", "poly")
                   .set("t", 0, 0, 0)
                   .expr("freq", "rint(3.5 + 2.5 * sin($T * 0.8))")
                   .expr("rad", {"1.4", "1.4 + 0.4 * sin($T * 1.5)", "1.4"});
    Node icoColor = color(b, geo, ico, 0.3, 0.7, 0.95);

    // The input: a grid that turns, moves and changes size; the sphere bounds its points
    Node grid = b.sop(geo, "grid", "inner_grid")
                    .set("rows", 6)
                    .set("cols", 6)
                    .expr("size", {"1.5 + 0.8 * sin($T)", "1.5"})
                    .expr("t", {"4", "0.6 * sin($T * 1.7)", "0"})
                    .expr("r", {"$T * 40", "0", "$T * 25"});
    Node gridColor = color(b, geo, grid, 1.0, 0.9, 0.3);
    Node bound =
        b.sop(geo, "sphere", "bounding_sphere").input(0, grid).set("type", "polymesh").set("rows", 16).set("cols", 32);
    // Clip keeps the part above the plane through the sphere center (dir — the plane normal)
    Node open = b.sop(geo, "clip", "open_front")
                    .input(0, bound)
                    .set("dir", 0, 0, -1)
                    .expr("origin", {"4", "ch(\"../inner_grid/ty\")", "0"});
    Node boundColor = color(b, geo, open, 0.6, 0.6, 0.65);

    merge(b, geo, {meshColor, icoColor, gridColor, boundColor}).display();
}

// Box: Axis Divisions, size and rotation animated; a box with an input takes the bounding box of the input (a spinning
// torus) — its top half is clipped away to show the torus inside
void sceneBox(SceneBuilder& b)
{
    Node geo = b.object("boxes");

    Node divided = b.sop(geo, "box", "divided_box")
                       .set("t", -4, 0, 0)
                       .expr("divrate",
                           {"rint(3 + 2 * sin($T))", "rint(3 + 2 * sin($T * 1.3))", "rint(3 + 2 * sin($T * 0.7))"})
                       .expr("size", {"1.6", "1.6 + 0.6 * sin($T * 2)", "1.6"})
                       .expr("r", {"$T * 20", "$T * 35", "0"});
    Node dividedColor = color(b, geo, divided, 0.9, 0.45, 0.25);

    Node tall = b.sop(geo, "box", "tall_box")
                    .set("divrate", 2, 6, 2)
                    .expr("size", {"0.8 + 0.4 * sin($T * 1.1)", "3", "0.8 + 0.4 * cos($T * 1.1)"})
                    .expr("r", 1, "$T * 45")
                    .expr("scale", "1 + 0.2 * sin($T * 3)");
    Node tallColor = color(b, geo, tall, 0.35, 0.75, 0.45);

    Node torus = b.sop(geo, "torus", "spinning_torus")
                     .set("rad", 1.2, 0.35)
                     .set("t", 4.5, 0, 0)
                     .expr("r", {"$T * 50", "$T * 20", "0"});
    Node torusColor = color(b, geo, torus, 0.95, 0.85, 0.3);
    Node bbox = b.sop(geo, "box", "bounding_box").input(0, torus);
    Node tray =
        b.sop(geo, "clip", "tray").input(0, bbox).set("clipop", "below").set("dir", 0, 1, 0).set("origin", 4.5, 0, 0);
    Node trayColor = color(b, geo, tray, 0.55, 0.6, 0.7);

    merge(b, geo, {dividedColor, tallColor, torusColor, trayColor}).display();
}

// Grid: the faces of every Connectivity (quadrilaterals, triangles, alternating triangles) and open polylines (rows,
// rows and columns), the numbers of rows and columns animated
void sceneGrid(SceneBuilder& b)
{
    Node geo = b.object("grids");
    struct Kind
    {
        const char* surftype;
        double r, g, bl;
    };
    const Kind kinds[] = {{"quads", 0.9, 0.5, 0.3}, {"triangles", 0.4, 0.8, 0.4}, {"alttriangles", 0.35, 0.6, 0.95},
        {"rows", 0.95, 0.9, 0.4}, {"rowcol", 0.9, 0.5, 0.9}};
    Node colored[5];
    for (int k = 0; k < 5; ++k) {
        const double x = (k - 2) * 4.0;
        Node g = b.sop(geo, "grid", fmt("grid_%s", kinds[k].surftype).c_str())
                     .set("surftype", kinds[k].surftype)
                     .set("orient", "xy")
                     .set("t", x, 0, 0)
                     .expr("size", {fmt("3.2 + 0.4 * sin($T * 2 + %d)", k).c_str(), "3.2"})
                     .expr("rows", fmt("rint(6 + 4 * sin($T * 0.9 + %d))", k).c_str())
                     .expr("cols", fmt("rint(6 + 4 * cos($T * 0.7 + %d))", k).c_str())
                     .expr("r", 1, fmt("25 * sin($T + %d)", k).c_str());
        colored[k] = color(b, geo, g, kinds[k].r, kinds[k].g, kinds[k].bl);
    }
    merge(b, geo, {colored[0], colored[1], colored[2], colored[3], colored[4]}).display();
}

// Torus: radii and divisions animated; a chain of thin tori made by Copy and Transform (every copy turned by 90° about
// the chain axis), the chain swings
void sceneTorus(SceneBuilder& b)
{
    Node geo = b.object("tori");

    Node donut = b.sop(geo, "torus", "donut")
                     .set("t", -4, 0, 0)
                     .expr("rad", {"1.5 + 0.3 * sin($T)", "0.5 + 0.25 * sin($T * 1.7)"})
                     .expr("rows", "rint(10 + 6 * sin($T * 0.8))")
                     .expr("cols", "rint(20 + 12 * sin($T * 0.6))")
                     .expr("r", {"60 + 20 * sin($T)", "$T * 30", "0"});
    Node donutColor = color(b, geo, donut, 0.9, 0.6, 0.35);

    // A link in the XY plane; copies along X, each turned by 90° about X — links alternate between XY and XZ
    Node link = b.sop(geo, "torus", "link").set("orient", "z").set("rad", 0.5, 0.11).set("rows", 8).set("cols", 20);
    Node chain = b.sop(geo, "copyxform", "chain")
                     .input(0, link)
                     .set("ncy", 9)
                     .set("t", 0.78, 0, 0)
                     .expr("r", {"90", "6 * sin($T * 1.5)", "8 * sin($T)"});
    Node placed =
        b.sop(geo, "xform", "place_chain").input(0, chain).set("t", 0, 0, 0).expr("r", 1, "15 * sin($T * 0.5)");
    Node chainColor = color(b, geo, placed, 0.75, 0.78, 0.85);

    merge(b, geo, {donutColor, chainColor}).display();
}

// Circle: polygon arcs of every Arc Type (closed polygon with animated divisions, open arc, closed arc, sliced arc —
// a pie) and the exact rational NURBS and Bezier circles (Imperfect off), the radii animated
void sceneCircle(SceneBuilder& b)
{
    Node geo = b.object("circles");

    Node polygon = b.sop(geo, "circle", "polygon")
                       .set("t", -7.5, 0, 0)
                       .expr("divs", "rint(7.5 + 4.5 * sin($T * 0.7))")
                       .expr("r", 2, "$T * 20");
    Node polygonColor = color(b, geo, polygon, 0.9, 0.5, 0.3);

    Node open = b.sop(geo, "circle", "open_arc")
                    .set("arc", "openarc")
                    .set("divs", 32)
                    .set("t", -4.5, 0, 0)
                    .expr("angle", {"$T * 36", "$T * 36 + 180 + 150 * sin($T)"});
    Node openColor = color(b, geo, open, 0.95, 0.95, 0.5);

    Node closedArc = b.sop(geo, "circle", "closed_arc")
                         .set("arc", "closedarc")
                         .set("divs", 24)
                         .set("t", -1.5, 0, 0)
                         .expr("angle", {"0", "200 + 140 * sin($T * 0.9)"});
    Node closedArcColor = color(b, geo, closedArc, 0.4, 0.8, 0.45);

    Node pie = b.sop(geo, "circle", "pie")
                   .set("arc", "slicedarc")
                   .set("divs", 24)
                   .set("t", 1.5, 0, 0)
                   .expr("angle", {"$T * 20", "$T * 20 + 270 + 80 * sin($T * 1.3)"});
    Node pieColor = color(b, geo, pie, 0.35, 0.6, 0.95);

    Node nurbs = b.sop(geo, "circle", "nurbs_circle")
                     .set("type", "nurbs")
                     .set("imperfect", 0)
                     .set("order", 3)
                     .set("t", 4.5, 0, 0)
                     .expr("rad", {"1 + 0.4 * sin($T * 1.2)", "1 - 0.4 * sin($T * 1.2)"});
    Node nurbsColor = color(b, geo, nurbs, 0.5, 1, 0.6);

    Node bezier = b.sop(geo, "circle", "bezier_circle")
                      .set("type", "bezier")
                      .set("imperfect", 0)
                      .set("orient", "zx")
                      .set("t", 7.5, 0, 0)
                      .expr("rad", {"1 + 0.3 * sin($T * 2)", "1 + 0.3 * sin($T * 2)"})
                      .expr("r", 0, "$T * 40");
    Node bezierColor = color(b, geo, bezier, 1, 0.6, 0.9);

    merge(b, geo, {polygonColor, openColor, closedArcColor, pieColor, nurbsColor, bezierColor}).display();
}

// Add: points of the "points" multiparm (numbered from 0: pt0, pt1, ...) moved by expressions and polygons of the
// "prims" multiparm By Pattern — a closed star "0-9" and an open polyline; another Add has points without primitives,
// which are drawn as points
void sceneAdd(SceneBuilder& b)
{
    Node geo = b.object("add_shapes");

    // A star of 10 points: odd points pulse, the star turns
    Node star = b.sop(geo, "add", "star").set("points", 10);
    for (int k = 0; k < 10; ++k) {
        const char* radius = k % 2 == 0 ? "2.2" : "(1 + 0.4 * sin($T * 3))";
        const double angle = k * 36.0 * 3.14159265 / 180.0;
        const std::string pt = fmt("pt%d", k);
        star.expr(pt.c_str(), {fmt("-3 + %s * cos(%.6f + $T * 0.5)", radius, angle).c_str(),
                                  fmt("%s * sin(%.6f + $T * 0.5)", radius, angle).c_str(), nullptr});
    }
    star.set("prims", 1).set("prim0", "0-9").set("closed0", 1);
    Node starColor = color(b, geo, star, 0.95, 0.75, 0.25);

    // A wave polyline of 9 points
    Node wave = b.sop(geo, "add", "wave").set("points", 9);
    for (int k = 0; k < 9; ++k)
        wave.expr(fmt("pt%d", k).c_str(),
            {fmt("%g", 1.5 + k * 0.6).c_str(), fmt("1.2 * sin($T * 2 + %d * 0.8)", k).c_str(),
                fmt("0.5 * cos($T * 1.5 + %d * 0.6)", k).c_str()});
    wave.set("prims", 1).set("prim0", "0-8").set("closed0", 0);
    Node waveColor = color(b, geo, wave, 0.4, 0.85, 0.95);

    merge(b, geo, {starColor, waveColor}).display();

    // Points only: three points circling each other
    Node dotsGeo = b.object("add_points");
    Node dots = b.sop(dotsGeo, "add", "dots").set("points", 3).set("prims", 0);
    for (int k = 0; k < 3; ++k)
        dots.expr(fmt("pt%d", k).c_str(),
            {fmt("1.2 * cos($T * 2 + %d * 2.0944)", k).c_str(), fmt("-2.5 + 0.5 * sin($T * 3 + %d)", k).c_str(),
                fmt("1.2 * sin($T * 2 + %d * 2.0944)", k).c_str()});
    color(b, dotsGeo, dots, 1, 0.4, 0.4).display();
}

} // namespace demo::scenes
