#ifndef DEMO_SCENES_C_H
#define DEMO_SCENES_C_H

// C interface over the demo scene registry (../../../scenes/demo_scenes.h, C++) for the Swift app: Swift imports it
// through the bridging header together with geonodus.h. The scenes themselves are the same code as on Windows
#include <geonodus.h>

#ifdef __cplusplus
extern "C" {
#endif

// Number of demo scenes (one per node type, in UI order)
int demoSceneCount(void);
// The node type of scene `index` ("sphere", "Object/null", ...) and its one-line description; nullptr — no such scene
const char* demoSceneNodeType(int index);
const char* demoSceneDescription(int index);
// The scene of a node type (case-insensitive), -1 — none
int demoSceneFind(const char* nodeType);

// Builds scene `index` into the session (clears /obj, sets the timeline, creates the nodes; no cooking). Returns the
// number of build problems (0 — all fine, -1 — no such scene); their text, one per line, goes into `errors` (truncated
// to `length` bytes, always zero-terminated)
int demoSceneBuild(GN_SessionId session, int index, char* errors, int length);

#ifdef __cplusplus
}
#endif

#endif // DEMO_SCENES_C_H
