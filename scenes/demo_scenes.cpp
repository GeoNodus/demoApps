#include "demo_scenes.h"

#include <cctype>

namespace demo::scenes {

namespace {

const DemoScene SCENES[] = {
    // Generators
    {"sphere", "Polygon mesh and icosahedron spheres, a sphere bounding a moving grid", sceneSphere},
    {"box", "Boxes: divisions, size and rotation; a box around a spinning torus", sceneBox},
    {"grid", "Grids of every connectivity, rows and columns animated", sceneGrid},
    {"torus", "Tori: radii and divisions, a ring of tori", sceneTorus},
    {"circle", "Polygon arcs of every type and exact NURBS / Bezier circles", sceneCircle},
    {"add", "Points of a multiparm moved by expressions, a star polygon and a polyline", sceneAdd},
    // Attributes
    {"attribcreate", "Point colors from expressions, a color only on a group", sceneAttribCreate},
    {"attribrandomize", "Random point colors and sizes with a changing seed", sceneAttribRandomize},
    {"attribblur", "Blurring random colors and smoothing a noisy terrain", sceneAttribBlur},
    {"normal", "Point normals of a moving surface shown by spikes copied along them", sceneNormal},
    {"texture", "UV projections written into Cd, so the coordinates are seen as colors", sceneTexture},
    // Groups
    {"groupcreate", "Groups by a moving bounding box and by normals", sceneGroupCreate},
    {"grouprange", "Every N of M primitives with a running offset", sceneGroupRange},
    // Deformers
    {"xform", "Transform SOP: a twisted column of transforms of point groups", sceneXform},
    {"mountain", "Noise displacement of a grid and a sphere, flowing in time", sceneMountain},
    {"bend", "Bending, twisting and tapering a column", sceneBend},
    {"pathdeform", "A tapered rolling column along a waving curve and a twisted band sliding around a circle",
        scenePathDeform},
    {"matchsize", "Shapes of different sizes fitted into a pulsing box", sceneMatchSize},
    {"fuse", "Snapping points to a grid of changing spacing", sceneFuse},
    // Modeling
    {"clip", "A sphere cut by a rotating plane, with a cap", sceneClip},
    {"polyextrude", "Faces extruded and inset with a twist, a curve extruded into a ribbon", scenePolyExtrude},
    {"polybevel", "Rounded edges of a box with changing distance and divisions", scenePolyBevel},
    {"boolean", "A box minus a moving sphere, a union and an intersection", sceneBoolean},
    // Copies and points
    {"scatter", "Points scattered over a torus, their count growing", sceneScatter},
    {"copytopoints", "Boxes copied onto the points of a waving grid, oriented by N", sceneCopyToPoints},
    {"copyxform", "A spiral staircase of copies, the number of steps animated", sceneCopyXform},
    // Curves
    {"fit", "Curves fitted through moving points: NURBS interpolation and approximation", sceneFit},
    {"resample", "A curve resampled by a changing segment length", sceneResample},
    {"carve", "Growing pieces of a circle and of a polyline", sceneCarve},
    {"join", "Two moving curves joined with a blend", sceneJoin},
    {"orientalongcurve", "Frames along a curve with a roll; boxes copied along it", sceneOrientAlongCurve},
    // Networks and objects
    {"merge", "Several generators merged into one geometry", sceneMerge},
    {"null", "A SOP Null as a control node: user parameters read by ch() in other nodes", sceneNull},
    {"subnet", "A SOP subnet: inputs inside the subnet, two outputs", sceneSubnet},
    {"output", "Output SOPs choosing the outputs of a subnet", sceneOutput},
    {"Object/geo", "Geometry objects with their own animated transforms", sceneGeoObject},
    {"Object/null", "Null objects as parents: a hierarchy of rotating objects", sceneNullObject},
};

bool equalNoCase(const char* a, const char* b)
{
    for (; *a && *b; ++a, ++b)
        if (std::tolower(static_cast<unsigned char>(*a)) != std::tolower(static_cast<unsigned char>(*b)))
            return false;
    return *a == *b;
}

} // namespace

const DemoScene* demoScenes(int* count)
{
    if (count)
        *count = static_cast<int>(sizeof(SCENES) / sizeof(SCENES[0]));
    return SCENES;
}

const DemoScene* findDemoScene(const char* nodeType)
{
    if (!nodeType)
        return nullptr;
    for (const DemoScene& s : SCENES)
        if (equalNoCase(s.nodeType, nodeType))
            return &s;
    return nullptr;
}

void buildDemoScene(const DemoScene& scene, SceneBuilder& b)
{
    b.clear();
    b.timeline(0.0, 10.0, 24.0);
    scene.build(b);
}

} // namespace demo::scenes
