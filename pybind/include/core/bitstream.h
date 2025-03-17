#ifndef CHIAKI_PY_CORE_BITSTREAM_H
#define CHIAKI_PY_CORE_BITSTREAM_H

#include "core/struct_wrapper.h"
#include "core/common.h"
#include "core/log.h"

#include <chiaki/bitstream.h>

#include <vector>

#include <pybind11/pybind11.h>

namespace py = pybind11;

void init_core_bitstream(py::module &m);

class BitstreamSliceWrapper : public StructWrapper<ChiakiBitstreamSlice>
{
public:
    using StructWrapper::StructWrapper;

    BitstreamSliceWrapper(ChiakiBitstreamSliceType slice_type, unsigned reference_frame) : StructWrapper()
    {
        raw().slice_type = slice_type;
        raw().reference_frame = reference_frame;
    }

    ChiakiBitstreamSliceType get_slice_type() { return raw().slice_type; }
    void set_slice_type(ChiakiBitstreamSliceType slice_type) { raw().slice_type = slice_type; }

    unsigned get_reference_frame() { return raw().reference_frame; }
    void set_reference_frame(unsigned reference_frame) { raw().reference_frame = reference_frame; }
};

class BitstreamWrapper : public StructWrapper<ChiakiBitstream>
{
private:
    LogWrapper log;

public:
    using StructWrapper::StructWrapper;

    BitstreamWrapper(LogWrapper &log, ChiakiCodec codec) : StructWrapper(), log(log)
    {
        chiaki_bitstream_init(ptr(), log.ptr(), codec);
    }

    LogWrapper get_log() { return log; }
    void set_log(LogWrapper &log) { this->log = log; raw().log = log.ptr(); }

    ChiakiCodec get_codec() { return raw().codec; }
    void set_codec(ChiakiCodec codec) { raw().codec = codec; }

    uint32_t get_log2_max_frame_num_minus4() { return raw().h264.sps.log2_max_frame_num_minus4; }
    void set_log2_max_frame_num_minus4(uint32_t log2_max_frame_num_minus4) { raw().h264.sps.log2_max_frame_num_minus4 = log2_max_frame_num_minus4; }

    uint32_t get_log2_max_pic_order_cnt_lsb_minus4() { return raw().h265.sps.log2_max_pic_order_cnt_lsb_minus4; }
    void set_log2_max_pic_order_cnt_lsb_minus4(uint32_t log2_max_pic_order_cnt_lsb_minus4) { raw().h265.sps.log2_max_pic_order_cnt_lsb_minus4 = log2_max_pic_order_cnt_lsb_minus4; }

    bool header(std::vector<uint8_t> &data)
    {
        return chiaki_bitstream_header(ptr(), data.data(), data.size());
    }

    bool slice(std::vector<uint8_t> &data, BitstreamSliceWrapper &slice)
    {
        return chiaki_bitstream_slice(ptr(), data.data(), data.size(), slice.ptr());
    }

    bool slice_set_reference_frame(std::vector<uint8_t> &data, unsigned reference_frame)
    {
        return chiaki_bitstream_slice_set_reference_frame(ptr(), data.data(), data.size(), reference_frame);
    }
};

#endif // CHIAKI_PY_CORE_BITSTREAM_H