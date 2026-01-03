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

# Set binary paths based on local mode
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Prefer system-wide installation, fall back to local
if command -v xhispertool &>/dev/null; then
  XHISPERTOOL_BIN="xhispertool"
  XHISPERTOOLD_BIN="xhispertoold"
elif [ "$LOCAL_MODE" -eq 1 ] || [ -f "$SCRIPT_DIR/xhispertool" ]; then
  XHISPERTOOL_BIN="$SCRIPT_DIR/xhispertool"
  XHISPERTOOLD_BIN="$SCRIPT_DIR/xhispertoold"
else
  XHISPERTOOL_BIN="xhispertool"
  XHISPERTOOLD_BIN="xhispertoold"
fi

USE_SUDO=0
if [ ! -w /dev/uinput ]; then
  USE_SUDO=1
fi

run_xhispertool() {
  if [ "$USE_SUDO" -eq 1 ]; then
    sudo "$XHISPERTOOL_BIN" "$@"
  else
    "$XHISPERTOOL_BIN" "$@"
  fi
}

run_xhispertoold() {
  if [ "$USE_SUDO" -eq 1 ]; then
    sudo "$XHISPERTOOLD_BIN" "$@"
  else
    "$XHISPERTOOLD_BIN" "$@"
  fi
}

notify_user() {
  local msg="$1"
  if command -v notify-send &>/dev/null; then
    notify-send -u low -t 2000 "xhisper" "$msg"
  fi
  echo "$msg" >> "$LOGFILE"
}

acquire_state_lock() {
  exec 200>"$STATE_LOCK"
  if ! flock -n 200; then
    return 1
  fi
  STATE_LOCK_HELD=1
  return 0
}

release_state_lock() {
  if [ "$STATE_LOCK_HELD" -eq 1 ]; then
    flock -u 200 2>/dev/null || true
    exec 200>&-
    STATE_LOCK_HELD=0
  fi
}

RECORDING="/tmp/xhisper.wav"
LAST_RECORDING="/tmp/xhisper_last.wav"
RECORD_PID_FILE="/tmp/xhisper_recording.pid"
LOGFILE="/tmp/xhisper.log"
PROCESS_PATTERN="pw-record.*$RECORDING"
STATUS_FILE="/tmp/xhisper_status"
RETRY_MODE=0
RETRY_FILE=""
STATE_LOCK="/tmp/xhisper_state.lock"
STATE_LOCK_HELD=0

# Default configuration
long_recording_threshold=1000
transcription_prompt=""
silence_threshold=-50
silence_percentage=95
non_ascii_initial_delay=0.1
non_ascii_default_delay=0.025
show_status_messages=1
post_transcribe_delay=0
recording_target=""
llm_postprocess=0
llm_cli=""
llm_model_path=""
llm_threads=8
llm_ctx=4096
llm_max_tokens=512
llm_temp=0.2
llm_confidence_threshold=0.75
llm_guardrails_file=""
llm_min_chars=40
llm_timeout=25
llm_english_terms="ChatGPT, Photoshop, Adobe, Adobe Express, Acrobat, art direction"
llm_json_schema_file=""
llm_similarity_threshold=0.55
transcribe_timeout=120
normalize_timeout=20
record_stop_timeout=3

# Transcription backend: 'groq' (remote) or 'local' (run local command)
transcribe_backend="groq"
# Command to run for local transcription. Use '{file}' as placeholder for the audio file path.
# Example: "/home/user/whisper.cpp/main -m /home/user/models/ggml-base.bin -f {file} -otxt 2>/dev/null | head -n 1"
local_transcribe_cmd=""
CONFIG_FILE="${XDG_CONFIG_HOME:-$HOME/.config}/xhisper/xhisperrc"

if [ "${XHISPER_RETRY:-0}" = "1" ] || [ -n "${XHISPER_RETRY_FILE:-}" ]; then
  RETRY_MODE=1
  RETRY_FILE="${XHISPER_RETRY_FILE:-$LAST_RECORDING}"
