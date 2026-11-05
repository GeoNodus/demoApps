// Metal renderer of the demo app: the same picture as the OpenGL and Vulkan demos on Windows — a vertical gradient
// background, lit surfaces (two-sided, the face normal from the position derivatives), lines and points in their color,
// MSAA 4x. Shaders are compiled from source when the app starts (no build-time Metal toolchain needed)
import Metal
import MetalKit

private let shaderSource = """
#include <metal_stdlib>
using namespace metal;

struct VertexIn {
    packed_float3 p;
    packed_float3 n;
    packed_float4 c;
};

struct Constants {
    float4x4 mvp;
    float4 modelViewRows[3];
    float4 params; // x — point size, y — 1 lit / 0 color as is
};

struct VertexOut {
    float4 position [[position]];
    float3 viewPos;
    float3 normal;
    float4 color;
    float pointSize [[point_size]];
};

vertex VertexOut meshVertex(uint vid [[vertex_id]], const device VertexIn* vertices [[buffer(0)]],
                            constant Constants& c [[buffer(1)]])
{
    const VertexIn v = vertices[vid];
    const float4 p = float4(float3(v.p), 1.0);
    VertexOut o;
    o.viewPos = float3(dot(c.modelViewRows[0], p), dot(c.modelViewRows[1], p), dot(c.modelViewRows[2], p));
    // Normal — by the inverse transpose of the model-view 3x3 (rows r0, r1, r2): its rows are the cross products of
    // pairs of rows, up to 1 / det (only the direction matters, the sign is fixed in the fragment shader)
    const float3 r0 = c.modelViewRows[0].xyz, r1 = c.modelViewRows[1].xyz, r2 = c.modelViewRows[2].xyz;
    const float3 n = float3(v.n);
    o.normal = float3(dot(cross(r1, r2), n), dot(cross(r2, r0), n), dot(cross(r0, r1), n));
    o.color = float4(v.c);
    o.position = c.mvp * p;
    o.pointSize = c.params.x;
    return o;
}

fragment float4 meshFragment(VertexOut in [[stage_in]], constant Constants& c [[buffer(1)]])
{
    if (c.params.y < 0.5)
        return float4(in.color.rgb, 1.0);
    // Face normal from the position derivatives, turned toward the camera: depends neither on the polygon winding nor
    // on the frame axis directions. Back faces are lit with the flipped normal (two-sided lighting, as in the Houdini
    // viewport)
    const float3 toEye = normalize(-in.viewPos);
    float3 faceN = normalize(cross(dfdx(in.viewPos), dfdy(in.viewPos)));
    if (dot(faceN, toEye) < 0.0)
        faceN = -faceN;
    float3 n = dot(in.normal, in.normal) > 1e-30 ? normalize(in.normal) : faceN;
    if (dot(n, faceN) < 0.0)
        n = -n;
    // Light from the camera, slightly from above and the left, plus a highlight
    const float3 L = normalize(float3(-0.3, 0.45, 1.0));
    const float diffuse = max(dot(n, L), 0.0);
    const float spec = pow(max(dot(n, normalize(L + toEye)), 0.0), 48.0) * 0.2;
    return float4(in.color.rgb * (0.18 + 0.82 * diffuse) + float3(spec), 1.0);
}

struct BackgroundOut {
    float4 position [[position]];
    float t;
};

vertex BackgroundOut backgroundVertex(uint vid [[vertex_id]])
{
    const float2 p = float2(vid == 1 ? 3.0 : -1.0, vid == 2 ? 3.0 : -1.0);
    BackgroundOut o;
    o.t = p.y * 0.5 + 0.5; // 1 — top of the frame
    o.position = float4(p, 0.0, 1.0);
    return o;
}

fragment float4 backgroundFragment(BackgroundOut in [[stage_in]])
{
    return float4(mix(float3(0.13, 0.13, 0.14), float3(0.36, 0.38, 0.42), in.t), 1.0);
}
"""

struct ViewParams {
    var view: Mat4 // world → camera space
    var fovy: Float // radians
    var zNear: Float
    var zFar: Float
}

final class MetalRenderer {
    let device: MTLDevice
    private let queue: MTLCommandQueue
    private let meshPipeline: MTLRenderPipelineState
    private let backgroundPipeline: MTLRenderPipelineState
    private let meshDepth: MTLDepthStencilState
    private let backgroundDepth: MTLDepthStencilState

    private var vertexBuffer: MTLBuffer?
    private var indexBuffer: MTLBuffer?
    private var batches: [DrawBatch] = []
    private var triCount = 0, lineCount = 0

    // Point size in pixels of the drawable (the Windows demos use 4 pixels at 100% scale)
    var pointSize: Float = 4

    static let sampleCount = 4
    static let colorFormat = MTLPixelFormat.bgra8Unorm
    static let depthFormat = MTLPixelFormat.depth32Float

    var description: String { "Metal — \(device.name)" }

