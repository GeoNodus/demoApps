#pragma once

// Demo scenes built in code with the GeoNodus API — one function per node type. Each function fills an empty session:
// creates the objects and SOPs, sets parameters and expressions of time (so the scene changes when played) and turns
// on the Display flags; the node type it is about is always used, other nodes only help to show it. A scene keeps
// within the node limit of the demo edition of the library (20 nodes).
//
// The registry is platform independent: the apps of every OS show it as a list of node types (on Windows — a combo
// box). The File SOP has no scene: loading files is what the Open button of the apps is for. Ready-made JSON scenes
// (gnLoadGraph) are next to this file.
#include "scene_builder.h"

namespace demo::scenes {

struct DemoScene
{
    // the operator the scene shows, as gnCreateNode takes it ("sphere", "Object/null", ...); "input" — an input node
    // of gnCreateInputNode
    const char* nodeType;
    const char* description; // one line for the UI
    void (*build)(SceneBuilder& b);
};

// All scenes in UI order
const DemoScene* demoScenes(int* count);
// The scene of a node type (case-insensitive), nullptr — none
const DemoScene* findDemoScene(const char* nodeType);

// Builds a scene into the session: clears /obj, sets the timeline (0..10 s at 24 fps unless the scene sets its own)
// and calls scene.build. The errors of the build (unknown parameters, failed calls) are in b.errors()
void buildDemoScene(const DemoScene& scene, SceneBuilder& b);

// --- Scene functions (scenes_*.cpp) ---------------------------------------------------------------------------------

// Objects and networks — scenes_network.cpp
void sceneGeoObject(SceneBuilder& b);
void sceneNullObject(SceneBuilder& b);
void sceneNull(SceneBuilder& b);
void sceneSubnet(SceneBuilder& b);
void sceneOutput(SceneBuilder& b);
void sceneMerge(SceneBuilder& b);

// Generators — scenes_primitives.cpp
void sceneSphere(SceneBuilder& b);
void sceneBox(SceneBuilder& b);
void sceneGrid(SceneBuilder& b);
void sceneTorus(SceneBuilder& b);
void sceneCircle(SceneBuilder& b);
void sceneAdd(SceneBuilder& b);

// Attributes — scenes_attributes.cpp
void sceneAttribCreate(SceneBuilder& b);
void sceneAttribRandomize(SceneBuilder& b);
void sceneAttribBlur(SceneBuilder& b);
void sceneNormal(SceneBuilder& b);
void sceneTexture(SceneBuilder& b);

// Groups — scenes_groups.cpp
void sceneGroupCreate(SceneBuilder& b);
void sceneGroupRange(SceneBuilder& b);

// Deformers — scenes_deform.cpp
void sceneXform(SceneBuilder& b);
void sceneMountain(SceneBuilder& b);
void sceneBend(SceneBuilder& b);
void scenePathDeform(SceneBuilder& b);
void sceneMatchSize(SceneBuilder& b);
void sceneFuse(SceneBuilder& b);

// Modeling — scenes_modeling.cpp
void sceneClip(SceneBuilder& b);
void scenePolyExtrude(SceneBuilder& b);
void scenePolyBevel(SceneBuilder& b);
void sceneBoolean(SceneBuilder& b);

// Copies and points — scenes_copy.cpp
void sceneScatter(SceneBuilder& b);
void sceneCopyToPoints(SceneBuilder& b);
void sceneCopyXform(SceneBuilder& b);

// Curves — scenes_curves.cpp
void sceneFit(SceneBuilder& b);
void sceneResample(SceneBuilder& b);
void sceneCarve(SceneBuilder& b);
void sceneJoin(SceneBuilder& b);
void sceneOrientAlongCurve(SceneBuilder& b);

// Input geometry made in code (gnCreateInputNode and the geometry setters) — scenes_input.cpp
void sceneInputGeometry(SceneBuilder& b);
void sceneInputBoolean(SceneBuilder& b);

} // namespace demo::scenes
