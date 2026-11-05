#include "demo_scenes_c.h"

#include "demo_scenes.h"

#include <cstring>
#include <string>

using namespace demo::scenes;

namespace {

const DemoScene* sceneAt(int index)
{
    int count = 0;
    const DemoScene* list = demoScenes(&count);
    return index >= 0 && index < count ? &list[index] : nullptr;
}

} // namespace

int demoSceneCount(void)
{
    int count = 0;
    demoScenes(&count);
    return count;
}

const char* demoSceneNodeType(int index)
{
    const DemoScene* s = sceneAt(index);
    return s ? s->nodeType : nullptr;
}

const char* demoSceneDescription(int index)
{
    const DemoScene* s = sceneAt(index);
    return s ? s->description : nullptr;
}

int demoSceneFind(const char* nodeType)
{
    int count = 0;
    const DemoScene* list = demoScenes(&count);
    const DemoScene* found = findDemoScene(nodeType);
    return found ? static_cast<int>(found - list) : -1;
}

int demoSceneBuild(GN_SessionId session, int index, char* errors, int length)
{
    if (errors && length > 0)
        errors[0] = '\0';
    const DemoScene* s = sceneAt(index);
    if (!s)
        return -1;
    // The scene function replaces the graph with its own nodes through the API (gnCreateNode, gnSetParm*, ...)
    SceneBuilder builder(session);
    buildDemoScene(*s, builder);
    std::string text;
    for (const std::string& e : builder.errors())
        text += e + "\n";
    if (errors && length > 0) {
        const size_t n = text.size() < static_cast<size_t>(length - 1) ? text.size() : static_cast<size_t>(length - 1);
        std::memcpy(errors, text.data(), n);
        errors[n] = '\0';
    }
    return static_cast<int>(builder.errors().size());
}
