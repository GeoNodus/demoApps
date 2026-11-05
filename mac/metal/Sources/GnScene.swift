// GeoNodus scene of the demo app: a session, a graph from JSON (gnLoadGraph) or built in code (the demo scenes of
// ../../scenes, through bridge/demo_scenes_c.h), time, asynchronous cooks of the visible objects and reading their
// geometry into one vertex buffer for drawing. The same as GnScene of the Windows demos (win/common/gn_scene.cpp):
// polygons with P, N and Cd of any class, polylines and NURBS / Bezier curves as lines, parts without primitives as
// points. As in HAPI, geometry is read in the space of its object, and gnGetObjectTransform gives the object's world
// matrix, applied when drawing (DrawBatch.model)
import Foundation
import QuartzCore
import simd

// A vertex for the GPU: position and normal in object space, RGBA color (40 bytes, packed_float3 / packed_float4 in
// the shader)
struct Vertex {
    var px: Float = 0, py: Float = 0, pz: Float = 0
    var nx: Float = 0, ny: Float = 0, nz: Float = 0
    var r: Float = 0, g: Float = 0, b: Float = 0, a: Float = 0
}

// Geometry of one object: its matrix and ranges in the shared arrays
struct DrawBatch {
    var model = matrix_identity_float4x4 // the object's world matrix (gnGetObjectTransform)
    var firstVertex = 0, vertexCount = 0
    var firstTriangle = 0, triangleCount = 0 // in indices of the triangles array
    var firstLine = 0, lineCount = 0
    var firstPoint = 0, pointCount = 0
}

struct SceneGeometry {
    var vertices: [Vertex] = []
    var triangles: [UInt32] = [] // 3 indices each
    var lines: [UInt32] = [] // 2 indices each
    var points: [UInt32] = []
    var batches: [DrawBatch] = []

    // Bounds — in world space
    var boundsMin: Vec3? = nil
    var boundsMax = Vec3(0, 0, 0)

    mutating func clear() {
        vertices.removeAll(keepingCapacity: true)
        triangles.removeAll(keepingCapacity: true)
        lines.removeAll(keepingCapacity: true)
        points.removeAll(keepingCapacity: true)
        batches.removeAll()
        boundsMin = nil
    }
}

// Color of surfaces without Cd and of lines and points without Cd (Houdini wireframe)
private let surfaceColor: (Float, Float, Float) = (0.8, 0.8, 0.8)
private let wireColor: (Float, Float, Float) = (0.773, 0.773, 0.702)

// Segments per span of a NURBS / Bezier curve (as the dense polyline in the Houdini viewport)
private let splineDivisions = 32

private func ok(_ r: GN_Result) -> Bool { r == GN_RESULT_SUCCESS }

private func makeVertex(_ P: [Float], _ point: Int, _ c: (Float, Float, Float, Float)) -> Vertex {
    Vertex(px: P[point * 3], py: P[point * 3 + 1], pz: P[point * 3 + 2], r: c.0, g: c.1, b: c.2, a: c.3)
}

// Splits a polygon of n vertices (base + 0 … base + n − 1) into triangles keeping the winding (as in the Qt client).
// Convex — as a fan; non-convex — by ear clipping in the projection onto the face plane. Work buffers are shared by all
// polygons: allocating for every quad is noticeable on scenes with hundreds of thousands of faces
private final class Triangulator {
    private var xy: [Double] = []
    private var idx: [Int] = []

