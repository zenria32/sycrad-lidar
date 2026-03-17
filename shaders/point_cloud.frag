#version 440

layout(location = 0) in float input_intensity;
layout(location = 1) in vec3 input_color;
flat layout(location = 2) in int input_color_mode;

layout(location = 0) out vec4 frag_color;

vec3 color_map(float c) {
    const vec3 c0 = vec3(0.1140890109226559, 0.06288340699912215, 0.2248337216805064);
    const vec3 c1 = vec3(6.716419496985708, 3.182286745507602, 7.571581586103393);
    const vec3 c2 = vec3(-66.09402360453038, -4.9279827041226, -10.09439367561635);
    const vec3 c3 = vec3(228.7660791526501, 25.04986699771073, -91.54105330182436);
    const vec3 c4 = vec3(-334.8351565777451, -69.31749712757485, 288.5858850615712);
    const vec3 c5 = vec3(218.7637218434795, 67.52150567819112, -305.2045772184957);
    const vec3 c6 = vec3(-52.88903478218835, -21.54527364654712, 110.5174647748972);

    return c0 + c * (c1 + c * (c2 + c * (c3 + c * (c4 + c * (c5 + c * c6)))));
}

vec3 enhance_color(vec3 color)
{
    color = clamp(color, 0.0, 1.0);
    const vec3 luma_weights = vec3(0.2126, 0.7152, 0.0722);
    float luma = dot(color, luma_weights);
    color = mix(vec3(luma), color, 1.25);
    color = clamp(color, 0.0, 1.0);
    color = pow(color, vec3(0.8)) * 1.08;
    return clamp(color, 0.0, 1.0);
}

void main()
{
    vec2 circle_uv = gl_PointCoord * 2.0 - 1.0;
    if (dot(circle_uv, circle_uv) > 1.0) {
        discard;
    }

    vec3 color = input_color;
    if (input_color_mode == 0) {
        float c = clamp(input_intensity, 0.0, 1.0);
        c = pow(c, 0.6);
        color = color_map(c);
    }

    color = enhance_color(color);

    frag_color = vec4(color, 1.0);
}
