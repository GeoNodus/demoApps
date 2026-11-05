#pragma once

// Matrices of the demo apps: 4x4 column-major (as glUniformMatrix4fv and GLSL expect), column vectors: p' = M * p
#include <cmath>

namespace demo {

struct Vec3
{
    float x = 0, y = 0, z = 0;
};

inline Vec3 operator+(Vec3 a, Vec3 b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}
inline Vec3 operator-(Vec3 a, Vec3 b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}
inline Vec3 operator*(Vec3 a, float s)
{
    return {a.x * s, a.y * s, a.z * s};
}
inline float dot(Vec3 a, Vec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
inline Vec3 cross(Vec3 a, Vec3 b)
{
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline Vec3 normalize(Vec3 a)
{
    const float l = std::sqrt(dot(a, a));
    return l > 0 ? a * (1.0f / l) : a;
}

struct Mat4
{
    float m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}; // m[column * 4 + row]

    float& at(int row, int col) { return m[col * 4 + row]; }
    float at(int row, int col) const { return m[col * 4 + row]; }
};

inline Mat4 operator*(const Mat4& a, const Mat4& b)
{
    Mat4 r;
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col) {
            float s = 0;
            for (int k = 0; k < 4; ++k)
                s += a.at(row, k) * b.at(k, col);
            r.at(row, col) = s;
        }
    return r;
}

// The camera looks along −Z of its space, Y is up (as in OpenGL)
inline Mat4 lookAt(Vec3 eye, Vec3 target, Vec3 up)
{
    const Vec3 f = normalize(target - eye);
    const Vec3 s = normalize(cross(f, up));
    const Vec3 u = cross(s, f);
    Mat4 r;
    r.at(0, 0) = s.x, r.at(0, 1) = s.y, r.at(0, 2) = s.z, r.at(0, 3) = -dot(s, eye);
    r.at(1, 0) = u.x, r.at(1, 1) = u.y, r.at(1, 2) = u.z, r.at(1, 3) = -dot(u, eye);
    r.at(2, 0) = -f.x, r.at(2, 1) = -f.y, r.at(2, 2) = -f.z, r.at(2, 3) = dot(f, eye);
    return r;
}

// Perspective. OpenGL: NDC depth −1..1, Y up; Vulkan: depth 0..1, framebuffer Y down — Y is flipped so that the
// picture is the same
inline Mat4 perspective(float fovyRadians, float aspect, float zNear, float zFar, bool vulkan)
{
    const float f = 1.0f / std::tan(fovyRadians * 0.5f);
    Mat4 r;
    r.at(0, 0) = f / aspect;
    r.at(1, 1) = vulkan ? -f : f;
    r.at(3, 3) = 0;
    r.at(3, 2) = -1;
    if (vulkan) {
        r.at(2, 2) = zFar / (zNear - zFar);
        r.at(2, 3) = zNear * zFar / (zNear - zFar);
    } else {
        r.at(2, 2) = (zFar + zNear) / (zNear - zFar);
        r.at(2, 3) = 2 * zFar * zNear / (zNear - zFar);
    }
    return r;
}

// Per-object shader constants (Vulkan push constants — 128 bytes, OpenGL uniforms — the same): proj · view · model,
// the first three rows of view · model (position in camera space; the normal uses the inverse transpose of their 3x3,
// computed by the vertex shader) and parameters
struct ShaderConstants
{
    float mvp[16];
    float modelViewRows[3][4];
    float params[4]; // x — point size, y — 1 lit / 0 color as is
};

inline ShaderConstants shaderConstants(const Mat4& proj, const Mat4& view, const Mat4& model, float pointSize)
{
    ShaderConstants c{};
    const Mat4 mv = view * model;
    const Mat4 mvp = proj * mv;
    for (int k = 0; k < 16; ++k)
        c.mvp[k] = mvp.m[k];
    for (int r = 0; r < 3; ++r)
        for (int k = 0; k < 4; ++k)
            c.modelViewRows[r][k] = mv.at(r, k);
    c.params[0] = pointSize;
    return c;
}
static_assert(sizeof(ShaderConstants) == 128);

} // namespace demo
