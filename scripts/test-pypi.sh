#!/usr/bin/env bash
# Installs chiaki-py from TestPyPI for several Python versions and checks that
# the stream window of an example opens (Ubuntu and macOS).
#
# Ubuntu: needs xdotool (sudo apt install xdotool). On Wayland, Qt is forced to
#         XWayland (QT_QPA_PLATFORM=xcb) so xdotool can see the window.
#         Without a display (CI/SSH), run it under xvfb-run -a.
# macOS:  no extra tools needed (uses CoreGraphics through osascript).
#
# Usage: scripts/test-pypi.sh [example.py]

set -u

PACKAGE_VERSION="0.3.0"
PYTHON_VERSIONS=("3.11" "3.12" "3.13" "3.14")
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
EXPECTED_TITLE="Live Image Stream"   # set in chiaki_py/gui/stream/views/base_view.py
TIMEOUT_SECONDS=60                   # max time until the window must appear
STABLE_SECONDS=5                     # window must stay alive this long
LOG_DIR="$REPO_ROOT/cache/test-pypi-logs"
mkdir -p "$LOG_DIR"

OS="$(uname -s)"
case "$OS" in
    Linux)  DEFAULT_EXAMPLE="1.4.2_stream_cuda_qt.py" ;;
    Darwin) DEFAULT_EXAMPLE="1.4.1_stream_cpu_qt.py" ;;  # no CUDA on macOS
    *)      echo "Unsupported OS: $OS"; exit 1 ;;
esac
EXAMPLE="$REPO_ROOT/examples/${1:-$DEFAULT_EXAMPLE}"

if [[ "$OS" == "Linux" ]]; then
    if ! command -v xdotool >/dev/null; then
        echo "xdotool not found: sudo apt install xdotool"
        exit 1
    fi
    export QT_QPA_PLATFORM=xcb
fi

eval "$(conda shell.bash hook)"

# Prints nothing and returns 0 if a visible window of the process exists.
window_exists() {
    local pid="$1"
    if [[ "$OS" == "Linux" ]]; then
        xdotool search --onlyvisible --pid "$pid" --name "$EXPECTED_TITLE" >/dev/null 2>&1
    else
        # Window titles need the Screen Recording permission, so match on the owner PID
        # and a normal window layer (0) instead.
        local count
        count="$(osascript -l JavaScript -e "
            ObjC.import('CoreGraphics');
            var list = ObjC.deepUnwrap(ObjC.castRefToObject(
                \$.CGWindowListCopyWindowInfo(\$.kCGWindowListOptionOnScreenOnly, \$.kCGNullWindowID)));
            list.filter(w => w.kCGWindowOwnerPID === $pid && w.kCGWindowLayer === 0).length;
        " 2>/dev/null)"
        [[ "${count:-0}" -gt 0 ]]
    fi
}

stop_process() {
    local pid="$1"
    kill -TERM "$pid" 2>/dev/null || return 0
    for _ in {1..10}; do
        kill -0 "$pid" 2>/dev/null || return 0
        sleep 0.5
    done
    kill -KILL "$pid" 2>/dev/null
}

# Prints an error message on failure, nothing on success.
test_window_opens() {
    local python="$1" log_prefix="$2"

    (cd "$REPO_ROOT" && exec "$python" "$EXAMPLE") >"$log_prefix.out.log" 2>"$log_prefix.err.log" &
    local pid=$!

    local deadline=$((SECONDS + TIMEOUT_SECONDS)) found=0
    while ((SECONDS < deadline)); do
        if ! kill -0 "$pid" 2>/dev/null; then
            wait "$pid"
            echo "process exited early with code $?"
            return
        fi
        if window_exists "$pid"; then
            found=1
            break
        fi
        sleep 0.5
    done
    if ((found == 0)); then
        stop_process "$pid"
        echo "no window within ${TIMEOUT_SECONDS} s"
        return
    fi

    # Make sure it doesn't crash right after opening
    sleep "$STABLE_SECONDS"
    if ! kill -0 "$pid" 2>/dev/null; then
        wait "$pid"
        echo "window opened but process exited with code $?"
        return
    fi
    if ! window_exists "$pid"; then
        stop_process "$pid"
        echo "window disappeared"
        return
    fi

    stop_process "$pid"
    wait "$pid" 2>/dev/null
}

declare -a RESULTS=()
for py in "${PYTHON_VERSIONS[@]}"; do
    echo -e "\033[36m=== Python $py ===\033[0m"
    conda activate base
    conda create -n chiaki-py-test python="$py" -y
    conda activate chiaki-py-test
    if ! pip install --extra-index-url https://test.pypi.org/simple/ "chiaki-py==$PACKAGE_VERSION"; then
        RESULTS+=("$py|pip install failed")
        continue
    fi

    err="$(test_window_opens "$(command -v python)" "$LOG_DIR/py$py")"
    RESULTS+=("$py|${err:-OK}")
done
conda activate base

echo -e "\n\033[36m=== Summary ($OS, $(basename "$EXAMPLE")) ===\033[0m"
failed=0
for entry in "${RESULTS[@]}"; do
    py="${entry%%|*}"
    result="${entry#*|}"
    if [[ "$result" == "OK" ]]; then
        echo -e "\033[32mPython $py: $result\033[0m"
    else
        echo -e "\033[31mPython $py: $result\033[0m"
        failed=1
    fi
done
echo "Logs: $LOG_DIR"
exit $failed
