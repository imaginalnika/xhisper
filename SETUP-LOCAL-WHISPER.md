# Local Whisper Setup for xhisper (Greek Support)

## Quick Start (3 steps)

### 1. Install whisper.cpp
```bash
git clone https://github.com/ggerganov/whisper.cpp.git
cd whisper.cpp
make
```
Binary will be at `./main`. Place it somewhere accessible, e.g., `/usr/local/bin/whisper`:
```bash
sudo cp main /usr/local/bin/whisper
```

### 2. Download Greek-capable model
Download a ggml multilingual model (supports Greek):
```bash
mkdir -p ~/.local/share/whisper.cpp/models
cd ~/.local/share/whisper.cpp/models

# Option A: ggml-base.bin (recommended for speed/quality balance)
wget https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.bin

# Option B: ggml-medium.bin (slower but higher accuracy)
wget https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-medium.bin

# Option C: ggml-large.bin (slowest but best accuracy, requires more GPU/memory)
wget https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-large.bin
```

### 3. Configure xhisper
Create or edit `~/.config/xhisper/xhisperrc`:
```bash
mkdir -p ~/.config/xhisper
cp default_xhisperrc ~/.config/xhisper/xhisperrc
```

Edit `~/.config/xhisper/xhisperrc` and set:
```
transcribe-backend       : local
local-transcribe-cmd     : "/usr/local/bin/whisper -m ~/.local/share/whisper.cpp/models/ggml-base.bin -f {file} -otxt --prompt \"{prompt}\" -nt 2>/dev/null | head -n 1"
transcription-prompt     : ""
```

Replace paths if you installed whisper.cpp or models elsewhere.

## Test Setup

Test the local transcription command directly:
```bash
# Record test audio (5 seconds) - speak Greek into your microphone
pw-record --channels=1 --rate=16000 /tmp/test_greek.wav &
sleep 5
pkill -f "pw-record.*test_greek"

# Transcribe it
/usr/local/bin/whisper -m ~/.local/share/whisper.cpp/models/ggml-base.bin -f /tmp/test_greek.wav -otxt 2>/dev/null | head -n 1
```

## Run xhisper

After setup, use xhisper from the repo:
```bash
cd /home/avenus/CODING/xhisper
./xhisper.sh
```

Or from anywhere after `sudo make install`:
```bash
sudo make install
xhisper
```

## Usage

1. Run `xhisper` (or press your bound hotkey) once to start recording.
2. Speak in Greek (or any language the model supports).
3. Run `xhisper` again (or press the hotkey again) to stop, transcribe, and insert the text at your cursor.

## Bind Hotkey

### sxhkd
```
super + d
    xhisper
```

### i3 / sway
```
bindsym $mod+d exec xhisper
```

### Hyprland
```
bind = $mainMod, D, exec, xhisper
```

## Troubleshooting

**Daemon fails to start (permission):**
```bash
sudo usermod -aG input $USER
# Log out and log in
```

**Local transcription returns empty:**
- Test your command manually:
  ```bash
  /usr/local/bin/whisper -m ~/.local/share/whisper.cpp/models/ggml-base.bin -f /tmp/xhisper.wav -otxt 2>/dev/null | head -n 1
  ```
- Check `/tmp/xhisper.log` for errors.

**Slow transcription:**
- Use smaller model (`ggml-base.bin` instead of `ggml-large.bin`).
- If you have a GPU (CUDA/Metal), rebuild whisper.cpp with GPU support:
  ```bash
  cd whisper.cpp
  make clean
  make WHISPER_CUDA=1  # or WHISPER_METAL=1 for Mac
  ```

**Inaccurate Greek transcription:**
- Use larger model (`ggml-medium.bin` or `ggml-large.bin`).
- Add context to `transcription-prompt` in `xhisperrc`:
  ```
  transcription-prompt : "Ελληνικά"
  ```
 - If long processing causes stalls, lower timeouts in `xhisperrc`:
   ```
   transcribe-timeout  : 120
   normalize-timeout   : 20
   record-stop-timeout : 3
   ```