    func triangulate(n: Int, base: Int, vertices: [Vertex], tris: inout [UInt32]) {
        let b = UInt32(base)
        func fan() {
            for v in 1..<(n - 1) {
                tris.append(b)
                tris.append(b + UInt32(v))
                tris.append(b + UInt32(v + 1))
            }
        }
        if n == 3 {
            fan()
            return
        }

        // Newell normal → projection onto the plane of the other two axes
        var nrm = (0.0, 0.0, 0.0)
        for v in 0..<n {
            let a = vertices[base + v], c = vertices[base + (v + 1) % n]
            nrm.0 += (Double(a.py) - Double(c.py)) * (Double(a.pz) + Double(c.pz))
            nrm.1 += (Double(a.pz) - Double(c.pz)) * (Double(a.px) + Double(c.px))
            nrm.2 += (Double(a.px) - Double(c.px)) * (Double(a.py) + Double(c.py))
        }
        let ax = abs(nrm.0), ay = abs(nrm.1), az = abs(nrm.2)
        let drop = ax >= ay && ax >= az ? 0 : (ay >= az ? 1 : 2)
        let dropValue = drop == 0 ? nrm.0 : (drop == 1 ? nrm.1 : nrm.2)
        if dropValue == 0 {
            fan() // degenerate face
            return
        }
        let sign = dropValue >= 0 ? 1.0 : -1.0

        xy.removeAll(keepingCapacity: true)
        for v in 0..<n {
            let p = vertices[base + v]
            let c = (Double(p.px), Double(p.py), Double(p.pz))
            switch drop {
            case 0: xy.append(c.1); xy.append(c.2)
            case 1: xy.append(c.2); xy.append(c.0)
            default: xy.append(c.0); xy.append(c.1)
            }
        }
        func cross2(_ a: Int, _ b: Int, _ c: Int) -> Double {
            sign * ((xy[b * 2] - xy[a * 2]) * (xy[c * 2 + 1] - xy[a * 2 + 1])
                - (xy[b * 2 + 1] - xy[a * 2 + 1]) * (xy[c * 2] - xy[a * 2]))
        }

        var convex = true
        for v in 0..<n where convex {
            convex = cross2(v, (v + 1) % n, (v + 2) % n) >= 0
        }
        if convex {
            fan()
            return
        }

        idx.removeAll(keepingCapacity: true)
        idx.append(contentsOf: 0..<n)
        func inside(_ q: Int, _ a: Int, _ b: Int, _ c: Int) -> Bool {
            cross2(a, b, q) >= 0 && cross2(b, c, q) >= 0 && cross2(c, a, q) >= 0
        }
        // A vertex at the same place as an ear corner is not an obstacle: in a face with a hole (Boolean joins the hole
        // to the outer contour with a bridge) the bridge vertices occur twice in the loop
        func samePlace(_ q: Int, _ a: Int) -> Bool { xy[q * 2] == xy[a * 2] && xy[q * 2 + 1] == xy[a * 2 + 1] }
        var guardCount = 0 // no ear (self-intersection) — the current corner is cut off to avoid looping forever
        var i = 0
        while idx.count > 3 {
            let m = idx.count
            let a = idx[(i + m - 1) % m], bb = idx[i % m], c = idx[(i + 1) % m]
            var ear = cross2(a, bb, c) > 0
            var k = 0
            while ear && k < m {
                let q = idx[k]
                if q != a && q != bb && q != c && !samePlace(q, a) && !samePlace(q, bb) && !samePlace(q, c)
                    && inside(q, a, bb, c)
                {
                    ear = false
                }
                k += 1
            }
            if ear || guardCount >= m {
                tris.append(b + UInt32(a))
                tris.append(b + UInt32(bb))
                tris.append(b + UInt32(c))
                let j = i % m
                idx.remove(at: j)
                i = j > 0 ? j - 1 : 0
                guardCount = 0
            } else {
                i = (i + 1) % m
                guardCount += 1
            }
        }
        tris.append(b + UInt32(idx[0]))
        tris.append(b + UInt32(idx[1]))
        tris.append(b + UInt32(idx[2]))
    }
}

// B-spline (P&T "The NURBS Book"): the span containing u — knots[span] ≤ u < knots[span + 1], at the end of the domain
// the last non-empty one. The knots of a curve start at `t`
private func findSpan(_ knots: [Float], _ t: Int, _ order: Int, _ count: Int, _ u: Double) -> Int {
    var span = order - 1
    while span < count - 1 && Double(knots[t + span + 1]) <= u { span += 1 }
    while span > order - 1 && knots[t + span + 1] <= knots[t + span] { span -= 1 }
    return span
}

