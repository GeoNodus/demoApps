// Input geometry: geometry made by the application itself and handed to GeoNodus, as the input nodes of Houdini Engine
// (HAPI_CreateInputNode and the geometry setters). The mesh and the curve below are computed here, in C++, and given to
// the library point by point and face by face with gnSetPartInfo, gnAddAttribute, gnSetAttribute*Data, gnSetVertexList,
// gnSetFaceCounts, gnAddGroup / gnSetGroupMembership and gnSetCurveInfo / gnSetCurveCounts; gnCommitGeo locks the input
// node with that geometry. Procedural nodes then work on it like on any other geometry and animate it.
#include "demo_scenes.h"
#include "scene_util.h"

#include <cmath>
#include <vector>

namespace demo::scenes {

namespace {

const double kPi = 3.14159265358979323846;

GN_AttributeInfo attribute(GN_AttributeOwner owner, GN_StorageType storage, int count, int tupleSize,
    GN_AttributeTypeInfo typeInfo)
{
    GN_AttributeInfo info{};
    info.exists = true;
    info.owner = owner;
    info.storage = storage;
    info.originalOwner = GN_ATTROWNER_INVALID;
    info.count = count;
    info.tupleSize = tupleSize;
    info.typeInfo = typeInfo;
    return info;
}

// The object of a node (an input node created with parent -1 gets its own geo object "input_<name>")
Node parentOf(SceneBuilder& b, GN_NodeId node)
{
    GN_NodeInfo info{};
    if (!b.check(gnGetNodeInfo(b.session(), node, &info), node, "gnGetNodeInfo"))
        return Node();
    return Node(&b, info.parentId);
}

// A twisted star column: rings of 2 * tips points (tips and notches) going up, quads between the rings and a star
// polygon at each end. Point colors go from blue at the bottom to orange at the top, every face has a string "part"
// ("side" or "cap"), and the tips of the top ring are the point group "crown"
Node starColumn(SceneBuilder& b)
{
    const GN_SessionId s = b.session();
    GN_NodeId node = -1;
    if (!b.check(gnCreateInputNode(s, -1, &node, "star"), -1, "gnCreateInputNode"))
        return Node();

    const int tips = 6, ringSize = 2 * tips, rings = 25;
    const int pointCount = ringSize * rings;
    const int sideFaces = ringSize * (rings - 1);
    const int faceCount = sideFaces + 2;
    const int vertexCount = sideFaces * 4 + 2 * ringSize;

    std::vector<float> positions, colors;
    for (int j = 0; j < rings; ++j) {
        const double t = j / double(rings - 1);
        const double twist = t * kPi * 0.5;
        const double scale = 1.0 - 0.35 * t + 0.15 * std::sin(t * kPi);
        for (int i = 0; i < ringSize; ++i) {
            const double angle = i * kPi / tips + twist;
            const double radius = (i % 2 == 0 ? 1.3 : 0.6) * scale;
            positions.push_back(float(radius * std::cos(angle)));
            positions.push_back(float(t * 4.0));
            positions.push_back(float(radius * std::sin(angle)));
            const double glow = i % 2 == 0 ? 1.0 : 0.8;
            colors.push_back(float((0.25 + 0.75 * t) * glow));
            colors.push_back(float((0.35 + 0.2 * t) * glow));
            colors.push_back(float((0.9 - 0.65 * t) * glow));
        }
    }
    std::vector<int> vertices, faceCounts;
    std::vector<const char*> parts;
    for (int j = 0; j + 1 < rings; ++j)
        for (int i = 0; i < ringSize; ++i) {
            const int a = j * ringSize + i, b2 = j * ringSize + (i + 1) % ringSize;
            vertices.insert(vertices.end(), {a, a + ringSize, b2 + ringSize, b2});
            faceCounts.push_back(4);
            parts.push_back("side");
        }
    for (int i = 0; i < ringSize; ++i) // bottom cap
        vertices.push_back(i);
    for (int i = ringSize - 1; i >= 0; --i) // top cap, the other way round
        vertices.push_back((rings - 1) * ringSize + i);
    faceCounts.insert(faceCounts.end(), {ringSize, ringSize});
    parts.insert(parts.end(), {"cap", "cap"});

    GN_PartInfo part{};
    part.type = GN_PARTTYPE_MESH;
    part.pointCount = pointCount;
    part.vertexCount = vertexCount;
    part.faceCount = faceCount;
    b.check(gnSetPartInfo(s, node, 0, &part), node, "gnSetPartInfo");

    const GN_AttributeInfo p = attribute(GN_ATTROWNER_POINT, GN_STORAGETYPE_FLOAT, pointCount, 3,
        HAPI_ATTRIBUTE_TYPE_POINT);
    b.check(gnAddAttribute(s, node, 0, "P", &p), node, "gnAddAttribute(P)");
    b.check(gnSetAttributeFloatData(s, node, 0, "P", &p, positions.data(), 0, pointCount), node,
        "gnSetAttributeFloatData(P)");
    const GN_AttributeInfo cd = attribute(GN_ATTROWNER_POINT, GN_STORAGETYPE_FLOAT, pointCount, 3,
        HAPI_ATTRIBUTE_TYPE_COLOR);
    b.check(gnAddAttribute(s, node, 0, "Cd", &cd), node, "gnAddAttribute(Cd)");
    b.check(gnSetAttributeFloatData(s, node, 0, "Cd", &cd, colors.data(), 0, pointCount), node,
        "gnSetAttributeFloatData(Cd)");
    const GN_AttributeInfo name = attribute(GN_ATTROWNER_PRIM, GN_STORAGETYPE_STRING, faceCount, 1,
        HAPI_ATTRIBUTE_TYPE_NONE);
    b.check(gnAddAttribute(s, node, 0, "part", &name), node, "gnAddAttribute(part)");
    b.check(gnSetAttributeStringData(s, node, 0, "part", &name, parts.data(), 0, faceCount), node,
        "gnSetAttributeStringData(part)");

    b.check(gnSetVertexList(s, node, 0, vertices.data(), 0, vertexCount), node, "gnSetVertexList");
    b.check(gnSetFaceCounts(s, node, 0, faceCounts.data(), 0, faceCount), node, "gnSetFaceCounts");

    std::vector<int> crown(static_cast<size_t>(pointCount), 0);
    for (int i = 0; i < ringSize; i += 2)
        crown[static_cast<size_t>((rings - 1) * ringSize + i)] = 1;
    b.check(gnAddGroup(s, node, 0, GN_GROUPTYPE_POINT, "crown"), node, "gnAddGroup");
    b.check(gnSetGroupMembership(s, node, 0, GN_GROUPTYPE_POINT, "crown", crown.data(), 0, pointCount), node,
        "gnSetGroupMembership");

    b.check(gnCommitGeo(s, node), node, "gnCommitGeo");
    return Node(&b, node);
}

// A trefoil knot as a closed NURBS curve of order 4 on 24 CVs (closed curves get uniform periodic knots)
Node trefoil(SceneBuilder& b)
{
    const GN_SessionId s = b.session();
    GN_NodeId node = -1;
    if (!b.check(gnCreateInputNode(s, -1, &node, "knot"), -1, "gnCreateInputNode"))
        return Node();

    const int cvs = 24;
    std::vector<float> positions;
    for (int k = 0; k < cvs; ++k) {
        const double t = 2.0 * kPi * k / cvs;
        positions.push_back(float(0.9 * (std::sin(t) + 2.0 * std::sin(2.0 * t))));
        positions.push_back(float(0.9 * -std::sin(3.0 * t)) + 2.0f);
        positions.push_back(float(0.9 * (std::cos(t) - 2.0 * std::cos(2.0 * t))));
    }

    GN_PartInfo part{};
    part.type = GN_PARTTYPE_CURVE;
    part.pointCount = cvs;
    part.vertexCount = cvs; // without gnSetVertexList vertex i is point i
    part.faceCount = 1;     // one curve
    b.check(gnSetPartInfo(s, node, 0, &part), node, "gnSetPartInfo");

    GN_CurveInfo curve{};
    curve.curveType = GN_CURVETYPE_NURBS;
    curve.curveCount = 1;
    curve.vertexCount = cvs;
    curve.order = 4;
    curve.isClosed = true;
    b.check(gnSetCurveInfo(s, node, 0, &curve), node, "gnSetCurveInfo");
    b.check(gnSetCurveCounts(s, node, 0, &cvs, 0, 1), node, "gnSetCurveCounts");

    const GN_AttributeInfo p = attribute(GN_ATTROWNER_POINT, GN_STORAGETYPE_FLOAT, cvs, 3, HAPI_ATTRIBUTE_TYPE_POINT);
    b.check(gnAddAttribute(s, node, 0, "P", &p), node, "gnAddAttribute(P)");
    b.check(gnSetAttributeFloatData(s, node, 0, "P", &p, positions.data(), 0, cvs), node,
        "gnSetAttributeFloatData(P)");

    b.check(gnCommitGeo(s, node), node, "gnCommitGeo");
    return Node(&b, node);
}

} // namespace

// Input geometry: a star column and a trefoil knot made in this function and given to the library as input nodes.
// Noise moves the column's surface, spheres sit on the points of its "crown" group and the whole thing spins; the knot
// is resampled into points that carry pulsing beads
void sceneInputGeometry(SceneBuilder& b)
{
    // The column: input node → Mountain → spheres on the "crown" group → merge → spin
    Node star = starColumn(b);
    Node starGeo = parentOf(b, star.id());
    Node wobble =
        b.sop(starGeo, "mountain", "wobble").input(0, star).set("height", 0.15).set("elementsize", 0.8).expr("time",
            "$T * 0.6");
    Node bead = b.sop(starGeo, "sphere", "bead").set("type", "poly").set("freq", 2).set("rad", 0.18, 0.18, 0.18);
    Node crown = b.sop(starGeo, "copytopoints", "crown").input(0, bead).input(1, wobble).set("targetgroup", "crown");
    Node crownColor = color(b, starGeo, crown, 1, 0.9, 0.35);
    Node column = merge(b, starGeo, {wobble, crownColor}, "column");
    b.sop(starGeo, "xform", "spin").input(0, column).set("t", -3.5, 0, 0).expr("r", {nullptr, "$T * 25", nullptr})
        .display();

    // The knot: input node (a closed NURBS curve) → points every changing length → beads; the curve itself is shown too
    Node knot = trefoil(b);
    Node knotGeo = parentOf(b, knot.id());
    Node points =
        b.sop(knotGeo, "resample", "points").input(0, knot).expr("length", "0.35 + 0.15 * sin($T * 1.3)");
    Node dot = b.sop(knotGeo, "sphere", "dot")
                   .set("type", "poly")
                   .set("freq", 2)
                   .expr("rad", {"0.12 + 0.05 * sin($T * 4)", "0.12 + 0.05 * sin($T * 4)", "0.12 + 0.05 * sin($T * 4)"});
    Node beads = b.sop(knotGeo, "copytopoints", "beads").input(0, dot).input(1, points);
    Node beadsColor = color(b, knotGeo, beads, 0.4, 0.85, 1);
    Node curveColor = color(b, knotGeo, knot, 0.9, 0.9, 0.9);
    Node all = merge(b, knotGeo, {curveColor, beadsColor});
    b.sop(knotGeo, "xform", "place").input(0, all).set("t", 3.5, 0, 0).expr("r", {"$T * 15", nullptr, nullptr})
        .display();
}


namespace {

// A closed mesh with a point color, given to an input node: positions and colors — 3 floats per point, faces — the
// points of their vertices one after another (faceCounts vertices per face). The faces go clockwise seen from outside,
// as in Houdini: Boolean takes that side as the outside of a solid
bool commitMesh(SceneBuilder& b, GN_NodeId node, const std::vector<float>& positions, const std::vector<float>& colors,
    const std::vector<int>& vertices, const std::vector<int>& faceCounts)
{
    const GN_SessionId s = b.session();
    const int pointCount = static_cast<int>(positions.size() / 3);
    const int vertexCount = static_cast<int>(vertices.size());
    const int faceCount = static_cast<int>(faceCounts.size());

    GN_PartInfo part{};
    part.type = GN_PARTTYPE_MESH;
    part.pointCount = pointCount;
    part.vertexCount = vertexCount;
    part.faceCount = faceCount;
    bool ok = b.check(gnSetPartInfo(s, node, 0, &part), node, "gnSetPartInfo");

    const GN_AttributeInfo p = attribute(GN_ATTROWNER_POINT, GN_STORAGETYPE_FLOAT, pointCount, 3,
        HAPI_ATTRIBUTE_TYPE_POINT);
    ok = ok && b.check(gnAddAttribute(s, node, 0, "P", &p), node, "gnAddAttribute(P)");
    ok = ok
         && b.check(gnSetAttributeFloatData(s, node, 0, "P", &p, positions.data(), 0, pointCount), node,
             "gnSetAttributeFloatData(P)");
    const GN_AttributeInfo cd = attribute(GN_ATTROWNER_POINT, GN_STORAGETYPE_FLOAT, pointCount, 3,
        HAPI_ATTRIBUTE_TYPE_COLOR);
    ok = ok && b.check(gnAddAttribute(s, node, 0, "Cd", &cd), node, "gnAddAttribute(Cd)");
    ok = ok
         && b.check(gnSetAttributeFloatData(s, node, 0, "Cd", &cd, colors.data(), 0, pointCount), node,
             "gnSetAttributeFloatData(Cd)");
    ok = ok && b.check(gnSetVertexList(s, node, 0, vertices.data(), 0, vertexCount), node, "gnSetVertexList");
    ok = ok && b.check(gnSetFaceCounts(s, node, 0, faceCounts.data(), 0, faceCount), node, "gnSetFaceCounts");
    return ok && b.check(gnCommitGeo(s, node), node, "gnCommitGeo");
}

// A star prism (solid A): a star of 5 tips, 1.8 high, standing on y = 0; star polygons at the ends (concave — the
// library triangulates them), quads around. Warm colors, lighter at the tips and at the top
Node starPrism(SceneBuilder& b, const Node& geo)
{
    GN_NodeId node = -1;
    if (!b.check(gnCreateInputNode(b.session(), geo.id(), &node, "star"), geo.id(), "gnCreateInputNode"))
        return Node();
    const int tips = 5, ring = 2 * tips;
    const double height = 1.8;
    std::vector<float> positions, colors;
    for (int level = 0; level < 2; ++level)
        for (int i = 0; i < ring; ++i) {
            const double angle = i * kPi / tips;
            const double radius = i % 2 == 0 ? 1.5 : 0.7;
            positions.insert(positions.end(),
                {float(radius * std::cos(angle)), float(level * height), float(radius * std::sin(angle))});
            const float glow = i % 2 == 0 ? 1.0f : 0.75f;
            colors.insert(colors.end(), {glow, (0.45f + 0.3f * level) * glow, 0.2f * glow});
        }
    std::vector<int> vertices, faceCounts;
    for (int i = ring - 1; i >= 0; --i) // bottom: clockwise seen from below
        vertices.push_back(i);
    faceCounts.push_back(ring);
    for (int i = 0; i < ring; ++i) // top: clockwise seen from above
        vertices.push_back(ring + i);
    faceCounts.push_back(ring);
    for (int i = 0; i < ring; ++i) { // sides
        const int a = i, c = (i + 1) % ring;
        vertices.insert(vertices.end(), {a, c, ring + c, ring + a});
        faceCounts.push_back(4);
    }
    commitMesh(b, node, positions, colors, vertices, faceCounts);
    return Node(&b, node);
}

// A torus (solid B) around the Y axis: ring radius 1.1, tube radius 0.4, 32 x 16 quads; cool colors around the tube
Node torusMesh(SceneBuilder& b, const Node& geo)
{
    GN_NodeId node = -1;
    if (!b.check(gnCreateInputNode(b.session(), geo.id(), &node, "ring"), geo.id(), "gnCreateInputNode"))
        return Node();
    const int around = 32, tube = 16;
    const double ringRadius = 1.1, tubeRadius = 0.4;
    std::vector<float> positions, colors;
    for (int i = 0; i < around; ++i) {
        const double theta = 2.0 * kPi * i / around;
        for (int j = 0; j < tube; ++j) {
            const double phi = 2.0 * kPi * j / tube;
            const double r = ringRadius + tubeRadius * std::cos(phi);
            positions.insert(positions.end(),
                {float(r * std::cos(theta)), float(tubeRadius * std::sin(phi)), float(r * std::sin(theta))});
            const float shade = float(0.75 + 0.25 * std::cos(phi));
            colors.insert(colors.end(), {0.25f * shade, 0.6f * shade, 1.0f * shade});
        }
    }
    std::vector<int> vertices, faceCounts;
    for (int i = 0; i < around; ++i)
        for (int j = 0; j < tube; ++j) {
            const int i1 = (i + 1) % around, j1 = (j + 1) % tube;
            vertices.insert(vertices.end(), {i * tube + j, i1 * tube + j, i1 * tube + j1, i * tube + j1});
            faceCounts.push_back(4);
        }
    commitMesh(b, node, positions, colors, vertices, faceCounts);
    return Node(&b, node);
}

} // namespace

// Input + Boolean: two solids made in code and given to the library as input nodes (gnCreateInputNode inside one geo
// object) — a star prism A and a torus B. A Transform SOP moves and turns B through A, and Boolean SOPs combine them:
// union, intersection and A minus B, shown next to the inputs themselves. The input geometry is committed once; the
// operations are recomputed every frame from it, and the colors of both inputs carry over into the results
void sceneInputBoolean(SceneBuilder& b)
{
    Node geo = b.object("input_boolean");
    Node star = starPrism(b, geo);
    Node ring = torusMesh(b, geo);
    Node moving = b.sop(geo, "xform", "move_b")
                      .input(0, ring)
                      .expr("t", {"0.9 * sin($T * 0.7)", "0.9 + 0.55 * sin($T * 1.1)", "0.3 * cos($T * 0.9)"})
                      .expr("r", {"90 + $T * 35", "$T * 20", nullptr});

    struct Op
    {
        const char* name;
        const char* op;
        double x;
    };
    const Op ops[] = {{"union", "union", -2.25}, {"intersect", "intersect", 2.25}, {"a_minus_b", "subtract", 6.75}};
    std::vector<Node> placed;
    Node both = merge(b, geo, {star, moving}, "a_and_b");
    placed.push_back(b.sop(geo, "xform", "show_inputs").input(0, both).set("t", -6.75, 0, 0));
    for (const Op& o : ops) {
        Node result = b.sop(geo, "boolean", o.name).input(0, star).input(1, moving).set("booleanop", o.op);
        placed.push_back(b.sop(geo, "xform", (std::string("show_") + o.name).c_str())
                             .input(0, result)
                             .set("t", o.x, 0, 0)
                             .expr("r", {nullptr, "$T * 12", nullptr}));
    }
    merge(b, geo, {placed[0], placed[1], placed[2], placed[3]}, "all").display();
}

} // namespace demo::scenes
