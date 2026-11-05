#version 450
layout(push_constant) uniform Constants
{
    mat4 mvp;
    vec4 modelViewRows[3];
    vec4 params;
} pc;

layout(location = 0) in vec3 v_viewPos;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec4 v_color;

layout(location = 0) out vec4 o_color;

void main()
{
    if (pc.params.y < 0.5) {
        o_color = vec4(v_color.rgb, 1.0);
        return;
    }
    // Face normal from the position derivatives, turned toward the camera: depends neither on the polygon winding nor
    // on the frame axis directions. Back faces are lit with the flipped normal (two-sided lighting, as in the Houdini
    // viewport)
    vec3 toEye = normalize(-v_viewPos);
    vec3 faceN = normalize(cross(dFdx(v_viewPos), dFdy(v_viewPos)));
    if (dot(faceN, toEye) < 0.0)
        faceN = -faceN;
    vec3 n = dot(v_normal, v_normal) > 1e-30 ? normalize(v_normal) : faceN;
    if (dot(n, faceN) < 0.0)
        n = -n;
    // Light from the camera, slightly from above and the left, plus a highlight
    vec3 L = normalize(vec3(-0.3, 0.45, 1.0));
    float diffuse = max(dot(n, L), 0.0);
    float spec = pow(max(dot(n, normalize(L + toEye)), 0.0), 48.0) * 0.2;
    o_color = vec4(v_color.rgb * (0.18 + 0.82 * diffuse) + vec3(spec), 1.0);
}
