#include "core/bitstream.h"

namespace py = pybind11;

void init_core_bitstream(py::module &m)
{
    py::enum_<ChiakiBitstreamSliceType>(m, "SliceType")
        .value("UNKNOWN", CHIAKI_BITSTREAM_SLICE_UNKNOWN)
        .value("I", CHIAKI_BITSTREAM_SLICE_I)
        .value("P", CHIAKI_BITSTREAM_SLICE_P);

    py::class_<BitstreamSliceWrapper>(m, "BitstreamSlice")
        .def(py::init<ChiakiBitstreamSliceType, unsigned>(),
             py::arg("slice_type"),
             py::arg("reference_frame"))
        .def_property("slice_type", &BitstreamSliceWrapper::get_slice_type, &BitstreamSliceWrapper::set_slice_type, "The slice type.")
        .def_property("reference_frame", &BitstreamSliceWrapper::get_reference_frame, &BitstreamSliceWrapper::set_reference_frame, "The reference frame.");

    py::class_<BitstreamWrapper>(m, "Bitstream")
        .def(py::init<LogWrapper &, ChiakiCodec>(),
             py::arg("log"),
             py::arg("codec"))
        .def_property("log", &BitstreamWrapper::get_log, &BitstreamWrapper::set_log, "The log.")
        .def_property("reference_frame", &BitstreamWrapper::get_codec, &BitstreamWrapper::set_codec, "The reference frame.")
        .def_property("slice_type", &BitstreamWrapper::get_log2_max_frame_num_minus4, &BitstreamWrapper::set_log2_max_frame_num_minus4, "The slice type.")
        .def_property("reference_frame", &BitstreamWrapper::get_log2_max_pic_order_cnt_lsb_minus4, &BitstreamWrapper::set_log2_max_pic_order_cnt_lsb_minus4, "The reference frame.")
        .def("header", &BitstreamWrapper::header, py::arg("data"), "Parses the header from the given data.")
        .def("slice", &BitstreamWrapper::slice, py::arg("data"), py::arg("slice"), "Parses the slice from the given data.")
        .def("slice_set_reference_frame", &BitstreamWrapper::slice_set_reference_frame, py::arg("data"), py::arg("reference_frame"), "Sets the reference frame.");
}