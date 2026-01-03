#!/bin/bash
# xhisper_rightctrl_daemon.sh
# Listens for right-ctrl key events and triggers xhisper
# Requires: evtest or access to /dev/input/event*

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
XHISPER="$SCRIPT_DIR/xhisper.sh"

# Ensure single instance
LOCK_FILE="/tmp/xhisper_rightctrl_daemon.lock"
exec 9>"$LOCK_FILE"
if ! flock -n 9; then
    echo "xhisper right-ctrl daemon already running. Exiting." >&2
    exit 0
fi

# If xhisper.sh doesn't exist, try PATH
if [ ! -f "$XHISPER" ]; then
    XHISPER=$(command -v xhisper)
fi

if [ ! -x "$XHISPER" ]; then
    echo "Error: xhisper not found" >&2
    exit 1
fi

export XHISPER_STATUS_MESSAGES="${XHISPER_STATUS_MESSAGES:-0}"

IS_RECORDING=0
RECORD_FLAG="/tmp/xhisper_rightctrl_recording"
PROCESS_FLAG="/tmp/xhisper_rightctrl_processing"
STOP_LOCK_DIR="/tmp/xhisper_rightctrl_stop.lock"
STATUS_FILE="/tmp/xhisper_status"
RECORDING_FILE="/tmp/xhisper.wav"
RECORD_PID_FILE="/tmp/xhisper_recording.pid"
RECORD_START_FILE="/tmp/xhisper_rightctrl_recording_start"
WATCHDOG_PID_FILE="/tmp/xhisper_rightctrl_watchdog.pid"
LAST_EVENT_FILE="/tmp/xhisper_rightctrl_last_event"
NOTIFY_LAST_FILE="/tmp/xhisper_rightctrl_notify_last"
BLOCK_UNTIL_RELEASE_FILE="/tmp/xhisper_rightctrl_block_until_release"
BLOCK_START_FILE="/tmp/xhisper_rightctrl_block_start"
RELEASE_POLL_MS=50
RELEASE_ZERO_COUNT=6
PROCESSING_TIMEOUT="${XHISPER_PROCESSING_TIMEOUT:-180}"
RELEASE_QUERY_GRACE_MS="${XHISPER_RELEASE_QUERY_GRACE_MS:-1200}"
RECORD_MAX_SECONDS="${XHISPER_RECORD_MAX_SECONDS:-3600}"
RECORD_STUCK_TIMEOUT="${XHISPER_RECORD_STUCK_TIMEOUT:-12}"
RECORD_START_GRACE="${XHISPER_RECORD_START_GRACE:-2}"
NOTIFY_COOLDOWN_MS="${XHISPER_NOTIFY_COOLDOWN_MS:-2000}"
BLOCK_COOLDOWN_MS="${XHISPER_BLOCK_COOLDOWN_MS:-500}"

echo "xhisper right-ctrl daemon starting..." >&2
echo "Hold right-ctrl to record" >&2
echo "Release right-ctrl to stop and transcribe" >&2

set_status() {
    printf "%s" "$1" > "$STATUS_FILE"
}

notify_user() {
    local msg="$1"
    local now last delta
    now=$(date +%s%3N)
    last=$(cat "$NOTIFY_LAST_FILE" 2>/dev/null || echo 0)
    delta=$((now - last))
    if [ "$delta" -lt "$NOTIFY_COOLDOWN_MS" ]; then
        return
    fi
    echo "$now" > "$NOTIFY_LAST_FILE" 2>/dev/null || true
    if command -v notify-send &>/dev/null; then
        notify-send -u low -t 2000 "xhisper" "$msg" >/dev/null 2>&1 || true
    fi
    echo "$msg" >&2
}

cleanup_flags() {
    stop_watchdog
    rm -f "$RECORD_FLAG" "$PROCESS_FLAG" "$LAST_EVENT_FILE" "$RECORD_START_FILE" "$WATCHDOG_PID_FILE" "$RECORD_PID_FILE" "$NOTIFY_LAST_FILE" "$BLOCK_UNTIL_RELEASE_FILE" "$BLOCK_START_FILE"
    pkill -f "pw-record.*$RECORDING_FILE" 2>/dev/null || true
    rmdir "$STOP_LOCK_DIR" 2>/dev/null || true
}

trap 'set_status "idle"; cleanup_flags' EXIT

