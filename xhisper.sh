#!/bin/bash

# xhisper v1.0
# Dictate anywhere in Linux. Transcription at your cursor.
# - Transcription via Groq Whisper

# Configuration (see default_xhisperrc or ~/.config/xhisper/xhisperrc):
# - long-recording-threshold : threshold for using large vs turbo model (seconds)
# - transcription-prompt : context words for better Whisper accuracy
# - silence-threshold : max volume in dB to consider silent (e.g., -50)
# - silence-percentage : percentage of recording that must be silent (e.g., 95)
# - non-ascii-initial-delay : sleep after first non-ASCII paste (seconds)
# - non-ascii-default-delay : sleep after subsequent non-ASCII pastes (seconds)

# Requirements:
# - pipewire, pipewire-utils (audio)
# - wl-clipboard (Wayland) or xclip (X11) for clipboard
# - jq, curl, ffmpeg (processing)
# - make to build, sudo make install to install

[ -f "$HOME/.env" ] && source "$HOME/.env"

# Parse command-line arguments
LOCAL_MODE=0
WRAP_KEY=""
for arg in "$@"; do
  case "$arg" in
    --local)
      LOCAL_MODE=1
      ;;
    --log)
      if [ -f "/tmp/xhisper.log" ]; then
        cat /tmp/xhisper.log
      else
        echo "No log file found at /tmp/xhisper.log" >&2
      fi
      exit 0
      ;;
    --leftalt|--rightalt|--leftctrl|--rightctrl|--leftshift|--rightshift|--super)
      if [ -n "$WRAP_KEY" ]; then
        echo "Error: Multiple wrap keys not yet supported" >&2
        exit 1
      fi
      WRAP_KEY="${arg#--}"
      ;;
    *)
      echo "Error: Unknown option '$arg'" >&2
      echo "Usage: xhisper [--local] [--log] [--leftalt|--rightalt|--leftctrl|--rightctrl|--leftshift|--rightshift|--super]" >&2
      exit 1
      ;;
  esac
done

XDOTOOL='/usr/bin/xdotool'

# Set binary paths based on local mode
if [ "$LOCAL_MODE" -eq 1 ]; then
  SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  XHISPERTOOL="$SCRIPT_DIR/xhispertool"
  XHISPERTOOLD="$SCRIPT_DIR/xhispertoold"
else
  XHISPERTOOL="xhispertool"
  XHISPERTOOLD="xhispertoold"
fi

RECORDING="/tmp/xhisper.wav"
LOGFILE="/tmp/xhisper.log"
PROCESS_PATTERN="pw-record.*$RECORDING"

# Get GROQ_API_KEY from environment, or read it from ~/.groq-api-key if not set
: "${GROQ_API_KEY:=$(cat "$HOME/.groq-api-key" 2>/dev/null)}"

# Optional: Throw an error (also to logfile) if no GROQ key found
if [ -z "$GROQ_API_KEY" ]; then
    echo "Error: GROQ_API_KEY env var is not set and ~/.groq-api-key file is missing or empty." >> $LOGFILE
    echo "Error: GROQ_API_KEY env var is not set and ~/.groq-api-key file is missing or empty." >&2
    exit 1
fi

# Default configuration
long_recording_threshold=1000
transcription_prompt=""
silence_threshold=-50
silence_percentage=95
non_ascii_initial_delay=0.1
non_ascii_default_delay=0.025

CONFIG_FILE="${XDG_CONFIG_HOME:-$HOME/.config}/xhisper/xhisperrc"
# An executable script that can process text we get back from STT
CONFIG_PROCESSOR="${XDG_CONFIG_HOME:-$HOME/.config}/xhisper/processor"

if [ -f "$CONFIG_FILE" ]; then
  while IFS=: read -r key value || [ -n "$key" ]; do
    # Skip comments and empty lines
    [[ "$key" =~ ^[[:space:]]*# ]] && continue
    [[ -z "$key" ]] && continue

    # Trim whitespace and quotes
    key=$(echo "$key" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
    value=$(echo "$value" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//;s/^"//;s/"$//')

    case "$key" in
      long-recording-threshold) long_recording_threshold="$value" ;;
      transcription-prompt) transcription_prompt="$value" ;;
      silence-threshold) silence_threshold="$value" ;;
      silence-percentage) silence_percentage="$value" ;;
      non-ascii-initial-delay) non_ascii_initial_delay="$value" ;;
      non-ascii-default-delay) non_ascii_default_delay="$value" ;;
    esac
  done < "$CONFIG_FILE"
fi

# Auto-start daemon if not running
if ! pgrep -x xhispertoold > /dev/null; then
    "$XHISPERTOOLD" 2>> /tmp/xhispertoold.log &
    sleep 1  # Give daemon time to start

    # Verify daemon started successfully
    if ! pgrep -x xhispertoold > /dev/null; then
        echo "Error: Failed to start xhispertoold daemon" >&2
        echo "Check /tmp/xhispertoold.log for details" >&2
        exit 1
    fi
fi

# Check if xhispertool is available
if ! command -v "$XHISPERTOOL" &> /dev/null; then
    echo "Error: xhispertool not found" >&2
    echo "Please either:" >&2
    echo "  - Run 'sudo make install' to install system-wide" >&2
    echo "  - Run 'xhisper --local' from the build directory" >&2
    exit 1
fi

# Detect clipboard tool: use wl-copy only when running under Wayland, else xclip
if command -v wl-copy &> /dev/null && { [ "$XDG_SESSION_TYPE" = "wayland" ] || [ -n "$WAYLAND_DISPLAY" ]; }; then
    CLIP_TOOL="wl-copy"
