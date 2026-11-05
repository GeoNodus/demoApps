#include "gn_scene.h"

#include "demo_scenes.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace demo {

namespace {

// Color of surfaces without Cd and of lines and points without Cd (Houdini wireframe)
const float SURFACE_COLOR[3] = {0.8f, 0.8f, 0.8f};
const float WIRE_COLOR[3] = {0.773f, 0.773f, 0.702f};

// Segments per span of a NURBS / Bezier curve (as the dense polyline in the Houdini viewport)
const int SPLINE_DIVISIONS = 32;

int64_t nowTicks()
{
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}

double ticksToMs(int64_t ticks)
{
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    return 1000.0 * double(ticks) / double(f.QuadPart);
}

std::string statusString(GN_SessionId session, GN_StatusType type)
{
    int len = 0;
    if (gnGetStatusStringBufLength(session, type, GN_STATUSVERBOSITY_ERRORS, &len) != GN_RESULT_SUCCESS || len <= 0)
        return {};
    std::string s(static_cast<size_t>(len), '\0');
    gnGetStatusString(session, type, s.data(), len);
    s.resize(std::strlen(s.c_str()));
    return s;
}

// Splits a polygon of n vertices (base + 0 … base + n − 1, p(i) — the position of the i-th) into triangles keeping the
// winding (as in the Qt client). Convex — as a fan; non-convex — by ear clipping in the projection onto the face plane
template<class GetP>
void triangulatePolygon(int n, uint32_t base, GetP p, std::vector<uint32_t>& tris)
{
    auto fan = [&]() {
        for (int v = 1; v + 1 < n; ++v) {
            tris.push_back(base);
            tris.push_back(base + v);
            tris.push_back(base + v + 1);
        }
    };
    if (n == 3) {
        fan();
        return;
    }

    // Newell normal → projection onto the plane of the other two axes
    double nrm[3] = {0, 0, 0};
    for (int v = 0; v < n; ++v) {
        const float* a = p(v);
        const float* b = p((v + 1) % n);
        nrm[0] += (double(a[1]) - b[1]) * (double(a[2]) + b[2]);
        nrm[1] += (double(a[2]) - b[2]) * (double(a[0]) + b[0]);
        nrm[2] += (double(a[0]) - b[0]) * (double(a[1]) + b[1]);
    }
    const double ax = std::abs(nrm[0]), ay = std::abs(nrm[1]), az = std::abs(nrm[2]);
    const int drop = ax >= ay && ax >= az ? 0 : (ay >= az ? 1 : 2);
    const int ix = (drop + 1) % 3;
    const int iy = (drop + 2) % 3;
    const double sign = nrm[drop] >= 0 ? 1.0 : -1.0;
    if (nrm[drop] == 0) {
        fan(); // degenerate face
        return;
    }

    // Work buffers shared by all polygons (reading runs on one thread): allocating for every quad is noticeable on
    // scenes with hundreds of thousands of faces
    static std::vector<double> xy;
    static std::vector<int> idx;
    xy.resize(static_cast<size_t>(n) * 2);
    for (int v = 0; v < n; ++v) {
        xy[v * 2] = p(v)[ix];
        xy[v * 2 + 1] = p(v)[iy];
    }
    auto cross2 = [&](int a, int b, int c) {
        const double* A = &xy[a * 2];
        const double* B = &xy[b * 2];
        const double* C = &xy[c * 2];
        return sign * ((B[0] - A[0]) * (C[1] - A[1]) - (B[1] - A[1]) * (C[0] - A[0]));
    };

    bool convex = true;
    for (int v = 0; v < n && convex; ++v)
        convex = cross2(v, (v + 1) % n, (v + 2) % n) >= 0;
    if (convex) {
        fan();
        return;
    }

    idx.resize(n);
    for (int v = 0; v < n; ++v)
        idx[v] = v;
    auto inside = [&](int q, int a, int b, int c) {
        return cross2(a, b, q) >= 0 && cross2(b, c, q) >= 0 && cross2(c, a, q) >= 0;
    };
    // A vertex at the same place as an ear corner is not an obstacle: in a face with a hole (Boolean joins the hole to
    // the outer contour with a bridge) the bridge vertices occur twice in the loop
    auto samePlace = [&](int q, int a) { return xy[q * 2] == xy[a * 2] && xy[q * 2 + 1] == xy[a * 2 + 1]; };
    int guard = 0; // no ear (self-intersection) — the current corner is cut off to avoid looping forever
    for (int i = 0; idx.size() > 3;) {
        const int m = static_cast<int>(idx.size());
        const int a = idx[(i + m - 1) % m], b = idx[i % m], c = idx[(i + 1) % m];
        bool ear = cross2(a, b, c) > 0;
        for (int k = 0; ear && k < m; ++k) {
            const int q = idx[k];
            if (q != a && q != b && q != c && !samePlace(q, a) && !samePlace(q, b) && !samePlace(q, c)
                && inside(q, a, b, c))
                ear = false;
        }
        if (ear || guard >= m) {
            tris.push_back(base + a);
            tris.push_back(base + b);
            tris.push_back(base + c);
            const int j = i % m;
            idx.erase(idx.begin() + j);
            i = j > 0 ? j - 1 : 0;
            guard = 0;
        } else {
            i = (i + 1) % m;
            ++guard;
        }
    }
    tris.push_back(base + idx[0]);
    tris.push_back(base + idx[1]);
    tris.push_back(base + idx[2]);
}

// B-spline (P&T "The NURBS Book"): the span containing u — knots[span] ≤ u < knots[span + 1], at the end of the domain
// the last non-empty one
int findSpan(const float* knots, int order, int count, double u)
{
    int span = order - 1;
    while (span < count - 1 && knots[span + 1] <= u)
        ++span;
    while (span > order - 1 && knots[span + 1] <= knots[span])
        --span;
    return span;
}

// order values of the basis functions N_{span − order + 1 .. span}(u) (P&T A2.2)
void basisFuns(const float* knots, int span, int order, double u, double* N)
{
    double left[16], right[16];
    N[0] = 1.0;
    for (int j = 1; j < order; ++j) {
        left[j] = u - knots[span + 1 - j];
        right[j] = knots[span + j] - u;
        double saved = 0.0;
        for (int r = 0; r < j; ++r) {
            const double denom = right[r + 1] + left[j - r];
            const double temp = denom != 0.0 ? N[r] / denom : 0.0;
            N[r] = saved + right[r + 1] * temp;
            saved = left[j - r] * temp;
        }
        N[j] = saved;
    }
}

Vertex makeVertex(const float* p, const float* c)
{
    Vertex v{};
    std::copy(p, p + 3, v.p);
    std::copy(c, c + 4, v.c);
    return v;
}

} // namespace

