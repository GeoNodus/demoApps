#include "scene_builder.h"

#include <cstdio>
#include <cstring>

namespace demo::scenes {

namespace {

const char* resultName(GN_Result r)
{
    switch (r) {
    case GN_RESULT_SUCCESS:
        return "success";
    case GN_RESULT_FAILURE:
        return "failure";
    case GN_RESULT_PARM_SET_FAILED:
        return "parm set failed";
    case GN_RESULT_INVALID_ARGUMENT:
        return "invalid argument";
    case GN_RESULT_NODE_INVALID:
        return "node invalid";
    case GN_RESULT_INVALID_SESSION:
        return "invalid session";
    case GN_RESULT_DEMO_LIMIT:
        return "demo edition limit";
    default:
        return "error";
    }
}

bool isFolderLike(GN_ParmType t)
{
    return t == GN_PARMTYPE_FOLDER || t == GN_PARMTYPE_FOLDERLIST || t == GN_PARMTYPE_LABEL
           || t == GN_PARMTYPE_SEPARATOR;
}

bool storesInt(GN_ParmType t)
{
    return t == GN_PARMTYPE_INT || t == GN_PARMTYPE_TOGGLE || t == GN_PARMTYPE_BUTTON || t == GN_PARMTYPE_MULTIPARMLIST
           || t == GN_PARMTYPE_FOLDERLIST_RADIO;
}

bool storesString(GN_ParmType t)
{
    return t == GN_PARMTYPE_STRING || t == GN_PARMTYPE_PATH_FILE || t == GN_PARMTYPE_PATH_FILE_GEO
           || t == GN_PARMTYPE_PATH_FILE_IMAGE || t == GN_PARMTYPE_PATH_FILE_DIR || t == GN_PARMTYPE_NODE;
}

} // namespace

// --- Node -----------------------------------------------------------------------------------------------------------

std::string Node::name() const
{
    return m_b ? m_b->nodeName(m_id) : std::string();
}

Node& Node::set(const char* parm, double x)
{
    if (valid())
        m_b->setValues(m_id, parm, 0, 1, &x);
    return *this;
}

Node& Node::set(const char* parm, double x, double y)
{
    const double v[] = {x, y};
    if (valid())
        m_b->setValues(m_id, parm, 0, 2, v);
    return *this;
}

Node& Node::set(const char* parm, double x, double y, double z)
{
    const double v[] = {x, y, z};
    if (valid())
        m_b->setValues(m_id, parm, 0, 3, v);
    return *this;
}

Node& Node::set(const char* parm, double x, double y, double z, double w)
{
    const double v[] = {x, y, z, w};
    if (valid())
        m_b->setValues(m_id, parm, 0, 4, v);
    return *this;
}

Node& Node::setComponent(const char* parm, int component, double value)
{
    if (valid())
        m_b->setValues(m_id, parm, component, 1, &value);
    return *this;
}

Node& Node::set(const char* parm, const char* value)
{
    if (valid())
        m_b->setString(m_id, parm, value);
    return *this;
}

Node& Node::expr(const char* parm, int component, const char* expression)
{
    if (valid())
        m_b->setExpression(m_id, parm, component, expression);
    return *this;
}

Node& Node::expr(const char* parm, std::initializer_list<const char*> expressions)
{
    int component = 0;
    for (const char* e : expressions) {
        if (e)
            expr(parm, component, e);
        ++component;
    }
    return *this;
}

Node& Node::press(const char* parm)
{
    const double one = 1;
    if (valid())
        m_b->setValues(m_id, parm, 0, 1, &one);
    return *this;
}

Node& Node::input(int input, const Node& from, int output)
{
    if (!valid() || !from.valid())
        return *this;
    const GN_Result r = gnConnectNodeInput(m_b->m_session, m_id, input, from.m_id, output);
    if (r != GN_RESULT_SUCCESS)
        m_b->error(m_id, "gnConnectNodeInput(" + std::to_string(input) + ", " + from.name() + ") — " + resultName(r));
    return *this;
}

Node& Node::display(bool on)
{
    if (!valid())
        return *this;
    const GN_Result r = gnSetNodeDisplay(m_b->m_session, m_id, on ? 1 : 0);
    if (r != GN_RESULT_SUCCESS)
        m_b->error(m_id, std::string("gnSetNodeDisplay — ") + resultName(r));
    return *this;
}

Node& Node::addFloatParm(const char* name, const char* label, double value, double min, double max)
{
    if (!valid())
        return *this;
    GN_SpareParmTemplate t{};
    t.type = GN_PARMTYPE_FLOAT;
    t.name = name;
    t.label = label;
    t.size = 1;
    t.defaults[0] = value;
    t.hasRange = true;
    t.min = min;
    t.max = max;
    GN_ParmId id = -1;
    const GN_Result r = gnAddSpareParm(m_b->m_session, m_id, &t, -1, &id);
    if (r != GN_RESULT_SUCCESS)
        m_b->error(m_id, std::string("gnAddSpareParm(") + name + ") — " + resultName(r));
    return *this;
}

// --- SceneBuilder ---------------------------------------------------------------------------------------------------

void SceneBuilder::clear()
{
    // The children of /obj (id 0); deleting an object deletes its whole network
    int count = 0;
    if (gnComposeChildNodeList(m_session, 0, GN_NODETYPE_ANY, GN_NODEFLAGS_ANY, false, &count) != GN_RESULT_SUCCESS
        || count <= 0)
        return;
    std::vector<GN_NodeId> children(static_cast<size_t>(count));
    gnGetComposedChildNodeList(m_session, 0, children.data(), count);
    for (GN_NodeId child : children)
        gnDeleteNode(m_session, child);
}

void SceneBuilder::timeline(double start, double end, double fps)
{
    GN_TimelineOptions tl{};
    tl.fps = fps;
    tl.startTime = start;
    tl.endTime = end;
    if (gnSetTimelineOptions(m_session, &tl) != GN_RESULT_SUCCESS)
        error(-1, "gnSetTimelineOptions failed");
    gnSetTime(m_session, start);
}

Node SceneBuilder::object(const char* name, const char* op)
{
    return create(0, op, name);
}

Node SceneBuilder::sop(const Node& parent, const char* op, const char* name)
{
    return parent.valid() ? create(parent.m_id, op, name) : Node();
}

Node SceneBuilder::subnetInput(const Node& subnet, int index)
{
    // The input nodes are hidden from the usual child list; the GN_NODEFLAGS_SOP_SUBNET_INPUT filter lists them
    int count = 0;
    if (!subnet.valid()
        || gnComposeChildNodeList(m_session, subnet.m_id, GN_NODETYPE_ANY, GN_NODEFLAGS_SOP_SUBNET_INPUT, false, &count)
               != GN_RESULT_SUCCESS)
        return Node();
    std::vector<GN_NodeId> children(static_cast<size_t>(count));
    if (count > 0)
        gnGetComposedChildNodeList(m_session, subnet.m_id, children.data(), count);
    const std::string wanted = std::to_string(index);
    for (GN_NodeId child : children)
        if (nodeName(child) == wanted)
            return Node(this, child);
    error(subnet.m_id, "no subnet input " + wanted);
    return Node();
}

Node SceneBuilder::create(GN_NodeId parent, const char* op, const char* name)
{
    GN_NodeId id = -1;
    const GN_Result r = gnCreateNode(m_session, parent, op, name, false, &id);
    if (r != GN_RESULT_SUCCESS) {
        error(parent, std::string("gnCreateNode(") + op + ", " + (name ? name : "") + ") — " + resultName(r));
        return Node();
    }
    return Node(this, id);
}

std::string SceneBuilder::string(GN_StringHandle handle) const
{
    int length = 0;
    if (gnGetStringBufLength(m_session, handle, &length) != GN_RESULT_SUCCESS || length <= 0)
        return {};
    std::string s(static_cast<size_t>(length), '\0');
    gnGetString(m_session, handle, s.data(), length);
    s.resize(std::strlen(s.c_str()));
    return s;
}

std::string SceneBuilder::nodeName(GN_NodeId node) const
{
    GN_NodeInfo info{};
    if (node < 0 || gnGetNodeInfo(m_session, node, &info) != GN_RESULT_SUCCESS)
        return "?";
    return string(info.nameSH);
}

SceneBuilder::Parm SceneBuilder::findParm(GN_NodeId node, const char* name)
{
    // The list is read anew every time: inserting multiparm instances shifts the ids of the parameters after them
    Parm result;
    GN_NodeInfo info{};
    if (gnGetNodeInfo(m_session, node, &info) != GN_RESULT_SUCCESS || info.parmCount <= 0)
        return result;
    std::vector<GN_ParmInfo> parms(static_cast<size_t>(info.parmCount));
    if (gnGetParameters(m_session, node, parms.data(), 0, info.parmCount) != GN_RESULT_SUCCESS)
        return result;
    for (const GN_ParmInfo& p : parms)
        if (!isFolderLike(p.type) && string(p.nameSH) == name) {
            result.info = p;
            result.found = true;
            break;
        }
    return result;
}

void SceneBuilder::setValues(GN_NodeId node, const char* parm, int first, int count, const double* values)
{
    const Parm p = findParm(node, parm);
    if (!p.found) {
        error(node, std::string("no parameter '") + parm + "'");
        return;
    }
    const int size = p.info.type == GN_PARMTYPE_FOLDERLIST_RADIO || p.info.type == GN_PARMTYPE_MULTIPARMLIST
                         ? 1
                         : p.info.size;
    if (first + count > size) {
        error(node, std::string("parameter '") + parm + "' has " + std::to_string(size) + " components");
        return;
    }
    GN_Result r = GN_RESULT_FAILURE;
    if (storesInt(p.info.type)) {
        int v[4];
        for (int k = 0; k < count; ++k)
            v[k] = static_cast<int>(values[k] < 0 ? values[k] - 0.5 : values[k] + 0.5);
        r = gnSetParmIntValues(m_session, node, v, p.info.intValuesIndex + first, count);
    } else if (p.info.floatValuesIndex >= 0) {
        float v[4];
        for (int k = 0; k < count; ++k)
            v[k] = static_cast<float>(values[k]);
        r = gnSetParmFloatValues(m_session, node, v, p.info.floatValuesIndex + first, count);
    }
    if (r != GN_RESULT_SUCCESS)
        error(node, std::string("setting '") + parm + "' — " + resultName(r));
}

void SceneBuilder::setString(GN_NodeId node, const char* parm, const char* value)
{
    const Parm p = findParm(node, parm);
    if (!p.found) {
        error(node, std::string("no parameter '") + parm + "'");
        return;
    }
    if (storesString(p.info.type)) {
        const GN_Result r = gnSetParmStringValue(m_session, node, value, p.info.id, 0);
        if (r != GN_RESULT_SUCCESS)
            error(node, std::string("setting '") + parm + "' — " + resultName(r));
        return;
    }
    if (p.info.choiceCount <= 0) {
        error(node, std::string("parameter '") + parm + "' is neither a string nor a menu");
        return;
    }
    // A menu: its items are in the node's flat choice list (gnGetParmChoiceLists) from GN_ParmInfo::choiceIndex,
    // the value of the parameter is the item index; valueSH is the item token
    std::vector<GN_ParmChoiceInfo> choices(static_cast<size_t>(p.info.choiceCount));
    if (gnGetParmChoiceLists(m_session, node, choices.data(), p.info.choiceIndex, p.info.choiceCount)
        != GN_RESULT_SUCCESS) {
        error(node, std::string("gnGetParmChoiceLists for '") + parm + "' failed");
        return;
    }
    for (int k = 0; k < p.info.choiceCount; ++k)
        if (string(choices[static_cast<size_t>(k)].valueSH) == value) {
            const int index = k;
            const GN_Result r = gnSetParmIntValues(m_session, node, &index, p.info.intValuesIndex, 1);
            if (r != GN_RESULT_SUCCESS)
                error(node, std::string("setting '") + parm + "' — " + resultName(r));
            m_menu_choices.push_back({node, parm, value});
            return;
        }
    error(node, std::string("menu '") + parm + "' has no item '" + value + "'");
}

void SceneBuilder::setExpression(GN_NodeId node, const char* parm, int component, const char* expression)
{
    const Parm p = findParm(node, parm);
    if (!p.found) {
        error(node, std::string("no parameter '") + parm + "'");
        return;
    }
    const GN_Result r = gnSetParmExpression(m_session, node, expression, p.info.id, component);
    if (r != GN_RESULT_SUCCESS)
        error(node, std::string("expression on '") + parm + "[" + std::to_string(component) + "]' = " + expression
                        + " — " + resultName(r));
}

void SceneBuilder::error(GN_NodeId node, const std::string& text)
{
    m_errors.push_back(node >= 0 ? nodeName(node) + ": " + text : text);
}

} // namespace demo::scenes