elif command -v xclip &> /dev/null; then
    CLIP_TOOL="xclip"
else
    echo "Error: No working clipboard tool found. Install wl-clipboard or xclip." >&2
    exit 1
fi

# Copy text to the clipboard (or PRIMARY selection for xterm) using the detected tool
clip_copy() {
    local window_class="$1"
    shift
    local text="$*"

    if [ "$CLIP_TOOL" = "wl-copy" ]; then
        if [ "$window_class" = "XTerm" ]; then
            echo -n "$text" | wl-copy --primary
        else
            echo -n "$text" | wl-copy
        fi
    else
        if [ "$window_class" = "XTerm" ]; then
            # Send to PRIMARY selection for xterm
            echo -n "$text" | xclip -selection primary
        else
            # Default to standard CLIPBOARD selection for everything else
            echo -n "$text" | xclip -selection clipboard
        fi
    fi
}

press_wrap_key() {
  if [ -n "$WRAP_KEY" ]; then
    "$XHISPERTOOL" "$WRAP_KEY"
  fi
}

## This is faster than xhispertool and can actually paste newlines
paste() {
  # 1. get the text
  local text="$1"

  # Detect active window name/class
  local winClass=$(/usr/bin/xprop -id $($XDOTOOL getactivewindow) WM_CLASS | awk -F '"' '{print $4}')

  # 2. Push the text to the clipboard buffer and force xclip to stay active in the background
  # (We use -selection clipboard to target Ctrl+V channels)
  clip_copy $winClass $text

  # 3. CRUCIAL: Give the X11 server a tiny window to register xclip's data ownership
  if [[ "$text" != "(recording...)" ]]; then
    sleep 0.25
  fi

  # 4. Trigger your terminal's paste shortcut based on window type
  if [ "$winClass" = "XTerm" ]; then
    # If it's xterm, paste is Shift+Insert
    $XDOTOOL key shift+Insert
  elif echo "$winClass" | grep -iq "terminal"; then
    # Default behavior for other terminals is ctrl-shift-v
    $XDOTOOL key Ctrl+Shift+v
  else
    # Default behavior for other applications
    $XDOTOOL key ctrl+v
  fi

}

delete_n_chars() {
  local n="$1"
  for ((i=0; i<n; i++)); do
    "$XHISPERTOOL" backspace
  done
}

get_duration() {
  local recording="$1"
  ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 "$recording" 2>/dev/null || echo "0"
}

is_silent() {
  local recording="$1"

  # Use ffmpeg volumedetect to get mean and max volume
  local vol_stats=$(ffmpeg -i "$recording" -af "volumedetect" -f null /dev/null 2>&1 | grep -E "mean_volume|max_volume")
  local max_vol=$(echo "$vol_stats" | grep "max_volume" | awk '{print $5}')

  # If max volume is below threshold, consider it silent
  # Note: ffmpeg reports in dB, negative values (e.g., -50 dB is quiet)
  if [ -n "$max_vol" ]; then
    local is_quiet=$(echo "$max_vol < $silence_threshold" | bc -l)
    [ "$is_quiet" -eq 1 ] && return 0
  fi

  return 1
}

logging_end_and_write_to_logfile() {
  local title="$1"
  local result="$2"
  local logging_start="$3"

  local logging_end=$(date +%s%N)
  local time=$(echo "scale=3; ($logging_end - $logging_start) / 1000000000" | bc)

  echo "=== $title ===" >> "$LOGFILE"
  echo "Result: [$result]" >> "$LOGFILE"
  echo "Time: ${time}s" >> "$LOGFILE"
}

transcribe() {
  local recording="$1"
  local logging_start=$(date +%s%N)

  # Use large model for longer recordings, turbo for short ones
  local is_long_recording=$(echo "$(get_duration "$recording") > $long_recording_threshold" | bc -l)
  local model=$([[ $is_long_recording -eq 1 ]] && echo "whisper-large-v3" || echo "whisper-large-v3-turbo")

  local transcription=$(curl -s -X POST "https://api.groq.com/openai/v1/audio/transcriptions" \
    -H "Authorization: Bearer $GROQ_API_KEY" \
    -H "Content-Type: multipart/form-data" \
    -F "file=@$recording" \
    -F "model=$model" \
    -F "prompt=$transcription_prompt" \
    | jq -r '.text' | sed 's/^ //') # Transcription always returns a leading space, so remove it via sed

  logging_end_and_write_to_logfile "Transcription" "$transcription" "$logging_start"

  echo "$transcription"
}

# Main

# Find recording process, if so then kill
if pgrep -f "$PROCESS_PATTERN" > /dev/null; then
  pkill -f "$PROCESS_PATTERN"; sleep 0.2 # Buffer for flush
  delete_n_chars 14 # "(recording...)"

  # Check if recording is silent
  if is_silent "$RECORDING"; then
    paste "(no sound detected)"
    sleep 0.6
    delete_n_chars 19 # "(no sound detected)"
    rm -f "$RECORDING"
    exit 0
  fi

  paste "(transcribing...)"

	# If we have a 'processor' available then pipe it through that
  if [[ -x $CONFIG_PROCESSOR ]]; then
		TRANSCRIPTION=$(transcribe "$RECORDING" | $CONFIG_PROCESSOR)
  else
    TRANSCRIPTION=$(transcribe "$RECORDING")
  fi

  delete_n_chars 17 # "(transcribing...)"

  paste "$TRANSCRIPTION"

  rm -f "$RECORDING"
else
  # No recording running, so start
  sleep 0.2
  paste "(recording...)"
  pw-record --channels=1 --rate=16000 "$RECORDING"
fi