void SceneGeometry::clear()
{
    vertices.clear();
    triangles.clear();
    lines.clear();
    points.clear();
    batches.clear();
    hasBounds = false;
}

GnScene::GnScene()
{
    m_session_ok = gnCreateSession(&m_session) == GN_RESULT_SUCCESS;
    // The demo edition of the library does not save graphs — that is how we detect it (it also prints a line about it)
    int length = 0;
    m_demo_edition = m_session_ok && gnGetGraphStringBufLength(m_session, &length) == GN_RESULT_DEMO_LIMIT;
}

GnScene::~GnScene()
{
    if (m_session_ok)
        gnCloseSession(m_session);
}

void GnScene::finishCooks()
{
    // The previous cook must finish before the graph is replaced (loading waits for it anyway — the session's cook
    // lock)
    for (const DisplayNode& n : m_cook_nodes)
        gnWaitCook(m_session, n.sop, -1);
    m_cook_nodes.clear();
    m_cooking = false;
    m_cooked_frame = -1;
}

void GnScene::readTimeline()
{
    GN_TimelineOptions tl{};
    gnGetTimelineOptions(m_session, &tl);
    m_fps = tl.fps > 0 ? tl.fps : 24.0;
    m_first_frame = static_cast<int>(std::lround(tl.startTime * m_fps));
    m_last_frame = (std::max) (m_first_frame + 1, static_cast<int>(std::lround(tl.endTime * m_fps)));
    double time = 0.0;
    gnGetTime(m_session, &time);
    m_start_frame = (std::clamp) (static_cast<int>(std::lround(time * m_fps)), m_first_frame, m_last_frame);
}

