#pragma once

// GeoNodus scene of the demo apps: a session, a graph from JSON (gnLoadGraph) or built in code (the demo scenes of
// ../../scenes), time, asynchronous cooks of the visible objects and reading their geometry into one vertex buffer for
// drawing. Geometry as in the Qt client (GLObject):
// polygons with P, N and Cd of any class, polylines and NURBS / Bezier curves as lines, parts without primitives as
// points. As in HAPI, geometry is read in the space of its object, and gnGetObjectTransform gives the object's world
// matrix, applied when drawing (DrawBatch::model)
#include "demo_math.h"

#include <geonodus.h>

#include <cstdint>
#include <string>
#include <vector>

namespace demo {

namespace scenes {
struct DemoScene;
}

// A vertex for the GPU: position and normal in object space, RGBA color
struct Vertex
{
    float p[3];
    float n[3];
    float c[4];
};

// Geometry of one object: its matrix and ranges in the shared arrays
struct DrawBatch
{
    Mat4 model; // the object's world matrix (gnGetObjectTransform + gnConvertTransformQuatToMatrix)
    uint32_t firstVertex = 0, vertexCount = 0;
    uint32_t firstTriangle = 0, triangleCount = 0; // in indices of the triangles array
    uint32_t firstLine = 0, lineCount = 0;
    uint32_t firstPoint = 0, pointCount = 0;
};

struct SceneGeometry
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> triangles; // 3 indices each
    std::vector<uint32_t> lines; // 2 indices each
    std::vector<uint32_t> points;
    std::vector<DrawBatch> batches;

    // Bounds — in world space
    bool hasBounds = false;
    float boundsMin[3] = {0, 0, 0};
    float boundsMax[3] = {0, 0, 0};

    void clear();
};

class GnScene
{
public:
    GnScene();
    ~GnScene();

    bool isValid() const { return m_session_ok; }
    // The library is the demo edition (node limit, no graph saving)
    bool isDemoEdition() const { return m_demo_edition; }

    // Loads a graph (without cooking): replaces the whole session graph. On failure — the error text
    bool load(const std::wstring& path, std::string& error);
    // Builds a demo scene in code (../../scenes): replaces the whole session graph through the API, without cooking.
    // false — the build reported problems (in error), the scene shows what was built
    bool build(const scenes::DemoScene& scene, std::string& error);
    bool isLoaded() const { return m_loaded; }

    // The graph's timeline, in frames (as the Qt client's slider: frame = time · fps)
    int firstFrame() const { return m_first_frame; }
    int lastFrame() const { return m_last_frame; }
    double fps() const { return m_fps; }
    // The frame of the time saved in the graph
    int startFrame() const { return m_start_frame; }

    // Queues the cook of a frame (gnSetTime + gnCookNode of the visible SOPs). Does not wait: cooks run on the
    // session's job thread
    void startCook(int frame);
    bool isCooking() const { return m_cooking; }
    // Whether the cook queued by startCook has finished. If so, the geometry has been read into out
    bool pollCook(SceneGeometry& out);
    int cookedFrame() const { return m_cooked_frame; }
    double lastCookMs() const { return m_last_cook_ms; }
    double lastFetchMs() const { return m_last_fetch_ms; }

private:
    struct Attr
    {
        std::vector<float> data;
        int tupleSize = 0;
        GN_AttributeOwner owner = GN_ATTROWNER_INVALID;

        bool exists() const { return !data.empty(); }
        // Element index for the attribute's class, -1 — no value
        int index(int vertexIdx, int pointIdx, int primIdx) const;
    };

    // Waits for the queued cooks before the graph is replaced
    void finishCooks();
    // Frames and fps from the session timeline
    void readTimeline();

    Attr readAttr(GN_NodeId node, GN_PartId part, const char* name, GN_AttributeOwner owner) const;
    // As in Houdini: vertex, then point, primitive, detail
    Attr findAttr(GN_NodeId node, GN_PartId part, const char* name) const;
    static void fillColor(float* c, const Attr& Cd, int vertexIdx, int pointIdx, int primIdx);

    void fetch(SceneGeometry& out);
    void fetchPart(GN_NodeId node, const GN_PartInfo& part, SceneGeometry& out);
    void fetchMesh(GN_NodeId node, const GN_PartInfo& part, const std::vector<float>& P, const Attr& N, const Attr& Cd,
        SceneGeometry& out);
    void fetchCurves(GN_NodeId node, const GN_PartInfo& part, const std::vector<float>& P, const Attr& Cd,
        SceneGeometry& out);
    void fetchSplines(GN_NodeId node, const GN_PartInfo& part, const GN_CurveInfo& info,
        const std::vector<int>& curveCounts, const std::vector<int>& vertexList, const std::vector<float>& P,
        const Attr& Cd, SceneGeometry& out);

    struct DisplayNode
    {
        GN_NodeId object;
        GN_NodeId sop;
    };
    std::vector<DisplayNode> displayNodes() const;
    Mat4 objectMatrix(GN_NodeId object) const;

    GN_SessionId m_session = 0;
    bool m_session_ok = false;
    bool m_demo_edition = false;
    bool m_loaded = false;
    int m_first_frame = 1;
    int m_last_frame = 240;
    int m_start_frame = 1;
    double m_fps = 24.0;

    std::vector<DisplayNode> m_cook_nodes;
    bool m_cooking = false;
    int m_cooking_frame = 0;
    int m_cooked_frame = -1;
    int64_t m_cook_start = 0;
    double m_last_cook_ms = 0;
    double m_last_fetch_ms = 0;
};

} // namespace demo
