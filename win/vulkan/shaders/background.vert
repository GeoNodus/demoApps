#version 450
// Background — a vertical gradient drawn with one full-screen triangle
layout(location = 0) out float v_t;

void main()
{
    vec2 p = vec2(gl_VertexIndex == 1 ? 3.0 : -1.0, gl_VertexIndex == 2 ? 3.0 : -1.0);
    v_t = 0.5 - p.y * 0.5; // 1 — top of the frame (Vulkan framebuffer Y points down)
    gl_Position = vec4(p, 0.0, 1.0);
}
