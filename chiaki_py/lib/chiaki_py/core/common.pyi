"""The common submodule."""

import enum

class Target(enum.IntEnum):
    PS4_UNKNOWN = 0
    PS4_8 = 800
    PS4_9 = 900
    PS4_10 = 1000
    PS5_UNKNOWN = 1000000
    PS5_1 = 1000100

class Codec(enum.IntEnum):
    H264 = 0
    H265 = 1
    H265_HDR = 2