# Find a readable keyboard event device.
# Preference order:
# 1) Name contains "keyboard" and handlers include kbd
# 2) Any handler with kbd
# 3) Any readable event device
find_keyboard_device() {
    local device
    local candidates

    candidates=$(awk '
        BEGIN {name=""; handlers=""}
        /^N: Name=/ {name=$0}
        /^H: Handlers=/ {handlers=$0}
        /^$/ {
            if (handlers ~ /kbd/) {
                gsub(/^N: Name=/, "", name)
                gsub(/^H: Handlers=/, "", handlers)
                print name "\t" handlers
            }
            name=""; handlers=""
        }
        END {
            if (handlers ~ /kbd/) {
                gsub(/^N: Name=/, "", name)
                gsub(/^H: Handlers=/, "", handlers)
                print name "\t" handlers
            }
        }
    ' /proc/bus/input/devices)

    device=$(echo "$candidates" | awk -F'\t' '
        BEGIN {IGNORECASE=1}
        $1 ~ /keyboard/ && $1 !~ /(button|power|sleep|consumer|system)/ {
            if (match($2, /event[0-9]+/)) { print substr($2, RSTART, RLENGTH); exit }
        }
    ')

    if [ -n "$device" ]; then
        echo "/dev/input/$device"
        return 0
    fi

    device=$(echo "$candidates" | awk -F'\t' '
        BEGIN {IGNORECASE=1}
        $1 !~ /(button|power|sleep|consumer|system)/ {
            if (match($2, /event[0-9]+/)) { print substr($2, RSTART, RLENGTH); exit }
        }
    ')

    if [ -n "$device" ]; then
        echo "/dev/input/$device"
        return 0
    fi

    for device in /dev/input/event*; do
        [ -r "$device" ] || continue
        echo "$device"
        return 0
    done

    return 1
}

KEYBOARD_DEVICE="${XHISPER_KEYBOARD_DEVICE:-}"
if [ -z "$KEYBOARD_DEVICE" ]; then
    KEYBOARD_DEVICE=$(find_keyboard_device || true)
fi

if [ -z "$KEYBOARD_DEVICE" ]; then
    echo "Error: no readable /dev/input/event* device found (need input group or sudo)" >&2
    exit 1
fi

echo "Using keyboard device: $KEYBOARD_DEVICE" >&2

# Use evtest to monitor right-ctrl (KEY_RIGHTCTRL = 97)
# This requires root or input group access
if ! command -v evtest &>/dev/null; then
    echo "Error: evtest not found. Install it or use the Python version." >&2
    exit 1
fi

# Start status UI if available and not already running
start_status_ui() {
    if [ "${XHISPER_STATUS_UI:-1}" != "1" ] || ! command -v python3 &>/dev/null; then
        return
    fi
    if [ ! -f "$SCRIPT_DIR/xhisper_status_ui.py" ] || pgrep -f "xhisper_status_ui.py" >/dev/null; then
        return
    fi

    local hypr_sig=""
    local runtime_dir="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"
    if [ -d "$runtime_dir/hypr" ]; then
        hypr_sig=$(ls -1 "$runtime_dir/hypr" 2>/dev/null | head -n 1)
    fi

    if [ -x /usr/bin/hyprctl ] && [ -n "$hypr_sig" ]; then
        export HYPRLAND_INSTANCE_SIGNATURE="$hypr_sig"
        /usr/bin/hyprctl dispatch exec "python3 $SCRIPT_DIR/xhisper_status_ui.py" >/tmp/xhisper_status_ui.log 2>&1
    else
        python3 "$SCRIPT_DIR/xhisper_status_ui.py" >/tmp/xhisper_status_ui.log 2>&1 &
    fi
}

start_status_ui

cleanup_flags
set_status "idle"

reset_stale_recording() {
    if [ -f "$RECORD_FLAG" ]; then
        return
    fi
    if pgrep -f "pw-record.*$RECORDING_FILE" >/dev/null; then
        pkill -f "pw-record.*$RECORDING_FILE" 2>/dev/null || true
    fi
    rm -f "$RECORD_PID_FILE"
}

reset_stale_recording

note_event() {
    date +%s%3N > "$LAST_EVENT_FILE" 2>/dev/null || true
}

recent_event_seen() {
    if [ ! -f "$LAST_EVENT_FILE" ]; then
        return 1
    fi
    local now last delta
    now=$(date +%s%3N)
    last=$(cat "$LAST_EVENT_FILE" 2>/dev/null || echo 0)
    delta=$((now - last))
    [ "$delta" -lt "$RELEASE_QUERY_GRACE_MS" ]
}

acquire_stop_lock() {
    mkdir "$STOP_LOCK_DIR" 2>/dev/null
}

release_stop_lock() {
    rmdir "$STOP_LOCK_DIR" 2>/dev/null || true
}

stop_recording() {
    if [ ! -f "$RECORD_FLAG" ]; then
        return
    fi
    if [ -f "$PROCESS_FLAG" ]; then
        return
    fi
    if ! acquire_stop_lock; then
        return
    fi
    rm -f "$RECORD_FLAG"
    touch "$PROCESS_FLAG"
    touch "$BLOCK_UNTIL_RELEASE_FILE"
    date +%s%3N > "$BLOCK_START_FILE"
    set_status "processing"
    run_transcribe &
}

# Stop without transcribing (used when recording never started).
abort_recording() {
    rm -f "$RECORD_FLAG" "$PROCESS_FLAG" "$RECORD_START_FILE"
    touch "$BLOCK_UNTIL_RELEASE_FILE"
    date +%s%3N > "$BLOCK_START_FILE"
    set_status "idle"
    stop_watchdog
    pkill -f "pw-record.*$RECORDING_FILE" 2>/dev/null || true
    clear_block_if_released
}

# Watchdog to prevent stuck recordings if release is missed.
watchdog_running() {
    if [ -f "$WATCHDOG_PID_FILE" ]; then
        local pid
        pid=$(cat "$WATCHDOG_PID_FILE" 2>/dev/null || echo "")
        if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
            return 0
        fi
        rm -f "$WATCHDOG_PID_FILE"
    fi
    return 1
}

stop_watchdog() {
    if watchdog_running; then
        local pid
        pid=$(cat "$WATCHDOG_PID_FILE" 2>/dev/null || echo "")
        if [ -n "$pid" ] && [ "$pid" -ne "$$" ]; then
            kill "$pid" 2>/dev/null || true
        fi
        rm -f "$WATCHDOG_PID_FILE"
    fi
}

recording_watchdog() {
    while [ -f "$RECORD_FLAG" ]; do
        local now_s now_ms start_s last_ms delta_ms state
        now_s=$(date +%s)
        now_ms=$(date +%s%3N)

        if [ -f "$RECORD_START_FILE" ] && [ "$RECORD_MAX_SECONDS" -gt 0 ]; then
            start_s=$(cat "$RECORD_START_FILE" 2>/dev/null || echo "$now_s")
            if [ "$((now_s - start_s))" -ge "$RECORD_MAX_SECONDS" ]; then
                echo "Recording max duration reached. Stopping..." >&2
                stop_recording
                break
            fi
        fi

        if [ -f "$LAST_EVENT_FILE" ]; then
            last_ms=$(cat "$LAST_EVENT_FILE" 2>/dev/null || echo "$now_ms")
            delta_ms=$((now_ms - last_ms))
            if [ "$delta_ms" -ge "$((RECORD_STUCK_TIMEOUT * 1000))" ]; then
                state=$(evtest --query "$KEYBOARD_DEVICE" EV_KEY KEY_RIGHTCTRL 2>/dev/null | tr -d '\r\n')
                if [ "$state" = "0" ]; then
                    echo "Right-ctrl state 0 (watchdog). Stopping..." >&2
                    stop_recording
                    break
                fi
            fi
        fi

        if ! pgrep -f "pw-record.*$RECORDING_FILE" >/dev/null; then
            if [ ! -f "$PROCESS_FLAG" ]; then
                start_s=$(cat "$RECORD_START_FILE" 2>/dev/null || echo "$now_s")
                if [ "$((now_s - start_s))" -ge "$RECORD_START_GRACE" ]; then
                    echo "Recording process missing. Aborting..." >&2
                    abort_recording
                    break
                fi
            fi
        fi

        sleep 1
    done
}

start_watchdog() {
    if watchdog_running; then
        return
    fi
    recording_watchdog &
    echo $! > "$WATCHDOG_PID_FILE"
}

key_state() {
    evtest --query "$KEYBOARD_DEVICE" EV_KEY KEY_RIGHTCTRL 2>/dev/null | tr -d '\r\n'
}

clear_block_if_released() {
    local state
    state=$(key_state)
    if [ "$state" = "0" ]; then
        rm -f "$BLOCK_UNTIL_RELEASE_FILE" "$BLOCK_START_FILE"
    fi
}

run_transcribe() {
    local timeout_status
    if command -v timeout &>/dev/null; then
        timeout "$PROCESSING_TIMEOUT" "$XHISPER"
        timeout_status=$?
        if [ "$timeout_status" -eq 124 ] || [ "$timeout_status" -eq 137 ]; then
            pkill -f "pw-record.*$RECORDING_FILE" 2>/dev/null || true
        fi
    else
        "$XHISPER"
    fi
    rm -f "$PROCESS_FLAG"
    set_status "idle"
    release_stop_lock
    rm -f "$RECORD_START_FILE"
    stop_watchdog
    clear_block_if_released
}

# Fallback release detector using key state queries.
wait_for_release() {
    local zero_count=0
    while [ -f "$RECORD_FLAG" ]; do
        if recent_event_seen; then
            zero_count=0
            sleep "$(printf "0.%03d" "$RELEASE_POLL_MS")"
            continue
        fi
        state=$(evtest --query "$KEYBOARD_DEVICE" EV_KEY KEY_RIGHTCTRL 2>/dev/null | tr -d '\r\n')
        if [ "$state" = "0" ]; then
            zero_count=$((zero_count + 1))
        else
            zero_count=0
        fi

        if [ "$zero_count" -ge "$RELEASE_ZERO_COUNT" ]; then
            echo "Right-ctrl released (query). Stopping..." >&2
            stop_recording
            break
        fi
        sleep "$(printf "0.%03d" "$RELEASE_POLL_MS")"
    done
}

# Stream events from evtest (line-buffered) and parse directly
stdbuf -oL -eL evtest "$KEYBOARD_DEVICE" 2>&1 \
  | while IFS= read -r line; do
    case "$line" in
        *"code 97"*|*"KEY_RIGHTCTRL"*)
            ;;
        *)
            continue
            ;;
    esac

    if echo "$line" | grep -q "value 1"; then  # Key pressed
        if [ -f "$PROCESS_FLAG" ]; then
            notify_user "Currently transcribing. Wait..."
            touch "$BLOCK_UNTIL_RELEASE_FILE"
            date +%s%3N > "$BLOCK_START_FILE"
            continue
        fi
        if [ -f "$BLOCK_UNTIL_RELEASE_FILE" ]; then
            notify_user "Release right-ctrl first..."
            continue
        fi
        if [ ! -f "$RECORD_FLAG" ]; then
            echo "Right-ctrl pressed. Starting xhisper..." >&2
            touch "$RECORD_FLAG"
            date +%s > "$RECORD_START_FILE"
            IS_RECORDING=1
            set_status "listening"
            note_event
            reset_stale_recording
            # Start recording without blocking event loop
            "$XHISPER" >/dev/null 2>&1 &
            wait_for_release &
            start_watchdog
            # Confirm recorder starts; otherwise abort without processing.
            for _ in $(seq 1 10); do
                if pgrep -f "pw-record.*$RECORDING_FILE" >/dev/null; then
                    break
                fi
                sleep 0.1
            done
            if ! pgrep -f "pw-record.*$RECORDING_FILE" >/dev/null; then
                echo "Recording failed to start. Aborting..." >&2
                abort_recording
            fi
        fi
    elif echo "$line" | grep -q "value 2"; then  # Key repeat
        if [ -f "$RECORD_FLAG" ]; then
            note_event
        fi
    elif echo "$line" | grep -q "value 0"; then  # Key released
        if [ -f "$BLOCK_UNTIL_RELEASE_FILE" ]; then
            if [ -f "$PROCESS_FLAG" ]; then
                continue
            fi
            now=$(date +%s%3N)
            start=$(cat "$BLOCK_START_FILE" 2>/dev/null || echo 0)
            if [ $((now - start)) -lt "$BLOCK_COOLDOWN_MS" ]; then
                continue
            fi
            rm -f "$BLOCK_UNTIL_RELEASE_FILE" "$BLOCK_START_FILE"
            continue
        fi
        if [ -f "$RECORD_FLAG" ]; then
            echo "Right-ctrl released. Stopping..." >&2
            IS_RECORDING=0
            note_event
            stop_recording
        fi
    fi
done
