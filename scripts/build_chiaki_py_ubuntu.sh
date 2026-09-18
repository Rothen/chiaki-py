#!/usr/bin/env bash
# Superseded by build_chiaki_ubuntu.sh, which now builds the actual
# chiaki-py target end to end (this file previously ran `brew`/clang
# commands under an "ubuntu" name - leftover from an earlier, pre-
# FetchContent build layout - and never worked on Ubuntu).
set -euo pipefail
exec "$(dirname "${BASH_SOURCE[0]}")/build_chiaki_ubuntu.sh" "$@"
