#version 440

layout(location = 0) in vec3 input_position;
layout(location = 1) in vec3 input_normal;
layout(location = 2) in vec4 input_color;

layout(std140, binding = 0) uniform uniform_buffer {
    mat4 mvp;
    float padding[4];
} ubo;

layout(location = 0) out vec4 frag_color;
layout(location = 1) out vec3 frag_normal;

void main()
{
    gl_Position = ubo.mvp * vec4(input_position, 1.0);
    frag_color = input_color;
    frag_normal = input_normal;
}
