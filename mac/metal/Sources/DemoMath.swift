// Matrices of the demo app: simd 4x4, column-major (as Metal expects), column vectors: p' = M * p
import simd

typealias Vec3 = SIMD3<Float>
typealias Mat4 = simd_float4x4

// The camera looks along −Z of its space, Y is up
func lookAt(eye: Vec3, target: Vec3, up: Vec3) -> Mat4 {
    let f = simd_normalize(target - eye)
    let s = simd_normalize(simd_cross(f, up))
    let u = simd_cross(s, f)
    return Mat4(rows: [
        SIMD4(s.x, s.y, s.z, -simd_dot(s, eye)),
        SIMD4(u.x, u.y, u.z, -simd_dot(u, eye)),
        SIMD4(-f.x, -f.y, -f.z, simd_dot(f, eye)),
        SIMD4(0, 0, 0, 1),
    ])
}

// Perspective for Metal: NDC depth 0..1, Y up (the picture is the same as in the OpenGL demo on Windows)
func perspective(fovy: Float, aspect: Float, near: Float, far: Float) -> Mat4 {
    let f = 1 / tan(fovy * 0.5)
    return Mat4(rows: [
        SIMD4(f / aspect, 0, 0, 0),
        SIMD4(0, f, 0, 0),
        SIMD4(0, 0, far / (near - far), near * far / (near - far)),
        SIMD4(0, 0, -1, 0),
    ])
}

// Per-object shader constants (128 bytes, the same layout as the Windows demos): proj · view · model, the first three
// rows of view · model (position in camera space; the normal uses the inverse transpose of their 3x3, computed by the
// vertex shader) and parameters
struct ShaderConstants {
    var mvp: Mat4
    var modelViewRow0: SIMD4<Float>
    var modelViewRow1: SIMD4<Float>
    var modelViewRow2: SIMD4<Float>
    var params: SIMD4<Float> // x — point size, y — 1 lit / 0 color as is

    init(proj: Mat4, view: Mat4, model: Mat4, pointSize: Float) {
        let mv = view * model
        mvp = proj * mv
        let t = mv.transpose // columns of the transpose are the rows of mv
        modelViewRow0 = t.columns.0
        modelViewRow1 = t.columns.1
        modelViewRow2 = t.columns.2
        params = SIMD4(pointSize, 0, 0, 0)
    }
}