// order values of the basis functions N_{span − order + 1 .. span}(u) (P&T A2.2)
private func basisFuns(_ knots: [Float], _ t: Int, _ span: Int, _ order: Int, _ u: Double, _ N: inout [Double]) {
    var left = [Double](repeating: 0, count: order), right = [Double](repeating: 0, count: order)
    N[0] = 1
    for j in 1..<order {
        left[j] = u - Double(knots[t + span + 1 - j])
        right[j] = Double(knots[t + span + j]) - u
        var saved = 0.0
        for r in 0..<j {
            let denom = right[r + 1] + left[j - r]
            let temp = denom != 0 ? N[r] / denom : 0
            N[r] = saved + right[r + 1] * temp
            saved = left[j - r] * temp
        }
        N[j] = saved
    }
}

final class GnScene {
    private(set) var isValid = false
    // The library is the demo edition (node limit, no graph saving)
    private(set) var isDemoEdition = false
    private(set) var isLoaded = false

    // The graph's timeline, in frames (as the Qt client's slider: frame = time · fps)
    private(set) var firstFrame = 1
    private(set) var lastFrame = 240
    private(set) var fps = 24.0
    // The frame of the time saved in the graph
    private(set) var startFrame = 1

    private(set) var isCooking = false
    private(set) var cookedFrame = -1
    private(set) var lastCookMs = 0.0
    private(set) var lastFetchMs = 0.0

    private var session: GN_SessionId = 0
    private struct DisplayNode {
        var object: GN_NodeId
        var sop: GN_NodeId
    }
    private var cookNodes: [DisplayNode] = []
    private var cookingFrame = 0
    private var cookStart = 0.0
    private let triangulator = Triangulator()

    init() {
        isValid = ok(gnCreateSession(&session))
        // The demo edition of the library does not save graphs — that is how we detect it (it also prints a line
        // about it)
        var length: Int32 = 0
        isDemoEdition = isValid && gnGetGraphStringBufLength(session, &length) == GN_RESULT_DEMO_LIMIT
    }

    deinit {
        if isValid {
            finishCooks()
            gnCloseSession(session)
        }
    }

    // Waits for the queued cooks before the graph is replaced (loading waits for it anyway — the session's cook lock)
    private func finishCooks() {
        for n in cookNodes { gnWaitCook(session, n.sop, -1) }
        cookNodes.removeAll()
        isCooking = false
        cookedFrame = -1
    }

    private func readTimeline() {
        var tl = GN_TimelineOptions()
        gnGetTimelineOptions(session, &tl)
        fps = tl.fps > 0 ? tl.fps : 24
        firstFrame = Int((tl.startTime * fps).rounded())
        lastFrame = max(firstFrame + 1, Int((tl.endTime * fps).rounded()))
        var time = 0.0
        gnGetTime(session, &time)
        startFrame = min(max(Int((time * fps).rounded()), firstFrame), lastFrame)
    }

    private func statusString(_ type: GN_StatusType) -> String {
        var len: Int32 = 0
        guard ok(gnGetStatusStringBufLength(session, type, GN_STATUSVERBOSITY_ERRORS, &len)), len > 0 else {
            return ""
        }
        var buffer = [CChar](repeating: 0, count: Int(len) + 1)
        gnGetStatusString(session, type, &buffer, len)
        return String(cString: buffer)
    }

    // Builds a demo scene in code (../../scenes): replaces the whole session graph through the API, without cooking.
    // Returns the text of the build problems (empty — all fine); the scene shows what was built
    func build(sceneIndex: Int) -> String {
        guard isValid else { return "gnCreateSession failed" }
        finishCooks()
        var errors = [CChar](repeating: 0, count: 16384)
        let count = demoSceneBuild(session, Int32(sceneIndex), &errors, Int32(errors.count))
        readTimeline()
        isLoaded = true
        return count == 0 ? "" : String(cString: errors)
    }