bool GnScene::build(const scenes::DemoScene& scene, std::string& error)
{
    if (!m_session_ok) {
        error = "gnCreateSession failed";
        return false;
    }
    finishCooks();
    // The scene function replaces the graph with its own nodes through the API (gnCreateNode, gnSetParm*, ...)
    scenes::SceneBuilder builder(m_session);
    scenes::buildDemoScene(scene, builder);
    readTimeline();
    m_loaded = true;
    // Problems of the build do not stop it: the scene shows what was built, the text lists what failed
    error.clear();
    for (const std::string& e : builder.errors())
        error += e + "\n";
    return builder.errors().empty();
}

bool GnScene::load(const std::wstring& path, std::string& error)
{
    if (!m_session_ok) {
        error = "gnCreateSession failed";
        return false;
    }
    finishCooks();

    // Relative file paths of the graph (File SOP: "assets/crystal.obj" of the demo scenes) are relative to the folder
    // of the graph: the library opens them relative to the current directory
    const size_t slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos)
        SetCurrentDirectoryW(path.substr(0, slash).c_str());

    // The graph is opened by path (UTF-8): the demo edition of the library recognizes the demo scenes by file name
    std::string utf8(static_cast<size_t>(
                         WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, nullptr, 0, nullptr, nullptr)),
        '\0');
    if (!utf8.empty())
        WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, utf8.data(), static_cast<int>(utf8.size()), nullptr, nullptr);
    const GN_Result result = gnLoadGraph(m_session, utf8.c_str(), false);
    if (result != GN_RESULT_SUCCESS) {
        // The file did not open — the library gives no text; otherwise the error text is in GN_STATUS_CALL_RESULT
        error = GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES
                    ? "Cannot open the file"
                    : statusString(m_session, GN_STATUS_CALL_RESULT);
        if (error.empty())
            error = "gnLoadGraph failed";
        m_loaded = false;
        return false;
    }
    readTimeline();
    m_loaded = true;
    return true;
}

std::vector<GnScene::DisplayNode> GnScene::displayNodes() const
{
    // Visible geo objects on /obj (the object's Display flag) → their Display SOPs (HAPI_GetDisplayGeoInfo)
    std::vector<DisplayNode> result;
    int count = 0;
    if (gnComposeChildNodeList(m_session, 0, GN_NODETYPE_OBJ, GN_NODEFLAGS_DISPLAY | GN_NODEFLAGS_OBJ_GEOMETRY, false,
            &count)
            != GN_RESULT_SUCCESS
        || count <= 0)
        return result;
    std::vector<GN_NodeId> objects(count);
    gnGetComposedChildNodeList(m_session, 0, objects.data(), count);
    for (GN_NodeId obj : objects) {
        GN_GeoInfo geo{};
        if (gnGetDisplayGeoInfo(m_session, obj, &geo) == GN_RESULT_SUCCESS)
            result.push_back({obj, geo.nodeId});
    }
    return result;
}

Mat4 GnScene::objectMatrix(GN_NodeId object) const
{
    // relative_to = -1 — the world transform (with the parents on input 0), as HAPI_GetObjectTransform. The row-major
    // matrix for row vectors has the same layout as Mat4, column-major for column vectors
    Mat4 m;
    GN_Transform xf{};
    if (gnGetObjectTransform(m_session, object, -1, GN_SRT, &xf) == GN_RESULT_SUCCESS)
        gnConvertTransformQuatToMatrix(m_session, &xf, m.m);
    return m;
}

