#!/bin/bash
# Tests for the paste() function in xhisper.sh
# Uses mock replacements for xhispertool and wl-copy/wl-paste to capture behavior

PASS=0
FAIL=0
CALL_LOG=""

# ── Test harness ─────────────────────────────────────────────────────────────

assert_equals() {
  local desc="$1" expected="$2" actual="$3"
  if [[ "$expected" == "$actual" ]]; then
    echo "  PASS: $desc"
    ((PASS++))
  else
    echo "  FAIL: $desc"
    echo "        expected: $(echo "$expected" | cat -A)"
    echo "        actual:   $(echo "$actual" | cat -A)"
    ((FAIL++))
  fi
}

assert_contains() {
  local desc="$1" needle="$2" haystack="$3"
  if echo "$haystack" | grep -qF "$needle"; then
    echo "  PASS: $desc"
    ((PASS++))
  else
    echo "  FAIL: $desc"
    echo "        expected to find: $needle"
    echo "        in: $haystack"
    ((FAIL++))
  fi
}

assert_not_contains() {
  local desc="$1" needle="$2" haystack="$3"
  if ! echo "$haystack" | grep -qF "$needle"; then
    echo "  PASS: $desc"
    ((PASS++))
  else
    echo "  FAIL: $desc"
    echo "        expected NOT to find: $needle"
    echo "        in: $haystack"
    ((FAIL++))
  fi
}

# ── Test environment setup ────────────────────────────────────────────────────

setup() {
  CALL_LOG_FILE=$(mktemp)
  CLIPBOARD_FILE=$(mktemp)
  echo -n "$INITIAL_CLIPBOARD" > "$CLIPBOARD_FILE"

  # Mock xhispertool — logs each call with timestamp
  XHISPERTOOL=$(mktemp)
  cat > "$XHISPERTOOL" <<'EOF'
#!/bin/bash
echo "xhispertool $*" >> "$CALL_LOG_FILE"
EOF
  chmod +x "$XHISPERTOOL"
  export XHISPERTOOL CALL_LOG_FILE

  # Mock wl-copy — writes stdin to clipboard file, logs call
  WL_COPY=$(mktemp)
  cat > "$WL_COPY" <<'EOF'
#!/bin/bash
content=$(cat)
echo -n "$content" > "$CLIPBOARD_FILE"
echo "wl-copy:[$content]" >> "$CALL_LOG_FILE"
EOF
  chmod +x "$WL_COPY"

  # Mock wl-paste — reads from clipboard file
  WL_PASTE=$(mktemp)
  cat > "$WL_PASTE" <<'EOF'
#!/bin/bash
cat "$CLIPBOARD_FILE"
EOF
  chmod +x "$WL_PASTE"

  export CLIP_COPY="$WL_COPY"
  export CLIP_PASTE="$WL_PASTE"
  export CLIPBOARD_FILE
  export CALL_LOG_FILE

  # Stub press_wrap_key and other deps
  WRAP_KEY=""
  non_ascii_initial_delay=0
  non_ascii_default_delay=0
}

teardown() {
  rm -f "$CALL_LOG_FILE" "$CLIPBOARD_FILE" "$XHISPERTOOL" "$WL_COPY" "$WL_PASTE"
}

# Source only the paste() function from xhisper.sh by extracting it
load_paste_fn() {
  # Extract paste() and its dependency press_wrap_key from xhisper.sh
  local script_dir
  script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  source <(sed -n '/^press_wrap_key()/,/^}/p; /^paste()/,/^}/p' "$script_dir/../xhisper.sh")
}

# ── Tests ─────────────────────────────────────────────────────────────────────

test_ascii_only_never_touches_clipboard() {
  echo "test_ascii_only_never_touches_clipboard"
  INITIAL_CLIPBOARD="my precious clipboard"
  setup
  load_paste_fn

  paste "Hello, world!"

  local log
  log=$(cat "$CALL_LOG_FILE")
  assert_not_contains "wl-copy not called for ASCII-only text" "wl-copy:" "$log"

  local final_clipboard
  final_clipboard=$(cat "$CLIPBOARD_FILE")
  assert_equals "clipboard unchanged after ASCII-only paste" "my precious clipboard" "$final_clipboard"

  teardown
}

test_ascii_typed_via_xhispertool_type() {
  echo "test_ascii_typed_via_xhispertool_type"
  INITIAL_CLIPBOARD=""
  setup
  load_paste_fn

  paste "Hi"

  local log
  log=$(cat "$CALL_LOG_FILE")
  assert_contains "H typed via xhispertool type" "xhispertool type H" "$log"
  assert_contains "i typed via xhispertool type" "xhispertool type i" "$log"

  teardown
}

test_clipboard_restored_after_non_ascii() {
  echo "test_clipboard_restored_after_non_ascii"
  INITIAL_CLIPBOARD="saved content"
  setup
  load_paste_fn

  paste "Hello\u2014world"  # em dash in the middle

  local final_clipboard
  final_clipboard=$(cat "$CLIPBOARD_FILE")
  assert_equals "clipboard restored to original after non-ASCII paste" "saved content" "$final_clipboard"

  teardown
}

