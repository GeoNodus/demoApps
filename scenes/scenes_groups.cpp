// Groups: Group Create, Group Range
#include "demo_scenes.h"
#include "scene_util.h"

namespace demo::scenes {

// Group Create: a point group by a bounding box flying through a sphere, a primitive group by normals (faces of a
// torus turned towards a rotating direction); every group is recolored by an Attribute Create limited to it
void sceneGroupCreate(SceneBuilder& b)
{
    Node geo = b.object("groupcreate");

    // Keep in Bounding Regions: points inside a moving box
    Node sphere = b.sop(geo, "sphere", "ball")
                      .set("type", "polymesh")
                      .set("rows", 40)
                      .set("cols", 80)
                      .set("rad", 2.2, 2.2, 2.2)
                      .set("t", -3.5, 0, 0);
    Node sphereBase = color(b, geo, sphere, 0.3, 0.4, 0.6);
    Node inBox = b.sop(geo, "groupcreate", "in_box")
                     .input(0, sphereBase)
                     .set("groupname", "inbox")
                     .set("grouptype", "point")
                     .set("groupbase", 0)
                     .set("groupbounding", 1)
                     .set("boundtype", "usebbox")
                     .expr("size", {"2.4", "2.4 + 0.8 * sin($T * 2)", "2.4"})
                     // The box sweeps the half of the sphere that faces the camera
                     .expr("t", {"-3.5 + 2.2 * cos(0.785 + 1.2 * sin($T * 0.8))", "1.2 * sin($T * 1.3)",
                                    "2.2 * sin(0.785 + 1.2 * sin($T * 0.8))"});
    b.sop(geo, "attribcreate", "in_box_color")
        .input(0, inBox)
        .set("group", "inbox")
        .set("grouptype", "points")
        .set("name1", "Cd")
        .set("class1", "point")
        .set("type1", "vector")
        .set("size1", 3)
        .set("value1v", 1, 0.45, 0.2, 0)
        .display();

    // Keep by Normals: faces within 50 ± 20° of a turning direction. A separate object: primitive colors are not mixed
    // with the point colors of the sphere
    Node torusGeo = b.object("groupcreate_normals");
    Node torus =
        b.sop(torusGeo, "torus", "ring").set("rad", 1.8, 0.7).set("rows", 24).set("cols", 48).set("t", 3.5, 0, 0);
    Node torusBase = primColor(b, torusGeo, torus, 0.3, 0.55, 0.35);
    Node facing = b.sop(torusGeo, "groupcreate", "facing")
                      .input(0, torusBase)
                      .set("groupname", "facing")
                      .set("grouptype", "primitive")
                      .set("groupbase", 0)
                      .set("groupnormal", 1)
                      .expr("dir", {"cos(0.785 + 1.2 * sin($T * 0.8))", "0.6 * sin($T * 0.7)",
                                       "sin(0.785 + 1.2 * sin($T * 0.8))"})
                      .expr("angle", "50 + 20 * sin($T * 1.5)");
    b.sop(torusGeo, "attribcreate", "facing_color")
        .input(0, facing)
        .set("group", "facing")
        .set("grouptype", "prims")
        .set("name1", "Cd")
        .set("class1", "primitive")
        .set("type1", "vector")
        .set("size1", 3)
        .set("value1v", 1, 0.9, 0.3, 0)
        .display();
}

// Group Range: the "Select N of M" filter with a running offset on the faces of a sphere, and Start and Length on the
// faces of a grid — the window runs along it
void sceneGroupRange(SceneBuilder& b)
{
    Node geo = b.object("grouprange");

    Node sphere = b.sop(geo, "sphere", "ball")
                      .set("type", "polymesh")
                      .set("rows", 20)
                      .set("cols", 40)
                      .set("rad", 2.2, 2.2, 2.2)
                      .set("t", -3.5, 0, 0);
    Node sphereBase = primColor(b, geo, sphere, 0.25, 0.35, 0.55);
    Node stripes = b.sop(geo, "grouprange", "n_of_m")
                       .input(0, sphereBase)
                       .set("groupname1", "stripes")
                       .set("grouptype1", "prims")
                       .set("selectamount1", 2)
                       .expr("selecttotal1", "rint(6 + 3 * sin($T * 0.4))")
                       .expr("selectoffset1", "rint($T * 6)");
    Node stripesColor = b.sop(geo, "attribcreate", "stripes_color")
                            .input(0, stripes)
                            .set("group", "stripes")
                            .set("grouptype", "prims")
                            .set("name1", "Cd")
                            .set("class1", "primitive")
                            .set("type1", "vector")
                            .set("size1", 3)
                            .set("value1v", 1, 0.6, 0.2, 0);

    Node grid = b.sop(geo, "grid", "floor").set("size", 5, 5).set("rows", 13).set("cols", 13).set("t", 3.5, 0, 0);
    Node gridBase = primColor(b, geo, grid, 0.3, 0.3, 0.3);
    Node window = b.sop(geo, "grouprange", "window")
                      .input(0, gridBase)
                      .set("groupname1", "window")
                      .set("grouptype1", "prims")
                      .set("method1", "length")
                      .expr("start1", "rint(60 + 60 * sin($T * 0.5))")
                      .expr("length1", "rint(14 + 10 * sin($T * 1.3))");
    Node windowColor = b.sop(geo, "attribcreate", "window_color")
                           .input(0, window)
                           .set("group", "window")
                           .set("grouptype", "prims")
                           .set("name1", "Cd")
                           .set("class1", "primitive")
                           .set("type1", "vector")
                           .set("size1", 3)
                           .set("value1v", 0.3, 0.9, 1, 0);

    merge(b, geo, {stripesColor, windowColor}).display();
}

} // namespace demo::scenes
