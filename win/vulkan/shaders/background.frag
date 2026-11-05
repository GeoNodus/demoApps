#version 450
layout(location = 0) in float v_t;
layout(location = 0) out vec4 o_color;

void main()
{
    o_color = vec4(mix(vec3(0.13, 0.13, 0.14), vec3(0.36, 0.38, 0.42), v_t), 1.0);
}
