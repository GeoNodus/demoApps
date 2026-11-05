// Networks and objects: Merge, SOP Null, SOP Subnet, Output, geo and null objects
#include "demo_scenes.h"
#include "scene_util.h"

namespace demo::scenes {

// Merge: five generators orbiting the center merged into one geometry (the inputs one after another, in input order)
void sceneMerge(SceneBuilder& b)
{
    Node geo = b.object("merge");
    const char* const ops[] = {"sphere", "box", "torus", "circle", "grid"};
    const double colors[5][3] = {{0.95, 0.55, 0.3}, {0.4, 0.8, 0.5}, {0.35, 0.6, 0.95}, {0.95, 0.9, 0.4},
        {0.85, 0.5, 0.9}};
    Node parts[5];
    for (int k = 0; k < 5; ++k) {
        Node n = b.sop(geo, ops[k], ops[k])
                     .expr("t", {fmt("3.5 * cos($T * 0.6 + %d * 1.2566)", k).c_str(),
                                    fmt("0.8 * sin($T * 1.4 + %d)", k).c_str(),
                                    fmt("3.5 * sin($T * 0.6 + %d * 1.2566)", k).c_str()})
                     .expr("r", {fmt("$T * %d", 20 + 10 * k).c_str(), "$T * 30", nullptr});
        if (k == 0)
            n.set("type", "polymesh");
        if (k == 3)
            n.set("arc", "slicedarc").set("angle", 0, 300);
        if (k == 4)
            n.set("size", 1.6, 1.6);
        parts[k] = color(b, geo, n, colors[k][0], colors[k][1], colors[k][2]);
    }
    merge(b, geo, {parts[0], parts[1], parts[2], parts[3], parts[4]}, "all").display();
}

// SOP Null: a node that passes its input through. Used as a named control node — CTRL has user (spare) parameters
// added with gnAddSpareParm, which other nodes read with ch() (an expression on a user parameter animates the control
// itself) — and as the named end of a chain (OUT carries the Display flag). A second object reads the same controls by
// an absolute path
void sceneNull(SceneBuilder& b)
{
    Node geo = b.object("shapes");

    Node ctrl = b.sop(geo, "null", "CTRL")
                    .addFloatParm("radius", "Radius", 2, 0.1, 5)
                    .addFloatParm("height", "Noise Height", 0.5, 0, 2)
                    .addFloatParm("speed", "Speed", 0.5, 0, 2)
                    .expr("height", "0.6 + 0.5 * sin($T * 0.7)")
                    .expr("radius", "2 + 0.4 * sin($T * 1.3)");
    (void) ctrl;

    Node sphere = b.sop(geo, "sphere", "ball")
                      .set("type", "polymesh")
                      .set("rows", 40)
                      .set("cols", 80)
                      .expr("rad", {"ch(\"../CTRL/radius\")", "ch(\"../CTRL/radius\")", "ch(\"../CTRL/radius\")"});
    Node bumps = b.sop(geo, "mountain", "bumps")
                     .input(0, sphere)
                     .set("elementsize", 1.2)
                     .expr("height", "ch(\"../CTRL/height\")")
                     .expr("time", "$T * ch(\"../CTRL/speed\")");
    Node tint = color(b, geo, bumps, 0.45, 0.65, 0.9);
    b.sop(geo, "null", "OUT").input(0, tint).display();

    // Another object: a ring around the sphere sized by the same control
    Node ringGeo = b.object("ring");
    Node ring = b.sop(ringGeo, "torus", "ring")
                    .expr("rad",
                        {"ch(\"/obj/shapes/CTRL/radius\") * 1.6", "0.15 + 0.2 * ch(\"/obj/shapes/CTRL/height\")"});
    Node ringColor = color(b, ringGeo, ring, 0.95, 0.75, 0.35);
    b.sop(ringGeo, "null", "OUT").input(0, ringColor).display();
}

// SOP Subnet: a network inside a SOP. Its inputs come in through the input nodes "1".."4" inside it; without Output
// SOPs the subnet outputs its Display SOP. Here input 1 (a torus) is spun inside, input 2 (a grid) is shaped by
// noise, and the two are merged inside the subnet
void sceneSubnet(SceneBuilder& b)
{
    Node geo = b.object("subnet");

    Node torus = b.sop(geo, "torus", "ring").set("rad", 1.6, 0.4);
    Node grid = b.sop(geo, "grid", "floor").set("size", 8, 8).set("rows", 40).set("cols", 40).set("t", 0, -2, 0);
    Node subnet =
        b.sop(geo, "subnet", "shape_both").input(0, torus).input(1, grid).set("label1", "Ring").set("label2", "Floor");

    Node in1 = b.subnetInput(subnet, 1);
    Node in2 = b.subnetInput(subnet, 2);
    Node spin = b.sop(subnet, "xform", "spin")
                    .input(0, in1)
                    .expr("r", {"$T * 50", "$T * 30", "0"})
                    .expr("t", 1, "0.5 * sin($T * 2)");
    Node spinColor = color(b, subnet, spin, 0.95, 0.6, 0.3);
    Node waves = b.sop(subnet, "mountain", "waves").input(0, in2).set("elementsize", 2.5).expr("time", "$T * 0.5");
    Node wavesColor = color(b, subnet, waves, 0.4, 0.7, 0.95);
    merge(b, subnet, {spinColor, wavesColor}, "result").display();

    subnet.display();
}

// Output: Output SOPs choose what the outputs of a subnet are (Output Index 0, 1, ...). The subnet splits a sphere
// into two halves (two Clip SOPs) and gives each half to its own output; outside the halves are moved apart
void sceneOutput(SceneBuilder& b)
{
    Node geo = b.object("output");

    Node sphere =
        b.sop(geo, "sphere", "ball").set("type", "polymesh").set("rows", 32).set("cols", 64).set("rad", 2, 2, 2);
    Node split = b.sop(geo, "subnet", "split").input(0, sphere);
    Node in = b.subnetInput(split, 1);
    Node top = b.sop(split, "clip", "top").input(0, in).set("dofill", 1).expr("dir", {"sin($T * 0.5)", "1", "0"});
    Node bottom = b.sop(split, "clip", "bottom")
                      .input(0, in)
                      .set("dofill", 1)
                      .set("clipop", "below")
                      .expr("dir", {"sin($T * 0.5)", "1", "0"});
    b.sop(split, "output", "output0").input(0, top).set("outputidx", 0);
    b.sop(split, "output", "output1").input(0, bottom).set("outputidx", 1);

    // Outputs 0 and 1 of the subnet
    Node up = b.sop(geo, "xform", "move_up").input(0, split, 0).expr("t", 1, "0.2 + 0.8 * (0.5 + 0.5 * sin($T * 1.5))");
    Node down =
        b.sop(geo, "xform", "move_down").input(0, split, 1).expr("t", 1, "-0.2 - 0.8 * (0.5 + 0.5 * sin($T * 1.5))");
    Node upColor = color(b, geo, up, 0.95, 0.6, 0.3);
    Node downColor = color(b, geo, down, 0.4, 0.7, 0.95);
    merge(b, geo, {upColor, downColor}).display();
}

// Geo objects: every object is a SOP network with its own transform (the Transform tab: t, r, s, pivot p, uniform
// scale). The geometry stays in the space of the object; the object matrix moves it in the world
void sceneGeoObject(SceneBuilder& b)
{
    struct Obj
    {
        const char* name;
        const char* op;
        double r, g, bl;
    };
    const Obj objs[] = {{"spinner", "box", 0.95, 0.55, 0.3}, {"bouncer", "sphere", 0.4, 0.8, 0.5},
        {"squasher", "torus", 0.35, 0.6, 0.95}, {"orbiter", "box", 0.95, 0.9, 0.4}};
    Node obj[4];
    for (int k = 0; k < 4; ++k) {
        obj[k] = b.object(objs[k].name);
        Node shape = b.sop(obj[k], objs[k].op, "shape");
        if (k == 1)
            shape.set("type", "polymesh");
        color(b, obj[k], shape, objs[k].r, objs[k].g, objs[k].bl).display();
    }
    // Rotation about a tilted axis
    obj[0].set("t", -4.5, 0, 0).expr("r", {"$T * 40", "$T * 70", "20"}).set("scale", 1.5);
    // A bounce: |sin| lifts, the squash at the bottom is a scale about the pivot at the foot
    obj[1]
        .set("t", -1.5, 0, 0)
        .expr("t", 1, "abs(sin($T * 3)) * 2")
        .set("p", 0, -1, 0)
        .expr("s", {"1 + 0.3 * exp(-8 * abs(sin($T * 3)))", "1 - 0.3 * exp(-8 * abs(sin($T * 3)))",
                       "1 + 0.3 * exp(-8 * abs(sin($T * 3)))"});
    // Non-uniform scale and a different transform order
    obj[2]
        .set("t", 2, 0, 0)
        .set("xOrd", "trs")
        .expr("s", {"1 + 0.4 * sin($T * 2)", "1", "1 - 0.4 * sin($T * 2)"})
        .expr("r", 0, "90");
    // A circular orbit by expressions on translate
    obj[3]
        .expr("t", {"5 + 1.5 * cos($T * 2)", "0.5 * sin($T * 4)", "1.5 * sin($T * 2)"})
        .expr("r", 1, "-$T * 114.6")
        .set("scale", 0.6);
}

// Null objects: objects without geometry used as parents (input 0 of an object — its parent; the world matrix of the
// child is its local matrix times the parent's). A system: a tilting root, an orbit pivot carrying the planet, an
// orbit pivot of the moon parented to the planet, and a ring parented to the planet
void sceneNullObject(SceneBuilder& b)
{
    Node tilt = b.object("tilt", "null").expr("r", 2, "15 * sin($T * 0.5)");

    Node sun = b.object("sun");
    Node sunBody = b.sop(sun, "sphere", "body").set("type", "poly").set("freq", 4).set("rad", 1.2, 1.2, 1.2);
    color(b, sun, sunBody, 1, 0.75, 0.25).display();
    sun.input(0, tilt).expr("r", 1, "$T * 10");

    Node orbit = b.object("planet_orbit", "null").input(0, tilt).expr("r", 1, "$T * 40");
    Node planet = b.object("planet").input(0, orbit).set("t", 5, 0, 0).expr("r", 1, "$T * 90");
    Node planetBody = b.sop(planet, "sphere", "body")
                          .set("type", "polymesh")
                          .set("rows", 16)
                          .set("cols", 32)
                          .set("rad", 0.6, 0.6, 0.6);
    color(b, planet, planetBody, 0.35, 0.6, 0.95).display();

    Node ring = b.object("ring").input(0, planet).set("r", 25, 0, 0);
    Node ringBody = b.sop(ring, "torus", "body").set("rad", 1, 0.06).set("cols", 48);
    color(b, ring, ringBody, 0.85, 0.8, 0.7).display();

    Node moonOrbit = b.object("moon_orbit", "null").input(0, planet).expr("r", 1, "$T * 160");
    Node moon = b.object("moon").input(0, moonOrbit).set("t", 1.6, 0, 0);
    Node moonBody = b.sop(moon, "sphere", "body").set("type", "poly").set("freq", 2).set("rad", 0.2, 0.2, 0.2);
    color(b, moon, moonBody, 0.8, 0.8, 0.8).display();
}

} // namespace demo::scenes
