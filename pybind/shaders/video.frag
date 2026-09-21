#version 450

layout(set = 0, binding = 0) uniform sampler2D plane_y;    // luma, one channel
layout(set = 0, binding = 1) uniform sampler2D plane_uv;   // interleaved chroma, two channels

// The same numbers as YuvToRgbParams (cuda_driver.h): levels are those of the raw samples, so
// `sample_max` (255 for 8-bit, 65535 for the 16-bit P010/P016) turns a normalised texel back into one.
layout(push_constant) uniform Params {
    float y_offset;
    float y_scale;
    float c_mid;
    float c_scale;
    float rv;
    float gu;
    float gv;
    float bu;
    float sample_max;
    float uv_scale_x;   // the visible part of the image: its size over the size of the image the decoder allocated
    float uv_scale_y;
    float uv_max_x;     // the last position that may be sampled, half a texel inside the visible part
    float uv_max_y;
} p;

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;

void main()
{
    vec2 st = min(uv * vec2(p.uv_scale_x, p.uv_scale_y), vec2(p.uv_max_x, p.uv_max_y));
    float y = (texture(plane_y, st).r * p.sample_max - p.y_offset) * p.y_scale;
    vec2 c = (texture(plane_uv, st).rg * p.sample_max - p.c_mid) * p.c_scale;   // (Cb, Cr)
    vec3 rgb = vec3(y + p.rv * c.y,
                    y - p.gu * c.x - p.gv * c.y,
                    y + p.bu * c.x);
    color = vec4(clamp(rgb, 0.0, 1.0), 1.0);
}