void GnScene::startCook(int frame)
{
    if (!m_loaded || m_cooking)
        return;
    m_cook_start = nowTicks();
    // Frame time as in the Qt client: frame / fps. Only time-dependent parameters change
    gnSetTime(m_session, frame / m_fps);

    GN_CookOptions options = gnCookOptions_Create();
    // Whole faces: triangulatePolygon splits them into triangles (by default the engine fans them from the first
    // vertex, and a triangle of a non-convex face crosses the concave corner). Curves come as splines, fetch evaluates
    // them
    options.maxVerticesPerPrimitive = -1;
    options.refineCurveToLinear = false;

    m_cook_nodes = displayNodes();
    for (const DisplayNode& n : m_cook_nodes)
        gnCookNode(m_session, n.sop, &options);
    m_cooking_frame = frame;
    m_cooking = true;
}

bool GnScene::pollCook(SceneGeometry& out)
{
    if (!m_cooking)
        return false;
    for (const DisplayNode& n : m_cook_nodes) {
        GN_CookState state = GN_COOKSTATE_INVALID;
        gnGetCookState(m_session, n.sop, &state);
        if (state == GN_COOKSTATE_QUEUED || state == GN_COOKSTATE_COOKING)
            return false;
    }
    const int64_t cooked = nowTicks();
    m_last_cook_ms = ticksToMs(cooked - m_cook_start);
    fetch(out);
    m_last_fetch_ms = ticksToMs(nowTicks() - cooked);
    m_cooking = false;
    m_cooked_frame = m_cooking_frame;
    return true;
}

int GnScene::Attr::index(int vertexIdx, int pointIdx, int primIdx) const
{
    int idx = -1;
    switch (owner) {
    case GN_ATTROWNER_VERTEX:
        idx = vertexIdx;
        break;
    case GN_ATTROWNER_POINT:
        idx = pointIdx;
        break;
    case GN_ATTROWNER_PRIM:
        idx = primIdx;
        break;
    case GN_ATTROWNER_DETAIL:
        idx = 0;
        break;
    default:
        return -1;
    }
    if (idx < 0 || tupleSize < 1 || static_cast<size_t>(idx + 1) * tupleSize > data.size())
        return -1;
    return idx;
}

GnScene::Attr GnScene::readAttr(GN_NodeId node, GN_PartId part, const char* name, GN_AttributeOwner owner) const
{
    Attr attr;
    GN_AttributeInfo info{};
    if (gnGetAttributeInfo(m_session, node, part, name, owner, &info) != GN_RESULT_SUCCESS || !info.exists)
        return attr;
    if (info.storage == GN_STORAGETYPE_STRING || info.tupleSize < 1 || info.count < 1)
        return attr;
    attr.data.resize(static_cast<size_t>(info.count) * info.tupleSize);
    if (gnGetAttributeFloatData(m_session, node, part, name, &info, -1, attr.data.data(), 0, info.count)
        != GN_RESULT_SUCCESS) {
        attr.data.clear();
        return attr;
    }
    attr.tupleSize = info.tupleSize;
    attr.owner = owner;
    return attr;
}

GnScene::Attr GnScene::findAttr(GN_NodeId node, GN_PartId part, const char* name) const
{
    for (GN_AttributeOwner owner : {GN_ATTROWNER_VERTEX, GN_ATTROWNER_POINT, GN_ATTROWNER_PRIM, GN_ATTROWNER_DETAIL}) {
        Attr attr = readAttr(node, part, name, owner);
        if (attr.exists())
            return attr;
    }
    return {};
}

