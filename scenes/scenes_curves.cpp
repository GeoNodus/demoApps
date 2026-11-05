// Curves: Fit, Resample, Carve, Join, Orient along Curve
#include "demo_scenes.h"
#include "scene_util.h"

namespace demo::scenes {

namespace {

// The expression with every '#' replaced by the number k
std::string withNumber(const char* expression, int k)
{
    std::string s;
    for (const char* c = expression; *c; ++c)
        s += *c == '#' ? std::to_string(k) : std::string(1, *c);
    return s;
}

// An Add SOP with an open polyline through `count` points; the coordinates of point k are the expressions x, y, z with
// '#' replaced by k
Node addPolyline(SceneBuilder& b, const Node& network, const char* name, int count, const char* x, const char* y,
    const char* z)
{
    Node add = b.sop(network, "add", name).set("points", count);
    for (int k = 0; k < count; ++k)
        add.expr(fmt("pt%d", k).c_str(),
            {withNumber(x, k).c_str(), withNumber(y, k).c_str(), withNumber(z, k).c_str()});
    return add.set("prims", 1).set("prim0", fmt("0-%d", count - 1).c_str()).set("closed0", 0);
}

} // namespace

// Fit: a polyline through 8 moving points (Add) and the curves fitted to it — an interpolating NURBS curve passing
// through every point and an approximating Bezier curve; the points are marked with small spheres
void sceneFit(SceneBuilder& b)
{
    Node geo = b.object("fit");

    Node points = addPolyline(b, geo, "points", 8, "-4.2 + # * 1.2", "1.3 * sin($T * 1.5 + # * 0.9)",
        "0.8 * cos($T + # * 0.7)");
    Node hull = color(b, geo, points, 0.6, 0.6, 0.6, "hull");

    Node interp = b.sop(geo, "fit", "interpolate").input(0, points).set("method", "interp");
    Node interpColor = color(b, geo, interp, 1, 0.6, 0.3);

    Node approx = b.sop(geo, "fit", "approximate")
                      .input(0, points)
                      .set("type", "bezier")
                      .expr("tol", "0.05 + 0.5 * (0.5 + 0.5 * sin($T * 0.7))");
    Node approxDown = b.sop(geo, "xform", "approx_down").input(0, approx).set("t", 0, -3.5, 0);
    Node approxColor = color(b, geo, approxDown, 0.4, 0.8, 0.95);

    Node dot = b.sop(geo, "sphere", "dot").set("type", "poly").set("freq", 2).set("rad", 0.12, 0.12, 0.12);
    Node dots = b.sop(geo, "copytopoints", "markers").input(0, dot).input(1, points);
    Node dotsColor = color(b, geo, dots, 1, 1, 0.5);

    merge(b, geo, {hull, interpColor, approxColor, dotsColor}).display();
}

// Resample: a NURBS circle resampled into a polyline by an animated segment length (spheres on the new points) and a
// zigzag polyline smoothed by resampling as a subdivision curve
void sceneResample(SceneBuilder& b)
{
    Node geo = b.object("resample");

    Node circle = b.sop(geo, "circle", "circle")
                      .set("type", "nurbs")
                      .set("imperfect", 0)
                      .set("rad", 2.5, 2.5)
                      .set("t", -3.5, 0, 0);
    Node resampled =
        b.sop(geo, "resample", "by_length").input(0, circle).expr("length", "0.2 + 0.9 * (0.5 + 0.5 * sin($T * 0.7))");
    Node resampledColor = color(b, geo, resampled, 0.6, 0.8, 1);
    Node dot = b.sop(geo, "sphere", "dot").set("type", "poly").set("freq", 2).set("rad", 0.1, 0.1, 0.1);
    Node dots = b.sop(geo, "copytopoints", "dots").input(0, dot).input(1, resampled);
    Node dotsColor = color(b, geo, dots, 1, 0.75, 0.3);

    Node zigzag = addPolyline(b, geo, "zigzag", 7, "1 + # * 0.9", "1.6 * (-1)^# * (0.6 + 0.4 * sin($T))", "0");
    Node smooth = b.sop(geo, "resample", "smooth")
                      .input(0, zigzag)
                      .set("treatpolysas", "subd")
                      .expr("length", "0.05 + 0.3 * (0.5 + 0.5 * sin($T * 0.9))");
    Node smoothColor = color(b, geo, smooth, 0.5, 1, 0.5);
    Node zigzagColor = color(b, geo, zigzag, 0.55, 0.55, 0.55);

    merge(b, geo, {resampledColor, dotsColor, smoothColor, zigzagColor}).display();
}

// Carve: an arc of a NURBS circle growing from U = 0 to an animated Second U, and a short piece of a spiral polyline
// (First U and Second U both moving) running along it; the pieces are extruded into ribbons to be seen better
void sceneCarve(SceneBuilder& b)
{
    Node geo = b.object("carve");

    Node circle = b.sop(geo, "circle", "circle")
                      .set("type", "nurbs")
                      .set("imperfect", 0)
                      .set("rad", 2.5, 2.5)
                      .set("orient", "zx")
                      .set("t", -3.5, 0, 0);
    Node grow = b.sop(geo, "carve", "grow")
                    .input(0, circle)
                    .set("firstu", 1)
                    .set("domainu1", 0)
                    .set("secondu", 1)
                    .expr("domainu2", "0.03 + 0.97 * (0.5 + 0.5 * sin($T * 0.8))");
    Node growRibbon = b.sop(geo, "polyextrude", "grow_ribbon").input(0, grow).set("dist", 0.6);
    Node growColor = color(b, geo, growRibbon, 1, 0.6, 0.3);

    // A spiral of 40 points
    Node spiral = addPolyline(b, geo, "spiral", 40, "3.5 + (0.5 + # * 0.05) * cos(# * 0.45)", "-2 + # * 0.1",
        "(0.5 + # * 0.05) * sin(# * 0.45)");
    Node piece = b.sop(geo, "carve", "running_piece")
                     .input(0, spiral)
                     .set("firstu", 1)
                     .expr("domainu1", "0.4 + 0.4 * sin($T * 0.7)")
                     .set("secondu", 1)
                     .expr("domainu2", "ch(\"domainu1\") + 0.18");
    Node pieceRibbon = b.sop(geo, "polyextrude", "piece_ribbon").input(0, piece).set("dist", 0.35);
    Node pieceColor = color(b, geo, pieceRibbon, 0.4, 0.8, 0.95);
    Node spiralColor = color(b, geo, spiral, 0.55, 0.55, 0.55);

    merge(b, geo, {growColor, pieceColor, spiralColor}).display();
}

// Join: two NURBS arcs moving towards each other joined into one curve with Blend (the joint is smoothed, the
// Tolerance and Bias animated), and two polylines joined end to start without blending; the joined curves are extruded
// into ribbons
void sceneJoin(SceneBuilder& b)
{
    Node geo = b.object("join");

    Node arc1 = b.sop(geo, "circle", "arc1")
                    .set("type", "nurbs")
                    .set("arc", "openarc")
                    .set("angle", 90, 270)
                    .set("rad", 1.5, 1.5)
                    .expr("t", {"-1.2 - 0.8 * sin($T)", "2", "0"});
    Node arc2 = b.sop(geo, "circle", "arc2")
                    .set("type", "nurbs")
                    .set("arc", "openarc")
                    .set("angle", -90, 90)
                    .set("rad", 1.5, 1.5)
                    .expr("t", {"1.2 + 0.8 * sin($T)", "2", "0"})
                    .expr("r", 2, "30 * sin($T * 1.3)");
    Node arcs = merge(b, geo, {arc1, arc2}, "arcs");
    Node blended = b.sop(geo, "join", "blend")
                       .input(0, arcs)
                       .set("blend", 1)
                       .expr("tolerance", "0.2 + 0.6 * (0.5 + 0.5 * sin($T * 0.9))")
                       .expr("bias", "0.5 + 0.4 * sin($T * 0.5)");
    Node blendedRibbon = b.sop(geo, "polyextrude", "blend_ribbon").input(0, blended).set("dist", 0.3);
    Node blendedColor = color(b, geo, blendedRibbon, 1, 0.6, 0.3);

    Node line1 = addPolyline(b, geo, "line1", 4, "-4 + #", "-2 + 0.5 * sin($T * 2 + #)", "0");
    Node line2 = addPolyline(b, geo, "line2", 4, "1 + #", "-2 + 0.8 * cos($T * 1.5 + #)", "0");
    Node lines = merge(b, geo, {line1, line2}, "lines");
    Node joined = b.sop(geo, "join", "connect").input(0, lines).set("blend", 0);
    Node joinedRibbon = b.sop(geo, "polyextrude", "connect_ribbon").input(0, joined).set("dist", 0.3);
    Node joinedColor = color(b, geo, joinedRibbon, 0.4, 0.8, 0.95);

    merge(b, geo, {blendedColor, joinedColor}).display();
}

// Orient along Curve: frames along a resampled circle — the tangent goes into N (Z axis) and the up vector into up
// (Y axis); Copy to Points orients plates by N and up. A roll grows along the curve and turns in time, so the plates
// twist like a Mobius band
void sceneOrientAlongCurve(SceneBuilder& b)
{
    Node geo = b.object("orient");

    Node circle =
        b.sop(geo, "circle", "path").set("type", "nurbs").set("imperfect", 0).set("rad", 4, 4).set("orient", "zx");
    Node points = b.sop(geo, "resample", "points").input(0, circle).set("length", 0.35);
    Node frames = b.sop(geo, "orientalongcurve", "frames")
                      .input(0, points)
                      .set("upvectortype", "y")
                      .set("applyroll", 1)
                      .expr("roll", "$T * 45")
                      .set("rollper", "fulledges")
                      .expr("fulltwists", "rint(1 + sin($T * 0.4))");
    Node plate = b.sop(geo, "box", "plate").set("size", 0.08, 1.4, 0.3);
    Node plates = b.sop(geo, "copytopoints", "plates").input(0, plate).input(1, frames);
    color(b, geo, plates, 0.95, 0.65, 0.35).display();
}

} // namespace demo::scenes
