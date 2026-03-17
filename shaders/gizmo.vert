#version 440

layout(location = 0) in vec3 input_position;

layout(std140, binding = 0) uniform uniform_buffer {
    mat4 mvp;
    vec4 color;
    float world_scale;
    float padding1;
    float padding2;
    float padding3;
} ubo;

layout(location = 0) out vec4 frag_color;

void main()
{
    vec3 scaled_position = input_position * ubo.world_scale;
    gl_Position = ubo.mvp * vec4(scaled_position, 1.0);
    frag_color = ubo.color;
}
