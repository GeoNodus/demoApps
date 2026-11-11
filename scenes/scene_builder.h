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
#include <utility>
#include <vector>

namespace demo::scenes {

class SceneBuilder;

// A user (spare) parameter of any kind for Node::addSpareParm: GN_SpareParmTemplate filled step by step (the strings are
// literals or outlive the call):
//   node.addSpareParm(SpareParm(GN_PARMTYPE_INT, "count", "Count").range(1, 10).joinNext())
struct SpareParm
{
    SpareParm(GN_ParmType type, const char* name, const char* label = nullptr)
    {
        t.type = type;
        t.name = name;
        t.label = label;
        t.size = 1;
    }
    // Components of INT, FLOAT, STRING (1..4)
    SpareParm& size(int n)
    {
        t.size = n;
        return *this;
    }
    // INT, FLOAT, TOGGLE (≠ 0 — on), menu — the item index
    SpareParm& defaults(std::initializer_list<double> values)
    {
        int k = 0;
        for (double v : values)
            if (k < 4)
                t.defaults[k++] = v;
        return *this;
    }
    SpareParm& stringDefaults(std::initializer_list<const char*> values)
    {
        int k = 0;
        for (const char* v : values)
            if (k < 4)
                t.stringDefaults[k++] = v;
        return *this;
    }
    // Slider range; a strict bound also clamps the value
    SpareParm& range(double min, double max, bool minStrict = false, bool maxStrict = false)
    {
        t.hasRange = true;
        t.min = min;
        t.max = max;
        t.minIsStrict = minStrict;
        t.maxIsStrict = maxStrict;
        return *this;
    }
    // Menu items of an INT: token and label; the value is the item index
    SpareParm& menu(std::initializer_list<std::pair<const char*, const char*>> items)
    {
        for (const auto& item : items) {
            tokens.push_back(item.first);
            labels.push_back(item.second);
        }
        return *this;
    }
    // FOLDER: GN_PRM_SCRIPT_TYPE_GROUP (a tab, the default), GROUPSIMPLE, GROUPCOLLAPSIBLE
    SpareParm& folder(GN_PrmScriptType type)
    {
        t.folderType = type;
        return *this;
    }
    // The folder (by name) the parameter goes into; without it — the root
    SpareParm& parent(const char* folderName)
    {
        parentFolder = folderName;
        return *this;
    }
    SpareParm& joinNext()
    {
        t.joinNext = true;
        return *this;
    }
    SpareParm& labelNone()
    {
        t.labelNone = true;
        return *this;
    }
    SpareParm& disableWhen(const char* condition)
    {
        t.disableWhen = condition;
        return *this;
    }
    SpareParm& hideWhen(const char* condition)
    {
        t.hideWhen = condition;
        return *this;
    }

    GN_SpareParmTemplate t{};
    const char* parentFolder = nullptr;
    std::vector<const char*> tokens, labels;
};

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
    // The same for one component of a string parameter
    Node& setString(const char* parm, int component, const char* value);

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
    // A user parameter of any kind (gnAddSpareParm), see SpareParm
    Node& addSpareParm(const SpareParm& parm);

    // --- Geometry given by the application (gnSetPartInfo ... gnCommitGeo, as in scenes_input.cpp): one part of an
    // input node or any SOP; describe it, then commit() locks the node with it

    // Polygons: the vertex count of each face and the point of each vertex, face after face
    Node& mesh(int points, const std::vector<int>& faceCounts, const std::vector<int>& vertices);
    // Curves of one type (GN_CURVETYPE_LINEAR / NURBS / BEZIER): orders — one for all or one per curve (not used by
    // LINEAR), knots — of open NURBS (order + CV count per curve), empty — uniform
    Node& curves(GN_CurveType type, int points, const std::vector<int>& curveCounts, const std::vector<int>& vertices,
        const std::vector<int>& orders, bool closed, const std::vector<float>& knots = {});
    // Attributes: `size` values per element of the owner, element after element
    Node& attrib(GN_AttributeOwner owner, const char* name, int size, GN_AttributeTypeInfo typeInfo,
        const std::vector<float>& values);
    Node& attribFloat64(GN_AttributeOwner owner, const char* name, int size, GN_AttributeTypeInfo typeInfo,
        const std::vector<double>& values);
    Node& attribInt(GN_AttributeOwner owner, const char* name, int size, GN_AttributeTypeInfo typeInfo,
        const std::vector<int>& values);
    Node& attribInt64(GN_AttributeOwner owner, const char* name, int size, GN_AttributeTypeInfo typeInfo,
        const std::vector<GN_Int64>& values);
    Node& attribString(GN_AttributeOwner owner, const char* name, int size, const std::vector<const char*>& values);
    // A point or primitive group (the numbers of its elements) or an edge group (pairs of point numbers)
    Node& group(GN_GroupType type, const char* name, const std::vector<int>& elements);
    // Builds the described geometry and locks the node with it (gnCommitGeo)
    Node& commit();

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
    // The current time in seconds ($T)
    void time(double seconds);

    // An object on /obj: "geo" (a SOP network) or "null"
    Node object(const char* name, const char* op = "geo");
    // A SOP inside a geo object or a SOP subnet
    Node sop(const Node& parent, const char* op, const char* name = nullptr);
    // A node inside a SOP subnet: its input "1".."4" (the subnet's input `index` from 1)
    Node subnetInput(const Node& subnet, int index);
    // An input node (gnCreateInputNode) in a geo object or a SOP subnet, named `name` (the library names it
    // "input_<name>"; it is renamed); its geometry is set with Node::mesh / curves ... commit
    Node inputNode(const Node& parent, const char* name);

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
    void setString(GN_NodeId node, const char* parm, int component, const char* value);
    GN_AttributeInfo addAttribute(GN_NodeId node, GN_AttributeOwner owner, const char* name, int size,
        GN_AttributeTypeInfo typeInfo, GN_StorageType storage, int valueCount);
    void beginPart(GN_NodeId node, const GN_PartInfo& part);
    void setExpression(GN_NodeId node, const char* parm, int component, const char* expression);
    void error(GN_NodeId node, const std::string& text);

    GN_SessionId m_session;
    std::vector<std::string> m_errors;
    std::vector<MenuChoice> m_menu_choices;
    // The part being described (Node::mesh / curves): its point and primitive counts for the groups
    GN_NodeId m_part_node = -1;
    int m_part_points = 0, m_part_prims = 0;
};

} // namespace demo::scenes
