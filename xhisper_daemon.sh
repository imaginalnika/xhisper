#!/bin/bash
# xhisper_daemon.sh - Standalone right-ctrl listener and xhisper trigger
# Requires: evtest for keyboard event monitoring

XHISPER_CMD="xhisper"
IS_RECORDING=0
DOUBLE_CLICK_TIMEOUT=0.5

# Find keyboard event device
find_keyboard_device() {
    # Look for input device with keyboard capability
    for device in /dev/input/event*; do
        if [ -r "$device" ] 2>/dev/null; then
            # Try to get device name
            if evtest --info "$device" 2>/dev/null | grep -qi "keyboard\|input"; then
                echo "$device"
                return 0
            fi
        fi
    done
    # Fallback to first accessible event device
    for device in /dev/input/event*; do
        if [ -r "$device" ] 2>/dev/null; then
            echo "$device"
            return 0
        fi
    done
}

# Try using evtest with timeout handling
run_with_evtest() {
    local device="$1"
    
    # Monitor keyboard with evtest
    evtest "$device" 2>/dev/null | while IFS= read -r line; do
        # Look for KEY_RIGHTCTRL (code 97) with value 1 (press)
        if echo "$line" | grep -qE "code 97|KEY_RIGHTCTRL" && echo "$line" | grep -q "value 1"; then
            if [ $IS_RECORDING -eq 1 ]; then
                # Stop recording
                IS_RECORDING=0
                $XHISPER_CMD &
            else
                # Start recording
                IS_RECORDING=1
                $XHISPER_CMD &
            fi
        fi
    done
}

main() {
    echo "xhisper daemon: starting right-ctrl listener..." >&2
    
    if ! command -v evtest &>/dev/null; then
        echo "Error: evtest not found. Install it with: sudo apt install input-utils" >&2
        echo "Falling back to manual trigger mode." >&2
        echo "Run 'xhisper' manually to start/stop recording." >&2
        exit 1
    fi
    
    KEYBOARD_DEVICE=$(find_keyboard_device)
    
    if [ -z "$KEYBOARD_DEVICE" ]; then
        echo "Error: Could not find keyboard event device" >&2
        exit 1
    fi
    
    echo "Using keyboard device: $KEYBOARD_DEVICE" >&2
    echo "Double-click right-ctrl to start, single press to stop" >&2
    echo "Press Ctrl+C to exit" >&2
    
    run_with_evtest "$KEYBOARD_DEVICE"
}

main "$@"
