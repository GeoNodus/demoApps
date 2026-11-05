#pragma once

// Small helpers shared by the scene functions (scenes_*.cpp)
#include "scene_builder.h"

#include <cstdarg>
#include <cstdio>
#include <string>

namespace demo::scenes {

// printf into a std::string: expressions with numbers substituted
inline std::string fmt(const char* format, ...)
{
    char buffer[512];
    va_list args;
    va_start(args, format);
    std::vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    return buffer;
}

// A constant point color Cd (Attribute Create: one attribute — Cd, point class, vector of 3)
inline Node color(SceneBuilder& b, const Node& network, const Node& input, double r, double g, double bl,
    const char* name = nullptr)
{
    return b.sop(network, "attribcreate", name)
        .input(0, input)
        .set("name1", "Cd")
        .set("class1", "point")
        .set("type1", "vector")
        .set("size1", 3)
        .set("value1v", r, g, bl, 0);
}

// The same for primitives: a color per face, which an attribute of the same name on points would hide
inline Node primColor(SceneBuilder& b, const Node& network, const Node& input, double r, double g, double bl,
    const char* name = nullptr)
{
    return color(b, network, input, r, g, bl, name).set("class1", "primitive");
}

// Merge of up to 10 inputs in order
inline Node merge(SceneBuilder& b, const Node& network, std::initializer_list<Node> inputs, const char* name = nullptr)
{
    Node m = b.sop(network, "merge", name);
    int i = 0;
    for (const Node& n : inputs)
        m.input(i++, n);
    return m;
}

} // namespace demo::scenes
