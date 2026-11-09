#pragma once

// A thin helper over the GeoNodus C API for building demo scenes in code. Every call is a plain gn* call: nodes are
// created with gnCreateNode, connected with gnConnectNodeInput, parameters are found by name in gnGetParameters and
// written with gnSetParm*Values / gnSetParmStringValue / gnSetParmExpression, the Display flag is gnSetNodeDisplay.
// The helper only saves the boilerplate of looking parameters up by name; read it as an example of how to drive the
// API. Errors (an unknown parameter, a failed call) do not stop the build: they are collected and returned by
// SceneBuilder::errors().
//
// Platform independent: only the C++ standard library and geonodus.h.
#include <geonodus.h>

#include <initializer_list>
#include <string>
#include <vector>

namespace demo::scenes {

class SceneBuilder;

// A node of the scene being built. Setters return *this, so a node is configured in one expression:
//   b.sop(geo, "sphere", "ball").set("type", "polymesh").set("rows", 24).expr("rad", 0, "1 + 0.2 * sin($T * 3)");
class Node
{
public:
    Node() = default;
    Node(SceneBuilder* builder, GN_NodeId id)
        : m_b(builder)
        , m_id(id)
    {}

    GN_NodeId id() const { return m_id; }
    bool valid() const { return m_b && m_id >= 0; }
    // The node name (for ch() paths and messages)
    std::string name() const;

    // Numeric values of the first components of a parameter: int, toggle (0 / 1), menu (item index), float, multiparm
    // (number of instances)
    Node& set(const char* parm, double x);
    // Integers and toggles: set("closed0", 0) — without it the literal 0 would also match the string overload
    Node& set(const char* parm, int x) { return set(parm, static_cast<double>(x)); }
    Node& set(const char* parm, double x, double y);
    Node& set(const char* parm, double x, double y, double z);
    Node& set(const char* parm, double x, double y, double z, double w);
    // One component of a parameter
    Node& setComponent(const char* parm, int component, double value);
    // A string parameter or, for a menu, the item token ("polymesh", "subtract", ...)
    Node& set(const char* parm, const char* value);

    // An expression on a parameter component (muParser: $T — time in seconds, $F — frame, sin / cos in radians,
    // ch("path") — the value of another parameter, a path relative to this node or absolute)
    Node& expr(const char* parm, int component, const char* expression);
    Node& expr(const char* parm, const char* expression) { return expr(parm, 0, expression); }
    // Expressions on several components at once; nullptr skips a component
    Node& expr(const char* parm, std::initializer_list<const char*> expressions);

    // Presses a button parameter (gnSetParmIntValues on a GN_PARMTYPE_BUTTON)
    Node& press(const char* parm);

    // Connects input `input` of this node to output `output` of `from`
    Node& input(int input, const Node& from, int output = 0);
    // The Display flag: for a SOP — the node shown as the network's geometry, for an object — visibility
    Node& display(bool on = true);

    // A user (spare) float parameter: a control that other nodes read with ch() (HDK: addSpareParmTuple)
    Node& addFloatParm(const char* name, const char* label, double value, double min, double max);

private:
    friend class SceneBuilder;
    SceneBuilder* m_b = nullptr;
    GN_NodeId m_id = -1;
};

class SceneBuilder
{
public:
    explicit SceneBuilder(GN_SessionId session)
        : m_session(session)
    {}

    GN_SessionId session() const { return m_session; }

    // Removes every node under /obj (the scene is built from an empty session)
    void clear();
    // Timeline in seconds (as the "Global Animation Options" of Houdini); the scene starts at `start`
    void timeline(double start, double end, double fps = 24.0);

    // An object on /obj: "geo" (a SOP network) or "null"
    Node object(const char* name, const char* op = "geo");
    // A SOP inside a geo object or a SOP subnet
    Node sop(const Node& parent, const char* op, const char* name = nullptr);
    // A node inside a SOP subnet: its input "1".."4" (the subnet's input `index` from 1)
    Node subnetInput(const Node& subnet, int index);

    // Checks the result of a gn* call made directly by a scene function (the input geometry setters of
    // scenes_input.cpp): false — the call failed, the error (with the GN_STATUS_CALL_RESULT text) goes to errors()
    bool check(GN_Result result, GN_NodeId node, const char* call);

    const std::vector<std::string>& errors() const { return m_errors; }
    // Every (node, parameter, token) of the menu items set by token: the build is checked against the saved graph
    struct MenuChoice
    {
        GN_NodeId node;
        std::string parm, token;
    };
    const std::vector<MenuChoice>& menuChoices() const { return m_menu_choices; }

private:
    friend class Node;

    struct Parm
    {
        GN_ParmInfo info{};
        bool found = false;
    };
    Parm findParm(GN_NodeId node, const char* name);
    std::string string(GN_StringHandle handle) const;
    std::string nodeName(GN_NodeId node) const;
    Node create(GN_NodeId parent, const char* op, const char* name);
    void setValues(GN_NodeId node, const char* parm, int first, int count, const double* values);
    void setString(GN_NodeId node, const char* parm, const char* value);
    void setExpression(GN_NodeId node, const char* parm, int component, const char* expression);
    void error(GN_NodeId node, const std::string& text);

    GN_SessionId m_session;
    std::vector<std::string> m_errors;
    std::vector<MenuChoice> m_menu_choices;
};

} // namespace demo::scenes
