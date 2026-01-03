#!/usr/bin/env python3
"""
xhisper_autostart.py - Auto-trigger xhisper on right-ctrl double-click
Simple, no dependencies beyond pynput (or fallback to keyboard monitoring)
"""

import subprocess
import sys
import time
from datetime import datetime

# Try to import keyboard library (simpler than pynput)
try:
    import keyboard
    HAS_KEYBOARD = True
except ImportError:
    HAS_KEYBOARD = False
    print("Installing keyboard library...", file=sys.stderr)
    try:
        subprocess.run([sys.executable, "-m", "pip", "install", "-q", "keyboard"], check=True)
        import keyboard
        HAS_KEYBOARD = True
    except Exception as e:
        print(f"Error installing keyboard: {e}", file=sys.stderr)
        print("Falling back to manual mode. Run 'xhisper' twice manually.", file=sys.stderr)
        sys.exit(1)

is_recording = False
last_rightctrl = 0
double_click_window = 0.4  # seconds

def on_press(event):
    global is_recording, last_rightctrl
    
    # Check if right-ctrl was pressed
    if event.name == 'right ctrl' or event.name == 'rctrl':
        now = time.time()
        time_since_last = now - last_rightctrl
        
        if is_recording:
            # Already recording: stop and transcribe
            print(f"[{datetime.now().strftime('%H:%M:%S')}] Right-ctrl pressed. Stopping xhisper...", file=sys.stderr)
            is_recording = False
            try:
                subprocess.Popen(['xhisper'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            except Exception as e:
                print(f"Error running xhisper: {e}", file=sys.stderr)
        elif time_since_last < double_click_window:
            # Double-click detected: start recording
            print(f"[{datetime.now().strftime('%H:%M:%S')}] Right-ctrl double-click detected. Starting xhisper...", file=sys.stderr)
            is_recording = True
            try:
                subprocess.Popen(['xhisper'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            except Exception as e:
                print(f"Error running xhisper: {e}", file=sys.stderr)
        
        last_rightctrl = now

def main():
    print("=== xhisper Auto-Trigger Daemon ===", file=sys.stderr)
    print("Double-click right-ctrl to START recording", file=sys.stderr)
    print("Single right-ctrl press (while recording) to STOP and transcribe", file=sys.stderr)
    print("Press Ctrl+C to exit", file=sys.stderr)
    print("", file=sys.stderr)
    
    try:
        # Listen to keyboard events
        keyboard.on_press(on_press)
        print("Listening for right-ctrl...", file=sys.stderr)
        
        # Keep running
        keyboard.wait()
    except KeyboardInterrupt:
        print("\nExiting...", file=sys.stderr)
        sys.exit(0)
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()
