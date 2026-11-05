#version 450
// The same shaders as in the OpenGL version (gl/gl_renderer.cpp); constants — demo::ShaderConstants (push constants)
layout(push_constant) uniform Constants
{
    mat4 mvp;
    vec4 modelViewRows[3];
    vec4 params; // x — point size, y — 1 lit / 0 color as is
} pc;

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec4 a_color;

layout(location = 0) out vec3 v_viewPos;
layout(location = 1) out vec3 v_normal;
layout(location = 2) out vec4 v_color;

void main()
{
    vec4 p = vec4(a_position, 1.0);
    v_viewPos = vec3(dot(pc.modelViewRows[0], p), dot(pc.modelViewRows[1], p), dot(pc.modelViewRows[2], p));
    // Normal — by the inverse transpose of model-view: a mat3 built from the rows is the transpose, its inverse is
    // what we need
    v_normal = inverse(mat3(pc.modelViewRows[0].xyz, pc.modelViewRows[1].xyz, pc.modelViewRows[2].xyz)) * a_normal;
    v_color = a_color;
    gl_Position = pc.mvp * p;
    gl_PointSize = pc.params.x;
}