    // Loads a graph (without cooking): replaces the whole session graph. On failure — the error text
    func load(path: String) -> String? {
        guard isValid else { return "gnCreateSession failed" }
        finishCooks()
        // Relative file paths of the graph (File SOP: "assets/crystal.obj" of the demo scenes) are relative to the
        // folder of the graph: the library opens them relative to the current directory
        let url = URL(fileURLWithPath: path)
        FileManager.default.changeCurrentDirectoryPath(url.deletingLastPathComponent().path)
        // The graph is opened by path (UTF-8): the demo edition of the library recognizes the demo scenes by file name
        guard ok(gnLoadGraph(session, path, 0)) else {
            isLoaded = false
            // The file did not open — the library gives no text; otherwise the error text is in GN_STATUS_CALL_RESULT
            if !FileManager.default.fileExists(atPath: path) { return "Cannot open the file" }
            let text = statusString(GN_STATUS_CALL_RESULT)
            return text.isEmpty ? "gnLoadGraph failed" : text
        }
        readTimeline()
        isLoaded = true
        return nil
    }

    // Visible geo objects on /obj (the object's Display flag) → their Display SOPs (HAPI_GetDisplayGeoInfo)
    private func displayNodes() -> [DisplayNode] {
        var count: Int32 = 0
        let types = GN_NodeTypeBits(GN_NODETYPE_OBJ.rawValue)
        let flags = GN_NodeFlagsBits(GN_NODEFLAGS_DISPLAY.rawValue | GN_NODEFLAGS_OBJ_GEOMETRY.rawValue)
        guard ok(gnComposeChildNodeList(session, 0, types, flags, 0, &count)), count > 0 else { return [] }
        var objects = [GN_NodeId](repeating: 0, count: Int(count))
        gnGetComposedChildNodeList(session, 0, &objects, count)
        var result: [DisplayNode] = []
        for obj in objects {
            var geo = GN_GeoInfo()
            if ok(gnGetDisplayGeoInfo(session, obj, &geo)) {
                result.append(DisplayNode(object: obj, sop: geo.nodeId))
            }
        }
        return result
    }

    private func objectMatrix(_ object: GN_NodeId) -> Mat4 {
        // relative_to = -1 — the world transform (with the parents on input 0), as HAPI_GetObjectTransform. The
        // row-major matrix for row vectors has the same layout as a column-major one for column vectors
        var xf = GN_Transform()
        var m = [Float](repeating: 0, count: 16)
        guard ok(gnGetObjectTransform(session, object, -1, GN_SRT, &xf)),
            ok(gnConvertTransformQuatToMatrix(session, &xf, &m))
        else { return matrix_identity_float4x4 }
        return Mat4(columns: (
            SIMD4(m[0], m[1], m[2], m[3]), SIMD4(m[4], m[5], m[6], m[7]),
            SIMD4(m[8], m[9], m[10], m[11]), SIMD4(m[12], m[13], m[14], m[15])
        ))
    }

    // Queues the cook of a frame (gnSetTime + gnCookNode of the visible SOPs). Does not wait: cooks run on the
    // session's job thread
    func startCook(frame: Int) {
        guard isLoaded, !isCooking else { return }
        cookStart = CACurrentMediaTime()
        // Frame time as in the Qt client: frame / fps. Only time-dependent parameters change
        gnSetTime(session, Double(frame) / fps)

        var options = gnCookOptions_Create()
        // Whole faces: Triangulator splits them into triangles (by default the engine fans them from the first vertex,
        // and a triangle of a non-convex face crosses the concave corner). Curves come as splines, fetch evaluates them
        options.maxVerticesPerPrimitive = -1
        options.refineCurveToLinear = 0

        cookNodes = displayNodes()
        for n in cookNodes { gnCookNode(session, n.sop, &options) }
        cookingFrame = frame
        isCooking = true
    }

    // Whether the cook queued by startCook has finished. If so, the geometry has been read into `out`
    func pollCook(_ out: inout SceneGeometry) -> Bool {
        guard isCooking else { return false }
        for n in cookNodes {
            var state = GN_COOKSTATE_INVALID
            gnGetCookState(session, n.sop, &state)
            if state == GN_COOKSTATE_QUEUED || state == GN_COOKSTATE_COOKING { return false }
        }
        let cooked = CACurrentMediaTime()
        lastCookMs = (cooked - cookStart) * 1000
        fetch(&out)
        lastFetchMs = (CACurrentMediaTime() - cooked) * 1000
        isCooking = false
        cookedFrame = cookingFrame
        return true
    }

