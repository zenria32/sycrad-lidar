#version 440

layout(location = 0) in vec3 input_position;
layout(location = 1) in float input_scalar;

layout(std140, binding = 0) uniform uniform_buffer {
    mat4 mvp;
    float min_intensity;
    float max_intensity;
    float point_size;
    float color_mode;
} ubo;

layout(location = 0) out float output_intensity;
layout(location = 1) out vec3 output_color;
flat layout(location = 2) out int output_color_mode;

vec3 unpack_rgb(float packed_rgb)
{
    uint packed = floatBitsToUint(packed_rgb);
    float r = float((packed >> 16u) & 0xffu) / 255.0;
    float g = float((packed >> 8u) & 0xffu) / 255.0;
    float b = float(packed & 0xffu) / 255.0;
    return vec3(r, g, b);
}

void main()
{
    gl_Position = ubo.mvp * vec4(input_position, 1.0);
    gl_PointSize = ubo.point_size;

    output_color_mode = int(ubo.color_mode + 0.5);
    output_intensity = 0.0;
    output_color = vec3(0.93, 0.93, 0.93);

    if (output_color_mode == 0) {
        float range = max(ubo.max_intensity - ubo.min_intensity, 1e-6);
        output_intensity = clamp((input_scalar - ubo.min_intensity) / range, 0.0, 1.0);
    } else if (output_color_mode == 1) {
        output_color = unpack_rgb(input_scalar);
    }
}