    // On failure — a message for the user
    init(device: MTLDevice) throws {
        self.device = device
        guard let queue = device.makeCommandQueue() else {
            throw NSError(domain: "GeoNodus", code: 1, userInfo: [NSLocalizedDescriptionKey: "No Metal command queue"])
        }
        self.queue = queue
        let library = try device.makeLibrary(source: shaderSource, options: nil)

        func pipeline(_ vs: String, _ fs: String) throws -> MTLRenderPipelineState {
            let d = MTLRenderPipelineDescriptor()
            d.vertexFunction = library.makeFunction(name: vs)
            d.fragmentFunction = library.makeFunction(name: fs)
            d.colorAttachments[0].pixelFormat = MetalRenderer.colorFormat
            d.depthAttachmentPixelFormat = MetalRenderer.depthFormat
            d.rasterSampleCount = MetalRenderer.sampleCount
            return try device.makeRenderPipelineState(descriptor: d)
        }
        meshPipeline = try pipeline("meshVertex", "meshFragment")
        backgroundPipeline = try pipeline("backgroundVertex", "backgroundFragment")

        let md = MTLDepthStencilDescriptor()
        md.depthCompareFunction = .lessEqual
        md.isDepthWriteEnabled = true
        meshDepth = device.makeDepthStencilState(descriptor: md)!
        // Background without depth test and depth writes
        let bd = MTLDepthStencilDescriptor()
        bd.depthCompareFunction = .always
        bd.isDepthWriteEnabled = false
        backgroundDepth = device.makeDepthStencilState(descriptor: bd)!
    }

    // New frame geometry: copied into GPU buffers. New buffers every time — the previous ones stay alive while a frame
    // in flight still uses them
    func setGeometry(_ g: SceneGeometry) {
        batches = g.batches
        triCount = g.triangles.count
        lineCount = g.lines.count
        if g.vertices.isEmpty {
            vertexBuffer = nil
            indexBuffer = nil
            return
        }
        vertexBuffer = g.vertices.withUnsafeBytes {
            device.makeBuffer(bytes: $0.baseAddress!, length: $0.count, options: .storageModeShared)
        }
        // Indices: triangles, then lines, then points
        let indexCount = g.triangles.count + g.lines.count + g.points.count
        guard indexCount > 0,
            let ib = device.makeBuffer(length: indexCount * MemoryLayout<UInt32>.stride, options: .storageModeShared)
        else {
            indexBuffer = nil
            return
        }
        let dst = ib.contents().bindMemory(to: UInt32.self, capacity: indexCount)
        var at = 0
        for array in [g.triangles, g.lines, g.points] {
            array.withUnsafeBufferPointer { src in
                if let p = src.baseAddress { (dst + at).update(from: p, count: src.count) }
            }
            at += array.count
        }
        indexBuffer = ib
    }

    func render(in view: MTKView, params: ViewParams) {
        guard let pass = view.currentRenderPassDescriptor, let drawable = view.currentDrawable,
            let cmd = queue.makeCommandBuffer(), let enc = cmd.makeRenderCommandEncoder(descriptor: pass)
        else { return }

        enc.setRenderPipelineState(backgroundPipeline)
        enc.setDepthStencilState(backgroundDepth)
        enc.drawPrimitives(type: .triangle, vertexStart: 0, vertexCount: 3)

        let size = view.drawableSize
        let proj = perspective(fovy: params.fovy, aspect: Float(size.width / max(size.height, 1)), near: params.zNear,
                               far: params.zFar)
        if let vb = vertexBuffer, let ib = indexBuffer {
            enc.setRenderPipelineState(meshPipeline)
            enc.setDepthStencilState(meshDepth)
            enc.setVertexBuffer(vb, offset: 0, index: 0)
            let stride = MemoryLayout<UInt32>.stride
            // Per object: its own matrix; indices in the buffer — triangles, lines, points
            for b in batches {
                var c = ShaderConstants(proj: proj, view: params.view, model: b.model, pointSize: pointSize)
                let constantsSize = MemoryLayout<ShaderConstants>.stride
                if b.triangleCount > 0 {
                    c.params.y = 1
                    enc.setVertexBytes(&c, length: constantsSize, index: 1)
                    enc.setFragmentBytes(&c, length: constantsSize, index: 1)
                    // The fill is pushed back so that lines and points on the surface do not flicker
                    enc.setDepthBias(1, slopeScale: 1, clamp: 0)
                    enc.drawIndexedPrimitives(type: .triangle, indexCount: b.triangleCount, indexType: .uint32,
                                              indexBuffer: ib, indexBufferOffset: b.firstTriangle * stride)
                    enc.setDepthBias(0, slopeScale: 0, clamp: 0)
                }
                if b.lineCount == 0 && b.pointCount == 0 { continue }
                c.params.y = 0
                enc.setVertexBytes(&c, length: constantsSize, index: 1)
                enc.setFragmentBytes(&c, length: constantsSize, index: 1)
                if b.lineCount > 0 {
                    enc.drawIndexedPrimitives(type: .line, indexCount: b.lineCount, indexType: .uint32,
                                              indexBuffer: ib, indexBufferOffset: (triCount + b.firstLine) * stride)
                }
                if b.pointCount > 0 {
                    enc.drawIndexedPrimitives(type: .point, indexCount: b.pointCount, indexType: .uint32,
                                              indexBuffer: ib,
                                              indexBufferOffset: (triCount + lineCount + b.firstPoint) * stride)
                }
            }
        }
        enc.endEncoding()
        cmd.present(drawable)
        cmd.commit()
    }
}