    // --- Reading geometry ------------------------------------------------------------------------------------------

    private struct Attr {
        var data: [Float] = []
        var tupleSize = 0
        var owner = GN_ATTROWNER_INVALID

        var exists: Bool { !data.isEmpty }

        // Element index for the attribute's class, -1 — no value
        func index(_ vertexIdx: Int, _ pointIdx: Int, _ primIdx: Int) -> Int {
            let idx: Int
            switch owner {
            case GN_ATTROWNER_VERTEX: idx = vertexIdx
            case GN_ATTROWNER_POINT: idx = pointIdx
            case GN_ATTROWNER_PRIM: idx = primIdx
            case GN_ATTROWNER_DETAIL: idx = 0
            default: return -1
            }
            if idx < 0 || tupleSize < 1 || (idx + 1) * tupleSize > data.count { return -1 }
            return idx
        }
    }

    private func readAttr(_ node: GN_NodeId, _ part: GN_PartId, _ name: String, _ owner: GN_AttributeOwner) -> Attr {
        var attr = Attr()
        var info = GN_AttributeInfo()
        guard ok(gnGetAttributeInfo(session, node, part, name, owner, &info)), info.exists != 0 else { return attr }
        if info.storage == GN_STORAGETYPE_STRING || info.tupleSize < 1 || info.count < 1 { return attr }
        attr.data = [Float](repeating: 0, count: Int(info.count) * Int(info.tupleSize))
        let r = attr.data.withUnsafeMutableBufferPointer {
            gnGetAttributeFloatData(session, node, part, name, &info, -1, $0.baseAddress, 0, info.count)
        }
        if !ok(r) {
            attr.data.removeAll()
            return attr
        }
        attr.tupleSize = Int(info.tupleSize)
        attr.owner = owner
        return attr
    }

    // As in Houdini: vertex, then point, primitive, detail
    private func findAttr(_ node: GN_NodeId, _ part: GN_PartId, _ name: String) -> Attr {
        for owner in [GN_ATTROWNER_VERTEX, GN_ATTROWNER_POINT, GN_ATTROWNER_PRIM, GN_ATTROWNER_DETAIL] {
            let attr = readAttr(node, part, name, owner)
            if attr.exists { return attr }
        }
        return Attr()
    }

    private static func fillColor(_ c: inout (Float, Float, Float, Float), _ Cd: Attr, _ v: Int, _ p: Int, _ prim: Int) {
        let ci = Cd.index(v, p, prim)
        if ci < 0 { return }
        let s = ci * Cd.tupleSize
        if Cd.tupleSize >= 3 {
            c.0 = Cd.data[s]
            c.1 = Cd.data[s + 1]
            c.2 = Cd.data[s + 2]
        } else {
            c.0 = Cd.data[s]
            c.1 = Cd.data[s]
            c.2 = Cd.data[s]
        }
    }

    private func fetch(_ out: inout SceneGeometry) {
        out.clear()
        for n in cookNodes {
            var geo = GN_GeoInfo()
            guard ok(gnGetGeoInfo(session, n.sop, &geo)) else { continue }
            var batch = DrawBatch()
            batch.model = objectMatrix(n.object)
            batch.firstVertex = out.vertices.count
            batch.firstTriangle = out.triangles.count
            batch.firstLine = out.lines.count
            batch.firstPoint = out.points.count
            for i in 0..<Int(geo.partCount) {
                var part = GN_PartInfo()
                if ok(gnGetPartInfo(session, n.sop, GN_PartId(i), &part)) {
                    fetchPart(n.sop, part, &out)
                }
            }
            batch.vertexCount = out.vertices.count - batch.firstVertex
            batch.triangleCount = out.triangles.count - batch.firstTriangle
            batch.lineCount = out.lines.count - batch.firstLine
            batch.pointCount = out.points.count - batch.firstPoint
            if batch.vertexCount > 0 { out.batches.append(batch) }
        }

        // Bounds — over the drawn vertices, in world space
        var lo = Vec3(repeating: .greatestFiniteMagnitude), hi = Vec3(repeating: -.greatestFiniteMagnitude)
        for b in out.batches {
            for i in b.firstVertex..<(b.firstVertex + b.vertexCount) {
                let v = out.vertices[i]
                let w = b.model * SIMD4(v.px, v.py, v.pz, 1)
                let p = Vec3(w.x, w.y, w.z)
                lo = simd_min(lo, p)
                hi = simd_max(hi, p)
            }
        }
        if !out.batches.isEmpty {
            out.boundsMin = lo
            out.boundsMax = hi
        }
    }