void GnScene::fillColor(float* c, const Attr& Cd, int vertexIdx, int pointIdx, int primIdx)
{
    const int ci = Cd.index(vertexIdx, pointIdx, primIdx);
    if (ci < 0)
        return;
    const float* src = &Cd.data[static_cast<size_t>(ci) * Cd.tupleSize];
    if (Cd.tupleSize >= 3) {
        c[0] = src[0];
        c[1] = src[1];
        c[2] = src[2];
    } else {
        c[0] = c[1] = c[2] = src[0];
    }
}

void GnScene::fetch(SceneGeometry& out)
{
    out.clear();
    for (const DisplayNode& n : m_cook_nodes) {
        GN_GeoInfo geo{};
        if (gnGetGeoInfo(m_session, n.sop, &geo) != GN_RESULT_SUCCESS)
            continue;
        DrawBatch batch;
        batch.model = objectMatrix(n.object);
        batch.firstVertex = static_cast<uint32_t>(out.vertices.size());
        batch.firstTriangle = static_cast<uint32_t>(out.triangles.size());
        batch.firstLine = static_cast<uint32_t>(out.lines.size());
        batch.firstPoint = static_cast<uint32_t>(out.points.size());
        for (int i = 0; i < geo.partCount; ++i) {
            GN_PartInfo part{};
            if (gnGetPartInfo(m_session, n.sop, i, &part) == GN_RESULT_SUCCESS)
                fetchPart(n.sop, part, out);
        }
        batch.vertexCount = static_cast<uint32_t>(out.vertices.size()) - batch.firstVertex;
        batch.triangleCount = static_cast<uint32_t>(out.triangles.size()) - batch.firstTriangle;
        batch.lineCount = static_cast<uint32_t>(out.lines.size()) - batch.firstLine;
        batch.pointCount = static_cast<uint32_t>(out.points.size()) - batch.firstPoint;
        if (batch.vertexCount)
            out.batches.push_back(batch);
    }

    // Bounds — over the drawn vertices, in world space
    for (const DrawBatch& b : out.batches) {
        const Mat4& m = b.model;
        for (uint32_t i = b.firstVertex; i < b.firstVertex + b.vertexCount; ++i) {
            const float* p = out.vertices[i].p;
            float w[3];
            for (int k = 0; k < 3; ++k)
                w[k] = m.at(k, 0) * p[0] + m.at(k, 1) * p[1] + m.at(k, 2) * p[2] + m.at(k, 3);
            if (!out.hasBounds) {
                std::copy(w, w + 3, out.boundsMin);
                std::copy(w, w + 3, out.boundsMax);
                out.hasBounds = true;
            }
            for (int k = 0; k < 3; ++k) {
                out.boundsMin[k] = (std::min) (out.boundsMin[k], w[k]);
                out.boundsMax[k] = (std::max) (out.boundsMax[k], w[k]);
            }
        }
    }
}

void GnScene::fetchPart(GN_NodeId node, const GN_PartInfo& part, SceneGeometry& out)
{
    const Attr Pattr = readAttr(node, part.id, "P", GN_ATTROWNER_POINT);
    if (!Pattr.exists() || Pattr.tupleSize < 3)
        return;
    const int pointCount = static_cast<int>(Pattr.data.size() / Pattr.tupleSize);
    std::vector<float> P(static_cast<size_t>(pointCount) * 3);
    for (int i = 0; i < pointCount; ++i)
        for (int k = 0; k < 3; ++k)
            P[i * 3 + k] = Pattr.data[static_cast<size_t>(i) * Pattr.tupleSize + k];

    Attr N = findAttr(node, part.id, "N");
    if (N.tupleSize < 3 || N.owner == GN_ATTROWNER_DETAIL)
        N = {};
    const Attr Cd = findAttr(node, part.id, "Cd");

    const size_t verticesBefore = out.vertices.size();
    if (part.type == GN_PARTTYPE_CURVE)
        fetchCurves(node, part, P, Cd, out);
    else
        fetchMesh(node, part, P, N, Cd, out);

    // A part without primitives (Scatter, Add without polygons) — as points
    if (out.vertices.size() == verticesBefore) {
        for (int i = 0; i < pointCount; ++i) {
            float c[4] = {WIRE_COLOR[0], WIRE_COLOR[1], WIRE_COLOR[2], 1.0f};
            if (Cd.owner == GN_ATTROWNER_POINT || Cd.owner == GN_ATTROWNER_DETAIL)
                fillColor(c, Cd, -1, i, -1);
            out.points.push_back(static_cast<uint32_t>(out.vertices.size()));
            out.vertices.push_back(makeVertex(&P[i * 3], c));
        }
    }
}

