// NV12 / P010 / P016 -> RGB24 for CUDA frames, one thread per pixel.
//
// This is compiled to PTX (checked in as ../../include/nv12_to_rgb_ptx.h, which the CUDA
// driver JIT-compiles for whatever GPU is present), so neither the CUDA toolkit
// nor nvcc is needed to build the module. Regenerate that header with:
//
//   clang -target nvptx64-nvidia-cuda -march=sm_50 -x cl -cl-std=CL1.2 -O2 -S \
//         nv12_to_rgb.cl -o nv12_to_rgb.ptx
//
// and paste the PTX into the raw string in the header.
//
// Chroma is sampled nearest-neighbour (each U/V pair covers a 2x2 pixel block).
// All colour parameters are computed on the host (see CudaDriver::nv12_to_rgb).

__kernel void nv12_to_rgb(__global const uchar *y_plane, __global const uchar *uv_plane, __global uchar *dst,
                          uint y_pitch, uint uv_pitch, uint width, uint height, uint bytes_per_sample,
                          float y_offset, float y_scale, float c_mid, float c_scale,
                          float rv, float gu, float gv, float bu)
{
    const uint x = __nvvm_read_ptx_sreg_ctaid_x() * __nvvm_read_ptx_sreg_ntid_x() +
                   __nvvm_read_ptx_sreg_tid_x();
    const uint row = __nvvm_read_ptx_sreg_ctaid_y() * __nvvm_read_ptx_sreg_ntid_y() +
                     __nvvm_read_ptx_sreg_tid_y();
    if (x >= width || row >= height)
        return;

    __global const uchar *y_sample = y_plane + (ulong)row * y_pitch + (ulong)x * bytes_per_sample;
    __global const uchar *uv_sample = uv_plane + (ulong)(row >> 1) * uv_pitch + (ulong)(x >> 1) * 2 * bytes_per_sample;

    float y, u, v;
    if (bytes_per_sample == 1)
    {
        y = y_sample[0];
        u = uv_sample[0];
        v = uv_sample[1];
    }
    else
    {
        y = *(__global const ushort *)y_sample;
        u = *(__global const ushort *)uv_sample;
        v = *(__global const ushort *)(uv_sample + 2);
    }

    const float luma = (y - y_offset) * y_scale;
    const float cb = (u - c_mid) * c_scale;
    const float cr = (v - c_mid) * c_scale;

    float rgb[3];
    rgb[0] = luma + rv * cr;
    rgb[1] = luma - gu * cb - gv * cr;
    rgb[2] = luma + bu * cb;

    __global uchar *out = dst + ((ulong)row * width + x) * 3;
    for (int i = 0; i < 3; i++)
    {
        const float c = rgb[i] < 0.0f ? 0.0f : (rgb[i] > 1.0f ? 1.0f : rgb[i]);
        out[i] = (uchar)(c * 255.0f + 0.5f);
    }
}