    private func fetchPart(_ node: GN_NodeId, _ part: GN_PartInfo, _ out: inout SceneGeometry) {
        let Pattr = readAttr(node, part.id, "P", GN_ATTROWNER_POINT)
        guard Pattr.exists, Pattr.tupleSize >= 3 else { return }
        let pointCount = Pattr.data.count / Pattr.tupleSize
        var P = [Float](repeating: 0, count: pointCount * 3)
        for i in 0..<pointCount {
            for k in 0..<3 { P[i * 3 + k] = Pattr.data[i * Pattr.tupleSize + k] }
        }

        var N = findAttr(node, part.id, "N")
        if N.tupleSize < 3 || N.owner == GN_ATTROWNER_DETAIL { N = Attr() }
        let Cd = findAttr(node, part.id, "Cd")

        let verticesBefore = out.vertices.count
        if part.type == GN_PARTTYPE_CURVE {
            fetchCurves(node, part, P, Cd, &out)
        } else {
            fetchMesh(node, part, P, N, Cd, &out)
        }

        // A part without primitives (Scatter, Add without polygons) — as points
        if out.vertices.count == verticesBefore {
            for i in 0..<pointCount {
                var c = (wireColor.0, wireColor.1, wireColor.2, Float(1))
                if Cd.owner == GN_ATTROWNER_POINT || Cd.owner == GN_ATTROWNER_DETAIL {
                    GnScene.fillColor(&c, Cd, -1, i, -1)
                }
                out.points.append(UInt32(out.vertices.count))
                out.vertices.append(makeVertex(P, i, c))
            }
        }
    }

    private func fetchMesh(_ node: GN_NodeId, _ part: GN_PartInfo, _ P: [Float], _ N: Attr, _ Cd: Attr,
                           _ out: inout SceneGeometry)
    {
        let pointCount = P.count / 3
        guard part.faceCount >= 1, part.vertexCount >= 1, pointCount >= 1 else { return }

        var faceCounts = [Int32](repeating: 0, count: Int(part.faceCount))
        var vertexList = [Int32](repeating: 0, count: Int(part.vertexCount))
        guard ok(gnGetFaceCounts(session, node, part.id, &faceCounts, 0, part.faceCount)),
            ok(gnGetVertexList(session, node, part.id, &vertexList, 0, part.vertexCount))
        else { return }

        let totalVertices = Int(part.vertexCount)
        out.vertices.reserveCapacity(out.vertices.count + totalVertices)
        var vOffset = 0
        for prim in 0..<Int(part.faceCount) {
            let numV = Int(faceCounts[prim])
            if numV < 1 { continue }
            if vOffset + numV > totalVertices { break }

            let base = out.vertices.count
            let line = numV < 3
            for v in 0..<numV {
                let vIdx = vOffset + v
                var pIdx = Int(vertexList[vIdx])
                if pIdx < 0 || pIdx >= pointCount { pIdx = 0 }

                let base3 = line ? wireColor : surfaceColor
                var c = (base3.0, base3.1, base3.2, Float(1))
                GnScene.fillColor(&c, Cd, vIdx, pIdx, prim)
                var vx = makeVertex(P, pIdx, c)
                let ni = N.index(vIdx, pIdx, prim)
                if ni >= 0 {
                    let s = ni * N.tupleSize
                    vx.nx = N.data[s]
                    vx.ny = N.data[s + 1]
                    vx.nz = N.data[s + 2]
                }
                out.vertices.append(vx)
            }

            // Without N — the flat face normal (Newell's method). Houdini polygon vertices go clockwise when seen from
            // the front, so the Newell normal is flipped
            if !N.exists && numV >= 3 {
                var n = Vec3(0, 0, 0)
                for v in 0..<numV {
                    let a = out.vertices[base + v], b = out.vertices[base + (v + 1) % numV]
                    n.x += (a.py - b.py) * (a.pz + b.pz)
                    n.y += (a.pz - b.pz) * (a.px + b.px)
                    n.z += (a.px - b.px) * (a.py + b.py)
                }
                let len = simd_length(n)
                if len > 0 {
                    for v in 0..<numV {
                        out.vertices[base + v].nx = -n.x / len
                        out.vertices[base + v].ny = -n.y / len
                        out.vertices[base + v].nz = -n.z / len
                    }
                }
            }

            if numV == 1 {
                out.points.append(UInt32(base))
            } else if numV == 2 {
                out.lines.append(UInt32(base))
                out.lines.append(UInt32(base + 1))
            } else {
                triangulator.triangulate(n: numV, base: base, vertices: out.vertices, tris: &out.triangles)
            }
            vOffset += numV
        }
    }