fi

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
      status-messages)
        value_lc=$(echo "$value" | tr '[:upper:]' '[:lower:]')
        case "$value_lc" in
          0|false|off|no) show_status_messages=0 ;;
          *) show_status_messages=1 ;;
        esac
        ;;
      post-transcribe-delay) post_transcribe_delay="$value" ;;
      recording-target) recording_target="$value" ;;
      transcribe-backend) transcribe_backend="$value" ;;
      local-transcribe-cmd) local_transcribe_cmd="$value" ;;
      llm-postprocess)
        value_lc=$(echo "$value" | tr '[:upper:]' '[:lower:]')
        case "$value_lc" in
          1|true|on|yes) llm_postprocess=1 ;;
          *) llm_postprocess=0 ;;
        esac
        ;;
      llm-cli) llm_cli="$value" ;;
      llm-model-path) llm_model_path="$value" ;;
      llm-threads) llm_threads="$value" ;;
      llm-ctx) llm_ctx="$value" ;;
      llm-max-tokens) llm_max_tokens="$value" ;;
      llm-temp) llm_temp="$value" ;;
      llm-confidence-threshold) llm_confidence_threshold="$value" ;;
      llm-guardrails-file) llm_guardrails_file="$value" ;;
      llm-min-chars) llm_min_chars="$value" ;;
      llm-timeout) llm_timeout="$value" ;;
      llm-english-terms) llm_english_terms="$value" ;;
      llm-json-schema-file) llm_json_schema_file="$value" ;;
      llm-similarity-threshold) llm_similarity_threshold="$value" ;;
      transcribe-timeout) transcribe_timeout="$value" ;;
      normalize-timeout) normalize_timeout="$value" ;;
      record-stop-timeout) record_stop_timeout="$value" ;;
    esac
  done < "$CONFIG_FILE"
fi

# Allow environment override for status messages (useful when UI is enabled).
if [ -n "${XHISPER_STATUS_MESSAGES:-}" ]; then
  value_lc=$(echo "$XHISPER_STATUS_MESSAGES" | tr '[:upper:]' '[:lower:]')
  case "$value_lc" in
    0|false|off|no) show_status_messages=0 ;;
    *) show_status_messages=1 ;;
  esac
fi

# Allow config values to include escaped quotes (e.g. \" inside local-transcribe-cmd)
if [ -n "$local_transcribe_cmd" ]; then
  local_transcribe_cmd="${local_transcribe_cmd//\\\"/\"}"
fi
if [ -n "$llm_cli" ]; then
  llm_cli="${llm_cli//\\\"/\"}"
fi
if [ -n "$llm_model_path" ]; then
  llm_model_path="${llm_model_path//\\\"/\"}"
fi
if [ -n "$llm_guardrails_file" ]; then
  llm_guardrails_file="${llm_guardrails_file//\\\"/\"}"
fi
if [ -n "$llm_english_terms" ]; then
  llm_english_terms="${llm_english_terms//\\\"/\"}"
fi
if [ -n "$llm_json_schema_file" ]; then
  llm_json_schema_file="${llm_json_schema_file//\\\"/\"}"
fi
if [ -z "$llm_guardrails_file" ]; then
  llm_guardrails_file="${XDG_CONFIG_HOME:-$HOME/.config}/xhisper/guardrails.txt"
fi

# Auto-start daemon if not running
if ! pgrep -x xhispertoold > /dev/null; then
    run_xhispertoold 2>> /tmp/xhispertoold.log &
    sleep 1  # Give daemon time to start

    # Verify daemon started successfully
    if ! pgrep -x xhispertoold > /dev/null; then
        echo "Error: Failed to start xhispertoold daemon" >&2
        echo "Check /tmp/xhispertoold.log for details" >&2
        exit 1
    fi
fi

# Check if xhispertool is available
if ! command -v "$XHISPERTOOL_BIN" &> /dev/null; then
    echo "Error: xhispertool not found" >&2
    echo "Please either:" >&2
    echo "  - Run 'sudo make install' to install system-wide" >&2
    echo "  - Run 'xhisper --local' from the build directory" >&2
    exit 1
fi

# Detect clipboard tool
if command -v wl-copy &> /dev/null; then
    CLIP_COPY="wl-copy"
    CLIP_PASTE="wl-paste"
elif command -v xclip &> /dev/null; then
    CLIP_COPY() { xclip -selection clipboard; }
    CLIP_PASTE() { xclip -o -selection clipboard; }
else
    echo "Error: No clipboard tool found. Install wl-clipboard or xclip." >&2
    exit 1
fi

press_wrap_key() {
  if [ -n "$WRAP_KEY" ]; then
    run_xhispertool "$WRAP_KEY"
  fi
}