void GnScene::fetchMesh(GN_NodeId node, const GN_PartInfo& part, const std::vector<float>& P, const Attr& N,
    const Attr& Cd, SceneGeometry& out)
{
    const int pointCount = static_cast<int>(P.size() / 3);
    if (part.faceCount < 1 || part.vertexCount < 1 || pointCount < 1)
        return;

    std::vector<int> faceCounts(part.faceCount);
    std::vector<int> vertexList(part.vertexCount);
    if (gnGetFaceCounts(m_session, node, part.id, faceCounts.data(), 0, part.faceCount) != GN_RESULT_SUCCESS
        || gnGetVertexList(m_session, node, part.id, vertexList.data(), 0, part.vertexCount) != GN_RESULT_SUCCESS)
        return;

    out.vertices.reserve(out.vertices.size() + part.vertexCount);
    int vOffset = 0;
    for (int prim = 0; prim < part.faceCount; ++prim) {
        const int numV = faceCounts[prim];
        if (numV < 1)
            continue;
        if (vOffset + numV > part.vertexCount)
            break;

        const uint32_t base = static_cast<uint32_t>(out.vertices.size());
        for (int v = 0; v < numV; ++v) {
            const int vIdx = vOffset + v;
            int pIdx = vertexList[vIdx];
            if (pIdx < 0 || pIdx >= pointCount)
                pIdx = 0;

            Vertex vx{};
            std::copy(&P[pIdx * 3], &P[pIdx * 3] + 3, vx.p);
            const int ni = N.index(vIdx, pIdx, prim);
            if (ni >= 0)
                std::copy(&N.data[static_cast<size_t>(ni) * N.tupleSize],
                    &N.data[static_cast<size_t>(ni) * N.tupleSize] + 3, vx.n);
            const bool line = numV < 3;
            vx.c[0] = line ? WIRE_COLOR[0] : SURFACE_COLOR[0];
            vx.c[1] = line ? WIRE_COLOR[1] : SURFACE_COLOR[1];
            vx.c[2] = line ? WIRE_COLOR[2] : SURFACE_COLOR[2];
            vx.c[3] = 1.0f;
            fillColor(vx.c, Cd, vIdx, pIdx, prim);
            out.vertices.push_back(vx);
        }

        // Without N — the flat face normal (Newell's method). Houdini polygon vertices go clockwise when seen from the
        // front, so the Newell normal is flipped
        if (!N.exists() && numV >= 3) {
            float n[3] = {0, 0, 0};
            for (int v = 0; v < numV; ++v) {
                const float* a = out.vertices[base + v].p;
                const float* b = out.vertices[base + (v + 1) % numV].p;
                n[0] += (a[1] - b[1]) * (a[2] + b[2]);
                n[1] += (a[2] - b[2]) * (a[0] + b[0]);
                n[2] += (a[0] - b[0]) * (a[1] + b[1]);
            }
            const float len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
            if (len > 0)
                for (int v = 0; v < numV; ++v)
                    for (int k = 0; k < 3; ++k)
                        out.vertices[base + v].n[k] = -n[k] / len;
        }

        if (numV == 1) {
            out.points.push_back(base);
        } else if (numV == 2) {
            out.lines.push_back(base);
            out.lines.push_back(base + 1);
        } else {
            triangulatePolygon(numV, base, [&](int v) { return out.vertices[base + v].p; }, out.triangles);
        }
        vOffset += numV;
    }
}

