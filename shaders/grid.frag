#version 440

layout(location = 0) in vec3 input_color;
layout(location = 0) out vec4 frag_color;

void main()
{
    frag_color = vec4(input_color, 0.8);
}
