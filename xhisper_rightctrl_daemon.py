#!/usr/bin/env python3
"""
xhisper_rightctrl_daemon.py
Listens for right-ctrl to toggle xhisper recording.
Single right-ctrl press = start recording
Single right-ctrl press while recording = stop and transcribe
"""

import subprocess
import sys
import time
from pynput import keyboard
from pathlib import Path

# State tracking
is_recording = False
last_rightctrl_press = 0
debounce_window = 0.2  # ignore repeats within 200ms
script_dir = Path(__file__).parent.resolve()
xhisper_script = script_dir / "xhisper.sh"

def get_xhisper_path():
    """Find xhisper script or binary"""
    if xhisper_script.exists():
        return str(xhisper_script)
    # Try PATH
    try:
        result = subprocess.run(['which', 'xhisper'], capture_output=True, text=True)
        if result.returncode == 0:
            return result.stdout.strip()
    except:
        pass
    return None

def run_xhisper():
    """Trigger xhisper (start recording or stop/transcribe)"""
    xhisper_path = get_xhisper_path()
    if not xhisper_path:
        print("Error: xhisper script not found", file=sys.stderr)
        return False
    
    try:
        subprocess.Popen([xhisper_path], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        return True
    except Exception as e:
        print(f"Error running xhisper: {e}", file=sys.stderr)
        return False

def on_press(key):
    """Handle key press events"""
    global is_recording, last_rightctrl_press
    
    try:
        # Check if right-ctrl was pressed
        if key == keyboard.Key.ctrl_r:
            current_time = time.time()
            time_since_last = current_time - last_rightctrl_press
            if time_since_last < debounce_window:
                return

            if is_recording:
                print("Right-ctrl pressed while recording. Stopping...", file=sys.stderr)
                is_recording = False
            else:
                print("Right-ctrl pressed. Starting xhisper...", file=sys.stderr)
                is_recording = True

            run_xhisper()
            last_rightctrl_press = current_time
    except AttributeError:
        pass

def main():
    """Main daemon loop"""
    print("xhisper right-ctrl daemon started", file=sys.stderr)
    print("Press right-ctrl to start recording", file=sys.stderr)
    print("Press right-ctrl again while recording to stop and transcribe", file=sys.stderr)
    
    try:
        with keyboard.Listener(on_press=on_press) as listener:
            listener.join()
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()