void GnScene::fetchCurves(GN_NodeId node, const GN_PartInfo& part, const std::vector<float>& P, const Attr& Cd,
    SceneGeometry& out)
{
    const int pointCount = static_cast<int>(P.size() / 3);
    if (pointCount < 1)
        return;
    GN_CurveInfo info{};
    if (gnGetCurveInfo(m_session, node, part.id, &info) != GN_RESULT_SUCCESS)
        return;
    const int curveCount = info.curveCount > 0 ? info.curveCount : part.faceCount;
    if (curveCount < 1)
        return;
    std::vector<int> curveCounts(curveCount);
    if (gnGetCurveCounts(m_session, node, part.id, curveCounts.data(), 0, curveCount) != GN_RESULT_SUCCESS)
        return;

    // Curve vertices refer to points through the vertex list; without it the points go in order
    std::vector<int> vertexList;
    if (part.vertexCount > 0) {
        vertexList.resize(part.vertexCount);
        if (gnGetVertexList(m_session, node, part.id, vertexList.data(), 0, part.vertexCount) != GN_RESULT_SUCCESS)
            vertexList.clear();
    }

    if (info.curveType == GN_CURVETYPE_NURBS || info.curveType == GN_CURVETYPE_BEZIER) {
        fetchSplines(node, part, info, curveCounts, vertexList, P, Cd, out);
        return;
    }

    // Polylines — segments between the vertices
    const bool closed = info.isPeriodic || info.isClosed;
    int vOffset = 0;
    for (int prim = 0; prim < curveCount; ++prim) {
        const int numV = curveCounts[prim];
        if (numV < 1)
            continue;
        const uint32_t base = static_cast<uint32_t>(out.vertices.size());
        for (int v = 0; v < numV; ++v) {
            const int vIdx = vOffset + v;
            int pIdx = vIdx < static_cast<int>(vertexList.size()) ? vertexList[vIdx] : vIdx;
            if (pIdx < 0 || pIdx >= pointCount)
                pIdx = 0;
            float c[4] = {WIRE_COLOR[0], WIRE_COLOR[1], WIRE_COLOR[2], 1.0f};
            fillColor(c, Cd, vIdx, pIdx, prim);
            out.vertices.push_back(makeVertex(&P[pIdx * 3], c));
        }
        if (numV == 1) {
            out.points.push_back(base);
        } else {
            for (int v = 0; v + 1 < numV; ++v) {
                out.lines.push_back(base + v);
                out.lines.push_back(base + v + 1);
            }
            if (closed && numV > 2) {
                out.lines.push_back(base + numV - 1);
                out.lines.push_back(base);
            }
        }
        vOffset += numV;
    }
}