test_clipboard_restored_when_initially_empty() {
  echo "test_clipboard_restored_when_initially_empty"
  INITIAL_CLIPBOARD=""
  setup
  load_paste_fn

  paste $'caf\u00e9'  # café — é is non-ASCII

  local final_clipboard
  final_clipboard=$(cat "$CLIPBOARD_FILE")
  assert_equals "clipboard empty after paste when it started empty" "" "$final_clipboard"

  teardown
}

test_non_ascii_uses_clipboard_then_paste() {
  echo "test_non_ascii_uses_clipboard_then_paste"
  INITIAL_CLIPBOARD=""
  setup
  load_paste_fn

  paste $'\u00f1'  # ñ

  local log
  log=$(cat "$CALL_LOG_FILE")
  assert_contains "wl-copy called for non-ASCII char" "wl-copy:[ñ]" "$log"
  assert_contains "xhispertool paste called after wl-copy" "xhispertool paste" "$log"

  teardown
}

test_consecutive_non_ascii_batched_into_one_paste() {
  echo "test_consecutive_non_ascii_batched_into_one_paste"
  INITIAL_CLIPBOARD=""
  setup
  load_paste_fn

  # Smart open quote + word + smart close quote: "hello"
  paste $'\u201chello\u201d'

  local log
  log=$(cat "$CALL_LOG_FILE")

  # Count how many times xhispertool paste was called
  local paste_count
  paste_count=$(grep -c "xhispertool paste" "$CALL_LOG_FILE" || true)
  assert_equals "two non-ASCII quotes = 2 clipboard pastes (one per isolated chunk)" "2" "$paste_count"

  # Count wl-copy calls: 2 content writes (open quote, close quote) + 1 restore = 3 total
  local copy_count
  copy_count=$(grep -c "wl-copy:" "$CALL_LOG_FILE" || true)
  assert_equals "wl-copy called 3 times total (2 content + 1 restore)" "3" "$copy_count"

  teardown
}

test_sleep_before_paste_not_after() {
  echo "test_sleep_before_paste_not_after"
  INITIAL_CLIPBOARD=""
  setup

  # Use a mock sleep that logs calls with sequence numbers
  MOCK_SLEEP=$(mktemp)
  cat > "$MOCK_SLEEP" <<'EOF'
#!/bin/bash
echo "sleep $*" >> "$CALL_LOG_FILE"
EOF
  chmod +x "$MOCK_SLEEP"

  # Override sleep in subshell
  export -f paste 2>/dev/null || true
  load_paste_fn

  # Temporarily override sleep
  sleep() { echo "sleep $*" >> "$CALL_LOG_FILE"; }
  export -f sleep

  paste $'\u00e9'  # é

  local log
  log=$(cat "$CALL_LOG_FILE")

  # sleep must appear BEFORE xhispertool paste in the log
  local sleep_line paste_line
  sleep_line=$(grep -n "^sleep" "$CALL_LOG_FILE" | head -1 | cut -d: -f1)
  paste_line=$(grep -n "xhispertool paste" "$CALL_LOG_FILE" | head -1 | cut -d: -f1)

  if [[ -n "$sleep_line" && -n "$paste_line" && "$sleep_line" -lt "$paste_line" ]]; then
    echo "  PASS: sleep occurs before xhispertool paste"
    ((PASS++))
  else
    echo "  FAIL: sleep occurs before xhispertool paste"
    echo "        sleep at line $sleep_line, paste at line $paste_line"
    ((FAIL++))
  fi

  rm -f "$MOCK_SLEEP"
  teardown
}

test_mixed_ascii_and_non_ascii_order_preserved() {
  echo "test_mixed_ascii_and_non_ascii_order_preserved"
  INITIAL_CLIPBOARD=""
  setup
  load_paste_fn

  # "café" → c,a,f are ASCII; é is non-ASCII
  paste $'caf\u00e9'

  local log
  log=$(cat "$CALL_LOG_FILE")

  # c, a, f should be typed before the clipboard paste for é
  local f_line paste_line
  f_line=$(grep -n "xhispertool type f" "$CALL_LOG_FILE" | head -1 | cut -d: -f1)
  paste_line=$(grep -n "xhispertool paste" "$CALL_LOG_FILE" | head -1 | cut -d: -f1)

  if [[ -n "$f_line" && -n "$paste_line" && "$f_line" -lt "$paste_line" ]]; then
    echo "  PASS: ASCII chars typed before non-ASCII clipboard paste"
    ((PASS++))
  else
    echo "  FAIL: ASCII chars typed before non-ASCII clipboard paste"
    echo "        'f' at line $f_line, paste at line $paste_line"
    ((FAIL++))
  fi

  teardown
}

# ── Run all tests ─────────────────────────────────────────────────────────────

echo ""
echo "Running paste() tests..."
echo "────────────────────────"

test_ascii_only_never_touches_clipboard
echo ""
test_ascii_typed_via_xhispertool_type
echo ""
test_clipboard_restored_after_non_ascii
echo ""
test_clipboard_restored_when_initially_empty
echo ""
test_non_ascii_uses_clipboard_then_paste
echo ""
test_consecutive_non_ascii_batched_into_one_paste
echo ""
test_sleep_before_paste_not_after
echo ""
test_mixed_ascii_and_non_ascii_order_preserved

echo ""
echo "────────────────────────"
echo "Results: $PASS passed, $FAIL failed"
echo ""

[[ $FAIL -eq 0 ]]