    private func fetchCurves(_ node: GN_NodeId, _ part: GN_PartInfo, _ P: [Float], _ Cd: Attr,
                             _ out: inout SceneGeometry)
    {
        let pointCount = P.count / 3
        guard pointCount >= 1 else { return }
        var info = GN_CurveInfo()
        guard ok(gnGetCurveInfo(session, node, part.id, &info)) else { return }
        let curveCount = Int(info.curveCount > 0 ? info.curveCount : part.faceCount)
        guard curveCount >= 1 else { return }
        var curveCounts = [Int32](repeating: 0, count: curveCount)
        guard ok(gnGetCurveCounts(session, node, part.id, &curveCounts, 0, Int32(curveCount))) else { return }

        // Curve vertices refer to points through the vertex list; without it the points go in order
        var vertexList: [Int32] = []
        if part.vertexCount > 0 {
            vertexList = [Int32](repeating: 0, count: Int(part.vertexCount))
            if !ok(gnGetVertexList(session, node, part.id, &vertexList, 0, part.vertexCount)) {
                vertexList.removeAll()
            }
        }

        if info.curveType == GN_CURVETYPE_NURBS || info.curveType == GN_CURVETYPE_BEZIER {
            fetchSplines(node, part, info, curveCounts, vertexList, P, Cd, &out)
            return
        }

        // Polylines — segments between the vertices
        let closed = info.isPeriodic != 0 || info.isClosed != 0
        var vOffset = 0
        for prim in 0..<curveCount {
            let numV = Int(curveCounts[prim])
            if numV < 1 { continue }
            let base = UInt32(out.vertices.count)
            for v in 0..<numV {
                let vIdx = vOffset + v
                var pIdx = vIdx < vertexList.count ? Int(vertexList[vIdx]) : vIdx
                if pIdx < 0 || pIdx >= pointCount { pIdx = 0 }
                var c = (wireColor.0, wireColor.1, wireColor.2, Float(1))
                GnScene.fillColor(&c, Cd, vIdx, pIdx, prim)
                out.vertices.append(makeVertex(P, pIdx, c))
            }
            if numV == 1 {
                out.points.append(base)
            } else {
                for v in 0..<(numV - 1) {
                    out.lines.append(base + UInt32(v))
                    out.lines.append(base + UInt32(v + 1))
                }
                if closed && numV > 2 {
                    out.lines.append(base + UInt32(numV - 1))
                    out.lines.append(base)
                }
            }
            vOffset += numV
        }
    }