paste() {
  local text="$1"
  if [ "${XHISPER_NO_PASTE:-0}" = "1" ]; then
    return 0
  fi
  press_wrap_key
  # Type character by character
  # Use xhispertool type for ASCII (32-126), clipboard+paste for Unicode
  for ((i=0; i<${#text}; i++)); do
    local char="${text:$i:1}"
    local ascii=$(printf '%d' "'$char")

    if [[ $ascii -ge 32 && $ascii -le 126 ]]; then
      # ASCII printable character - use direct key typing (faster)
      run_xhispertool type "$char"
    else
      # Unicode or special character - use clipboard
      echo -n "$char" | $CLIP_COPY
      run_xhispertool paste
      # On first character (more error-prone), sleep longer
      [ "$i" -eq 0 ] && sleep "$non_ascii_initial_delay" || sleep "$non_ascii_default_delay"
    fi
  done
  press_wrap_key
}

dedupe_transcription() {
  local text="$1"
  if [ -z "$text" ]; then
    echo ""
    return
  fi
  python3 - "$text" <<'PY'
import re
import sys
from difflib import SequenceMatcher

text = sys.argv[1]
if not text:
    print("")
    raise SystemExit

def clean_for_compare(s: str) -> str:
    # Ensure sentence boundaries split even when a space is missing after punctuation.
    s = re.sub(r"([.!?])([^\s])", r"\1 \2", s)
    s = re.sub(r"\s+", " ", s.replace("[BLANK_AUDIO]", " ").strip())
    return s

def normalize(s: str) -> str:
    return re.sub(r"[^\w]+", "", s, flags=re.UNICODE).lower()

canon = clean_for_compare(text)
if not canon:
    print("")
    raise SystemExit

# Detect repeated sentence blocks.
sentences = [s.strip() for s in re.findall(r"[^.!?]+[.!?]?", canon) if s.strip()]
if len(sentences) >= 2 and len(sentences) % 2 == 0:
    mid = len(sentences) // 2
    if [normalize(s) for s in sentences[:mid]] == [normalize(s) for s in sentences[mid:]]:
        print(" ".join(sentences[:mid]).strip())
        raise SystemExit

# Detect repeated word blocks (no punctuation case).
words = re.findall(r"\w+", canon, flags=re.UNICODE)
if len(words) >= 8 and len(words) % 2 == 0:
    mid = len(words) // 2
    if [w.lower() for w in words[:mid]] == [w.lower() for w in words[mid:]]:
        print(" ".join(words[:mid]))
        raise SystemExit

# Detect near-duplicate halves (punctuation/spacing differences).
if len(canon) >= 40:
    half = len(canon) // 2
    left = normalize(canon[:half])
    right = normalize(canon[half:])
    if left and right and SequenceMatcher(None, left, right).ratio() > 0.95:
        print(canon[:half].strip())
        raise SystemExit

print(canon)
PY
}

cleanup_transcription() {
  local text="$1"
  if [ -z "$text" ]; then
    echo ""
    return
  fi
  python3 - "$text" <<'PY'
import re
import sys

text = sys.argv[1]
if not text:
    print("")
    raise SystemExit

text = text.replace("[BLANK_AUDIO]", " ")
text = re.sub(r"\s+", " ", text).strip()

# Collapse extreme character repeats while preserving legitimate double letters.
text = re.sub(r"(.)\1{2,}", r"\1\1", text, flags=re.UNICODE)

# Collapse repeated punctuation.
text = re.sub(r"([.!?,;:])\1{1,}", r"\1", text)

# Ensure spacing after punctuation.
text = re.sub(r"([.!?,;:])([^\s])", r"\1 \2", text)
text = re.sub(r"\s+", " ", text).strip()

print(text)
PY
}

set_status_file() {
  local value="$1"
  if [ -n "$STATUS_FILE" ]; then
    printf "%s" "$value" > "$STATUS_FILE" 2>/dev/null || true
  fi
}

is_noise_text() {
  local text="$1"
  python3 - "$text" <<'PY'
import re
import sys
from collections import Counter

text = sys.argv[1].strip()
if not text:
    sys.exit(0)

text = text.replace("[BLANK_AUDIO]", " ").strip()
if not re.search(r"[\w]", text, flags=re.UNICODE):
    sys.exit(0)

tokens = re.findall(r"[\w]+", text, flags=re.UNICODE)
if tokens:
    counts = Counter(t.lower() for t in tokens)
    if len(tokens) >= 8:
        most = counts.most_common(1)[0][1]
        if most / len(tokens) > 0.7:
            sys.exit(0)
        if len(counts) <= 2 and len(tokens) >= 12:
            sys.exit(0)

sys.exit(1)
PY
}

process_recording() {
  local keep_file="$1"
  local recording="$2"
  local normalized_recording=""

  if [ -z "$recording" ] || [ ! -f "$recording" ]; then
    echo "Recording missing: $recording" >> "$LOGFILE"
    return 1
  fi

  normalized_recording=$(normalize_recording "$recording")
  if [ "$normalized_recording" != "$recording" ]; then
    if [ "$keep_file" -eq 0 ]; then
      rm -f "$recording"
    fi
    recording="$normalized_recording"
  fi

  if [ -f "$recording" ]; then
    cp -f "$recording" "$LAST_RECORDING" 2>/dev/null || true
  fi

  if is_silent "$recording"; then
    if [ "$show_status_messages" -eq 1 ]; then
      paste "(no sound detected)"
      sleep 0.6
      delete_n_chars 19 # "(no sound detected)"
    fi
    if [ "$keep_file" -eq 0 ]; then
      rm -f "$recording"
    fi
    return 0
  fi

  if [ "$show_status_messages" -eq 1 ]; then
    paste "(transcribing...)"
  fi
  TRANSCRIPTION=$(transcribe "$recording")
  TRANSCRIPTION=$(postprocess_with_llm "$TRANSCRIPTION")
  if [ -n "$TRANSCRIPTION" ]; then
    TRANSCRIPTION=$(cleanup_transcription "$TRANSCRIPTION")
  fi
  if [ "$show_status_messages" -eq 1 ]; then
    delete_n_chars 17 # "(transcribing...)"
  fi

  if is_noise_text "$TRANSCRIPTION"; then
    echo "Transcription noise; skipping paste." >> "$LOGFILE"
    set_status_file "empty"
    if [ "$keep_file" -eq 0 ]; then
      rm -f "$recording"
    fi
    return 0
  fi

  if [ "$post_transcribe_delay" != "0" ]; then
    sleep "$post_transcribe_delay"
  fi

  if [ "$keep_file" -eq 1 ] && [ "${XHISPER_RETRY_PASTE:-0}" != "1" ]; then
    echo -n "$TRANSCRIPTION" | $CLIP_COPY
  else
    paste "$TRANSCRIPTION"
  fi

  if [ "$keep_file" -eq 0 ]; then
    rm -f "$recording"
  fi
  return 0
}

postprocess_with_llm() {
  local text="$1"
  local llm_log="/tmp/xhisper_llm.log"
  if [ "$llm_postprocess" -ne 1 ]; then
    echo "$text"
    return
  fi
  if [ -z "$text" ]; then
    echo ""
    return
  fi
  local text_len=${#text}
  if [ "$text_len" -lt "$llm_min_chars" ]; then
    echo "$text"
    return
  fi
  if [ -z "$llm_model_path" ] || [ ! -f "$llm_model_path" ]; then
    echo "$text"
    return
  fi

  local cli="$llm_cli"
  if [ -z "$cli" ]; then
    if command -v llama-cli &> /dev/null; then
      cli="llama-cli"
    elif [ -x "$HOME/CODING/llama.cpp/build/bin/llama-cli" ]; then
      cli="$HOME/CODING/llama.cpp/build/bin/llama-cli"
    else
      echo "$text"
      return
    fi
  fi
  if [ ! -x "$cli" ]; then
    echo "$text"
    return
  fi

  echo "LLM postprocess start" >> "$llm_log"
  local llm_output=""
  local llm_input=""
  llm_input=$(mktemp /tmp/xhisper_llm_input.XXXXXX)
  printf '%s' "$text" > "$llm_input"
  echo "LLM input bytes: $(wc -c < "$llm_input")" >> "$llm_log"
  llm_output=$(LLM_CLI="$cli" \
    LLM_MODEL="$llm_model_path" \
    LLM_THREADS="$llm_threads" \
    LLM_CTX="$llm_ctx" \
    LLM_MAX_TOKENS="$llm_max_tokens" \
    LLM_TEMP="$llm_temp" \
    LLM_CONFIDENCE="$llm_confidence_threshold" \
    LLM_SIMILARITY_THRESHOLD="$llm_similarity_threshold" \
    LLM_GUARDRAILS_FILE="$llm_guardrails_file" \
    LLM_ENGLISH_TERMS="$llm_english_terms" \
    LLM_TIMEOUT="$llm_timeout" \
    LLM_JSON_SCHEMA_FILE="$llm_json_schema_file" \
    LLM_INPUT_FILE="$llm_input" \
    python3 - <<'PY'
import json
import os
import re
import subprocess
import sys
from difflib import SequenceMatcher

input_path = os.environ.get("LLM_INPUT_FILE", "")
raw = ""
if input_path and os.path.exists(input_path):
    with open(input_path, "r", encoding="utf-8") as fh:
        raw = fh.read()
if not raw:
    print("")
    raise SystemExit

def clean_text(s: str) -> str:
    s = s.replace("[BLANK_AUDIO]", " ")
    s = re.sub(r"\s+", " ", s.strip())
    return s

def normalize(s: str) -> str:
    return re.sub(r"[^\w]+", "", s, flags=re.UNICODE).lower()

def apply_guardrail_replacements(s: str, repls):
    out = s
    for pat, rep in repls:
        try:
            out = re.sub(re.escape(pat), rep, out, flags=re.IGNORECASE)
        except re.error:
            out = out.replace(pat, rep)
    return out

text = clean_text(raw)
if not text:
    print("")
    raise SystemExit

guardrails_file = os.environ.get("LLM_GUARDRAILS_FILE", "")
protect = []
repls = []
if guardrails_file and os.path.exists(guardrails_file):
    with open(guardrails_file, "r", encoding="utf-8") as fh:
        for line in fh:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            if "=" in line:
                left, right = line.split("=", 1)
                left = left.strip()
                right = right.strip()
                if left and right:
                    repls.append((left, right))
            else:
                protect.append(line)

placeholders = {}
protected_text = text
protect_sorted = sorted(protect, key=len, reverse=True)
for i, token in enumerate(protect_sorted):
    if not token:
        continue
    ph = f"__GR{i}__"
    placeholders[ph] = token
    protected_text = re.sub(re.escape(token), ph, protected_text, flags=re.IGNORECASE)

# Apply guardrail replacements before LLM to help it keep known terms.
protected_text = apply_guardrail_replacements(protected_text, repls)

english_terms = os.environ.get("LLM_ENGLISH_TERMS", "").strip()
schema_file = os.environ.get("LLM_JSON_SCHEMA_FILE", "").strip()
english_hint = ""
if english_terms:
    english_hint = (
        "If the input contains Greek transliterations of these English terms, "
        "output them in English exactly as written: "
        f"{english_terms}. "
    )

prompt = (
    "You are a Greek transcription editor. Rewrite the text into correct, natural Greek. "
    "Fix spelling, diacritics, punctuation, grammar, and word splits. "
    "Remove obvious noise tokens and repeated garbage. "
    "Preserve meaning and proper nouns. "
    + english_hint +
    "Do NOT change tokens like __GR0__ (guardrails). "
    "Return ONLY JSON with keys: text, confidence (0-1), changed (true/false).\n"
    "Input:\n"
    f"{protected_text}\n"
    "JSON:"
)

cmd = [
    os.environ.get("LLM_CLI", "llama-cli"),
    "-m", os.environ.get("LLM_MODEL", ""),
    "-t", os.environ.get("LLM_THREADS", "8"),
    "--ctx-size", os.environ.get("LLM_CTX", "4096"),
    "-n", os.environ.get("LLM_MAX_TOKENS", "512"),
    "--temp", os.environ.get("LLM_TEMP", "0.2"),
    "--top-p", "0.9",
    "--repeat-penalty", "1.1",
    "-p", prompt,
]
if schema_file and os.path.exists(schema_file):
    cmd.extend(["--json-schema-file", schema_file])

timeout_s = 25.0
try:
    timeout_s = float(os.environ.get("LLM_TIMEOUT", "25"))
except Exception:
    timeout_s = 25.0

try:
    proc = subprocess.run(cmd, capture_output=True, text=True, check=False, timeout=timeout_s)
except subprocess.TimeoutExpired:
    cleaned = apply_guardrail_replacements(text, repls)
    print(cleaned)
    raise SystemExit
except Exception:
    cleaned = apply_guardrail_replacements(text, repls)
    print(cleaned)
    raise SystemExit

output = (proc.stdout or "") + "\n" + (proc.stderr or "")
match = re.search(r"\{.*\}", output, flags=re.DOTALL)
if not match:
    cleaned = apply_guardrail_replacements(text, repls)
    print(cleaned)
    raise SystemExit

json_str = match.group(0)
try:
    data = json.loads(json_str)
except Exception:
    cleaned = apply_guardrail_replacements(text, repls)
    print(cleaned)
    raise SystemExit

confidence = 0.0
try:
    confidence = float(data.get("confidence", 0.0))
except Exception:
    confidence = 0.0

out_text = clean_text(str(data.get("text", "")))
if not out_text:
    cleaned = apply_guardrail_replacements(text, repls)
    print(cleaned)
    raise SystemExit

threshold = float(os.environ.get("LLM_CONFIDENCE", "0.75"))
if confidence < threshold:
    cleaned = apply_guardrail_replacements(text, repls)
    print(cleaned)
    raise SystemExit

# Restore protected tokens.
restored = out_text
for ph, token in placeholders.items():
    restored = restored.replace(ph, token)

# Apply guardrail replacements last.
restored = apply_guardrail_replacements(restored, repls)

# Avoid extreme drift: if output diverges too far, keep original.
sim_threshold = float(os.environ.get("LLM_SIMILARITY_THRESHOLD", "0.55"))
if len(normalize(text)) >= 20:
    sim = SequenceMatcher(None, normalize(text), normalize(restored)).ratio()
    if sim < sim_threshold:
        cleaned = apply_guardrail_replacements(text, repls)
        print(cleaned)
        raise SystemExit

print(restored)
PY
  )
  rm -f "$llm_input"
  local llm_status=$?
  echo "LLM postprocess exit: $llm_status" >> "$llm_log"
  if [ -n "$llm_output" ]; then
    echo "LLM output: $llm_output" >> "$llm_log"
  else
    echo "LLM output: <empty>" >> "$llm_log"
  fi
  if [ "$llm_status" -ne 0 ] || [ -z "$llm_output" ]; then
    echo "$text"
    return
  fi
  echo "$llm_output"
}

delete_n_chars() {
  local n="$1"
  if [ "${XHISPER_NO_PASTE:-0}" = "1" ]; then
    return 0
  fi
  for ((i=0; i<n; i++)); do
    run_xhispertool backspace
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

normalize_recording() {
  local recording="$1"
  local normalized="${recording%.wav}.norm.wav"

  if command -v timeout &>/dev/null; then
    if timeout "$normalize_timeout" ffmpeg -y -i "$recording" -ac 1 -ar 16000 -f wav "$normalized" >>"$LOGFILE" 2>&1; then
      echo "$normalized"
    else
      echo "$recording"
    fi
    return
  fi

  if ffmpeg -y -i "$recording" -ac 1 -ar 16000 -f wav "$normalized" >>"$LOGFILE" 2>&1; then
    echo "$normalized"
  else
    echo "$recording"
  fi
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

  local transcription=""

  if [ "$transcribe_backend" = "local" ]; then
    if [ -z "$local_transcribe_cmd" ]; then
      echo "Error: transcribe-backend set to 'local' but no local-transcribe-cmd configured" >&2
      logging_end_and_write_to_logfile "Transcription" "ERROR: no local_transcribe_cmd" "$logging_start"
      echo ""
      return
    fi

    # Replace placeholders with actual values
    local cmd="${local_transcribe_cmd//\{file\}/$recording}"
    if [ -n "$transcription_prompt" ]; then
      cmd="${cmd//\{prompt\}/$transcription_prompt}"
    fi
    echo "Local transcribe cmd: $cmd" >> "$LOGFILE"
    # Run the local transcription command and capture its stdout
    local cmd_output=""
    local cmd_status=0
    if command -v timeout &>/dev/null; then
      cmd_output=$(timeout "$transcribe_timeout" bash -lc "$cmd" 2>>"$LOGFILE")
      cmd_status=$?
    else
      cmd_output=$(eval "$cmd" 2>>"$LOGFILE")
      cmd_status=$?
    fi
    echo "Local transcribe exit: $cmd_status" >> "$LOGFILE"
    if [ "$cmd_status" -ne 0 ]; then
      logging_end_and_write_to_logfile "Transcription" "ERROR: local transcribe exit $cmd_status" "$logging_start"
      echo ""
      return
    fi
    transcription=$(echo "$cmd_output" | tr '\n' ' ' | sed 's/^ //; s/ $//')
  else
    transcription=$(curl -s -X POST "https://api.groq.com/openai/v1/audio/transcriptions" \
      -H "Authorization: Bearer $GROQ_API_KEY" \
      -H "Content-Type: multipart/form-data" \
      -F "file=@$recording" \
      -F "model=$model" \
      -F "prompt=$transcription_prompt" \
      | jq -r '.text' | sed 's/^ //') # Transcription always returns a leading space, so remove it via sed
  fi

  transcription=$(dedupe_transcription "$transcription")
  transcription=$(cleanup_transcription "$transcription")

  logging_end_and_write_to_logfile "Transcription" "$transcription" "$logging_start"

  echo "$transcription"
}

recording_pid_alive() {
  local pid="$1"
  [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null
}

get_recording_pid() {
  if [ -f "$RECORD_PID_FILE" ]; then
    local pid
    pid=$(cat "$RECORD_PID_FILE" 2>/dev/null)
    if recording_pid_alive "$pid"; then
      echo "$pid"
      return 0
    fi
    rm -f "$RECORD_PID_FILE"
  fi
  return 1
}

recording_active() {
  if get_recording_pid >/dev/null; then
    return 0
  fi
  if pgrep -f "$PROCESS_PATTERN" > /dev/null; then
    return 0
  fi
  return 1
}

stop_recording_process() {
  local pid
  pid=$(get_recording_pid 2>/dev/null || true)
  if [ -n "$pid" ]; then
    kill -INT "$pid" 2>/dev/null || true
    local max_loops=$((record_stop_timeout * 10))
    local i=0
    while kill -0 "$pid" 2>/dev/null && [ "$i" -lt "$max_loops" ]; do
      sleep 0.1
      i=$((i + 1))
    done
    if kill -0 "$pid" 2>/dev/null; then
      kill -TERM "$pid" 2>/dev/null || true
    fi
  else
    pkill -f "$PROCESS_PATTERN" 2>/dev/null || true
  fi
  rm -f "$RECORD_PID_FILE"
  sleep 0.2
}

start_recording_process() {
  if recording_active; then
    if [ ! -f "$RECORD_PID_FILE" ]; then
      pkill -f "$PROCESS_PATTERN" 2>/dev/null || true
    else
      return 0
    fi
  fi
  rm -f "$RECORDING"
  local pid=""
  record_cmd=(pw-record --channels=1 --rate=16000)
  if [ -n "$recording_target" ]; then
    record_cmd+=(--target "$recording_target")
  fi
  record_cmd+=("$RECORDING")
  "${record_cmd[@]}" >/tmp/xhisper_record.log 2>&1 &
  pid=$!
  sleep 0.1
  if ! recording_pid_alive "$pid" && [ -n "$recording_target" ]; then
    # Fallback: try default source if target fails
    record_cmd=(pw-record --channels=1 --rate=16000 "$RECORDING")
    "${record_cmd[@]}" >/tmp/xhisper_record.log 2>&1 &
    pid=$!
    sleep 0.1
  fi
  if recording_pid_alive "$pid"; then
    echo "$pid" > "$RECORD_PID_FILE"
    return 0
  fi
  echo "Recording start failed. See /tmp/xhisper_record.log" >> "$LOGFILE"
  return 1
}

# Main

# Ensure only one xhisper action runs at a time.
if ! acquire_state_lock; then
  notify_user "Currently transcribing. Wait..."
  exit 0
fi
trap 'release_state_lock' EXIT

# Retry mode: re-transcribe last recording.
if [ "$RETRY_MODE" -eq 1 ]; then
  if recording_active; then
    echo "Retry requested while recording active; skipping." >> "$LOGFILE"
    exit 1
  fi
  if [ ! -f "$RETRY_FILE" ]; then
    echo "Retry file missing: $RETRY_FILE" >> "$LOGFILE"
    exit 1
  fi
  printf "processing" > "$STATUS_FILE" 2>/dev/null || true
  process_recording 1 "$RETRY_FILE"
  printf "idle" > "$STATUS_FILE" 2>/dev/null || true
  exit 0
fi

# Find recording process, if so then kill
if recording_active; then
  stop_recording_process
  if [ "$show_status_messages" -eq 1 ]; then
    delete_n_chars 14 # "(recording...)"
  fi
  process_recording 0 "$RECORDING"
else
  # No recording running, so start
  sleep 0.2
  if [ "$show_status_messages" -eq 1 ]; then
    paste "(recording...)"
  fi
  start_recording_process
fi
