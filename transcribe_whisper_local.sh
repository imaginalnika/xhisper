#!/bin/bash
# transcribe_whisper_local.sh
# Helper script for local whisper.cpp transcription
# Usage: transcribe_whisper_local.sh <audio_file> [model_path] [whisper_bin_path]

AUDIO_FILE="${1:-/tmp/xhisper.wav}"
MODEL_PATH="${2:-/usr/local/share/whisper.cpp/models/ggml-base.bin}"
WHISPER_BIN="${3:-whisper}"

# Check if audio file exists
if [ ! -f "$AUDIO_FILE" ]; then
    echo "Error: Audio file not found: $AUDIO_FILE" >&2
    exit 1
fi

# Check if model exists
if [ ! -f "$MODEL_PATH" ]; then
    echo "Error: Model file not found: $MODEL_PATH" >&2
    echo "Download from: https://huggingface.co/ggerganov/whisper.cpp/tree/main" >&2
    exit 1
fi

# Run whisper.cpp and extract text
# Supports both 'whisper' CLI and direct path to binary
if command -v "$WHISPER_BIN" &>/dev/null || [ -x "$WHISPER_BIN" ]; then
    "$WHISPER_BIN" -m "$MODEL_PATH" -f "$AUDIO_FILE" -otxt 2>/dev/null | head -n 1
else
    echo "Error: whisper binary not found at: $WHISPER_BIN" >&2
    echo "Install whisper.cpp or update WHISPER_BIN path" >&2
    exit 1
fi