void GnScene::fetchSplines(GN_NodeId node, const GN_PartInfo& part, const GN_CurveInfo& info,
    const std::vector<int>& curveCounts, const std::vector<int>& vertexList, const std::vector<float>& P,
    const Attr& Cd, SceneGeometry& out)
{
    const int curveCount = static_cast<int>(curveCounts.size());
    const int pointCount = static_cast<int>(P.size() / 3);

    // Orders: one for all or per curve
    std::vector<int> orders(curveCount, info.order);
    if (info.order <= 0
        && gnGetCurveOrders(m_session, node, part.id, orders.data(), 0, curveCount) != GN_RESULT_SUCCESS)
        return;

    // Knots: count + order per curve — unrolled B-spline knots (for Bezier — breakpoints of multiplicity order − 1)
    std::vector<float> knots;
    if (info.hasKnots && info.knotCount > 0) {
        knots.resize(info.knotCount);
        if (gnGetCurveKnots(m_session, node, part.id, knots.data(), 0, info.knotCount) != GN_RESULT_SUCCESS)
            return;
    } else {
        for (int i = 0; i < curveCount; ++i) {
            const int n = curveCounts[i], k = orders[i];
            const int segments = k > 1 ? (std::max) (1, (n - 1) / (k - 1)) : 1;
            for (int s = 0; s <= segments; ++s)
                for (int r = 0; r < (s == 0 || s == segments ? k : k - 1); ++r)
                    knots.push_back(static_cast<float>(s));
        }
    }

    // CV weights — Pw (rational curves)
    const Attr Pw = readAttr(node, part.id, "Pw", GN_ATTROWNER_POINT);

    std::vector<double> N;
    std::vector<int> cvVertex, cvPoint;
    std::vector<double> params;
    int vOffset = 0;
    size_t kOffset = 0;
    for (int prim = 0; prim < curveCount; ++prim) {
        const int n = curveCounts[prim];
        const int k = orders[prim];
        if (n < 1 || k < 2 || k > 15 || kOffset + n + k > knots.size()) {
            vOffset += n;
            kOffset += static_cast<size_t>((std::max) (n, 0) + (std::max) (k, 0));
            continue;
        }
        const float* t = knots.data() + kOffset;
        cvVertex.resize(n);
        cvPoint.resize(n);
        for (int j = 0; j < n; ++j) {
            cvVertex[j] = vOffset + j;
            const int pIdx = cvVertex[j] < static_cast<int>(vertexList.size()) ? vertexList[cvVertex[j]] : cvVertex[j];
            cvPoint[j] = pIdx >= 0 && pIdx < pointCount ? pIdx : 0;
        }

        // Sample parameters: each non-empty span of the domain [t[k − 1], t[n]] in SPLINE_DIVISIONS parts
        params.clear();
        for (int s = k - 1; s < n; ++s) {
            if (!(t[s + 1] > t[s]))
                continue;
            for (int d = 0; d < SPLINE_DIVISIONS; ++d)
                params.push_back(t[s] + (static_cast<double>(t[s + 1]) - t[s]) * d / SPLINE_DIVISIONS);
        }
        params.push_back(t[n]);

        const uint32_t base = static_cast<uint32_t>(out.vertices.size());
        N.resize(k);
        for (double u : params) {
            const int span = findSpan(t, k, n, u);
            basisFuns(t, span, k, u, N.data());
            const int first = span - k + 1;
            // Rational basis R_j = N_j·w_j / Σ — for the position and the color
            double wsum = 0.0;
            for (int j = 0; j < k; ++j) {
                const int pw = Pw.index(cvVertex[first + j], cvPoint[first + j], prim);
                N[j] *= pw >= 0 ? Pw.data[static_cast<size_t>(pw) * Pw.tupleSize] : 1.0f;
                wsum += N[j];
            }
            if (wsum != 0.0)
                for (int j = 0; j < k; ++j)
                    N[j] /= wsum;

            Vertex vx{};
            for (int j = 0; j < k; ++j) {
                const int cv = first + j;
                float cj[4] = {WIRE_COLOR[0], WIRE_COLOR[1], WIRE_COLOR[2], 1.0f};
                fillColor(cj, Cd, cvVertex[cv], cvPoint[cv], prim);
                for (int a = 0; a < 3; ++a)
                    vx.p[a] += static_cast<float>(N[j] * P[static_cast<size_t>(cvPoint[cv]) * 3 + a]);
                for (int a = 0; a < 4; ++a)
                    vx.c[a] += static_cast<float>(N[j] * cj[a]);
            }
            out.vertices.push_back(vx);
        }
        for (uint32_t i = 1; i < params.size(); ++i) {
            out.lines.push_back(base + i - 1);
            out.lines.push_back(base + i);
        }
        vOffset += n;
        kOffset += static_cast<size_t>(n + k);
    }
}

} // namespace demo