    private func fetchSplines(_ node: GN_NodeId, _ part: GN_PartInfo, _ info: GN_CurveInfo, _ curveCounts: [Int32],
                              _ vertexList: [Int32], _ P: [Float], _ Cd: Attr, _ out: inout SceneGeometry)
    {
        let curveCount = curveCounts.count
        let pointCount = P.count / 3

        // Orders: one for all or per curve
        var orders = [Int32](repeating: info.order, count: curveCount)
        if info.order <= 0 && !ok(gnGetCurveOrders(session, node, part.id, &orders, 0, Int32(curveCount))) {
            return
        }

        // Knots: count + order per curve — unrolled B-spline knots (for Bezier — breakpoints of multiplicity order − 1)
        var knots: [Float] = []
        if info.hasKnots != 0 && info.knotCount > 0 {
            knots = [Float](repeating: 0, count: Int(info.knotCount))
            guard ok(gnGetCurveKnots(session, node, part.id, &knots, 0, info.knotCount)) else { return }
        } else {
            for i in 0..<curveCount {
                let n = Int(curveCounts[i]), k = Int(orders[i])
                let segments = k > 1 ? max(1, (n - 1) / (k - 1)) : 1
                for s in 0...segments {
                    for _ in 0..<(s == 0 || s == segments ? k : k - 1) { knots.append(Float(s)) }
                }
            }
        }

        // CV weights — Pw (rational curves)
        let Pw = readAttr(node, part.id, "Pw", GN_ATTROWNER_POINT)

        var N: [Double] = []
        var cvVertex: [Int] = [], cvPoint: [Int] = []
        var params: [Double] = []
        var vOffset = 0
        var kOffset = 0
        for prim in 0..<curveCount {
            let n = Int(curveCounts[prim])
            let k = Int(orders[prim])
            if n < 1 || k < 2 || k > 15 || kOffset + n + k > knots.count {
                vOffset += n
                kOffset += max(n, 0) + max(k, 0)
                continue
            }
            let t = kOffset
            cvVertex = Array(vOffset..<(vOffset + n))
            cvPoint = cvVertex.map { v in
                let p = v < vertexList.count ? Int(vertexList[v]) : v
                return p >= 0 && p < pointCount ? p : 0
            }

            // Sample parameters: each non-empty span of the domain [t[k − 1], t[n]] in splineDivisions parts
            params.removeAll(keepingCapacity: true)
            for s in (k - 1)..<n where knots[t + s + 1] > knots[t + s] {
                let a = Double(knots[t + s]), b = Double(knots[t + s + 1])
                for d in 0..<splineDivisions { params.append(a + (b - a) * Double(d) / Double(splineDivisions)) }
            }
            params.append(Double(knots[t + n]))

            let base = UInt32(out.vertices.count)
            N = [Double](repeating: 0, count: k)
            for u in params {
                let span = findSpan(knots, t, k, n, u)
                basisFuns(knots, t, span, k, u, &N)
                let first = span - k + 1
                // Rational basis R_j = N_j·w_j / Σ — for the position and the color
                var wsum = 0.0
                for j in 0..<k {
                    let pw = Pw.index(cvVertex[first + j], cvPoint[first + j], prim)
                    N[j] *= pw >= 0 ? Double(Pw.data[pw * Pw.tupleSize]) : 1
                    wsum += N[j]
                }
                if wsum != 0 {
                    for j in 0..<k { N[j] /= wsum }
                }

                var vx = Vertex()
                for j in 0..<k {
                    let cv = first + j
                    var cj = (wireColor.0, wireColor.1, wireColor.2, Float(1))
                    GnScene.fillColor(&cj, Cd, cvVertex[cv], cvPoint[cv], prim)
                    let w = Float(N[j])
                    let p = cvPoint[cv] * 3
                    vx.px += w * P[p]
                    vx.py += w * P[p + 1]
                    vx.pz += w * P[p + 2]
                    vx.r += w * cj.0
                    vx.g += w * cj.1
                    vx.b += w * cj.2
                    vx.a += w * cj.3
                }
                out.vertices.append(vx)
            }
            for i in 1..<params.count {
                out.lines.append(base + UInt32(i - 1))
                out.lines.append(base + UInt32(i))
            }
            vOffset += n
            kOffset += n + k
        }
    }
}
