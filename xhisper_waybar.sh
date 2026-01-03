#!/bin/bash

STATUS_FILE="/tmp/xhisper_status"
LAST_WAV="/tmp/xhisper_last.wav"

status="idle"
if [ -f "$STATUS_FILE" ]; then
  status=$(tr -d '\r\n' < "$STATUS_FILE")
fi

case "$status" in
  listening|processing|idle|empty) ;;
  *) status="idle" ;;
esac

text="xh: $status"
tooltip="last audio: none"

if [ -f "$LAST_WAV" ]; then
  size=$(stat -c %s "$LAST_WAV" 2>/dev/null || echo 0)
  size_kb=$((size / 1024))
  mtime=$(stat -c %Y "$LAST_WAV" 2>/dev/null || echo 0)
  now=$(date +%s)
  age=$((now - mtime))
  tooltip="last audio: ${size_kb} KB\nage: ${age}s"
  if command -v ffprobe >/dev/null 2>&1; then
    dur=$(ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 "$LAST_WAV" 2>/dev/null)
    if [ -n "$dur" ]; then
      dur_s=$(awk "BEGIN {printf \"%.1f\", $dur}")
      tooltip="${tooltip}\nduration: ${dur_s}s"
    fi
  fi
fi

tooltip=${tooltip//\\/\\\\}
tooltip=${tooltip//$'\n'/\\n}
tooltip=${tooltip//\"/\\\"}

printf '{"text":"%s","class":"%s","tooltip":"%s"}\n' "$text" "$status" "$tooltip"
