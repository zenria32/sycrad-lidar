#version 440

layout(location = 0) in vec4 frag_color;
layout(location = 1) in vec3 frag_normal;

layout(location = 0) out vec4 output_color;

void main()
{
    output_color = frag_color;
}
