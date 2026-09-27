"""The log submodule."""

import enum

class LogLevel(enum.IntEnum):
    DEBUG = 16
    VERBOSE = 8
    INFO = 4
    WARNING = 2
    ERROR = 1
