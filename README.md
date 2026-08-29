<div align="center">
  <h1>xhisper <i>/ˈzɪspər/</i></h1>
  <img src="demo.gif" alt="xhisper demo" width="300">
  <br><br>
</div>

Dictation at cursor for Linux.

## Installation

### Dependencies

<details>
<summary>Arch Linux / Manjaro</summary>
<pre><code>sudo pacman -S pipewire jq curl ffmpeg gcc</code></pre>
</details>

<details>
<summary>Debian / Ubuntu / Linux Mint</summary>
<pre><code>sudo apt update
sudo apt install pipewire jq curl ffmpeg gcc</code></pre>
</details>

<details>
<summary>Fedora / RHEL / AlmaLinux / Rocky</summary>
<pre><code>sudo dnf install -y pipewire pipewire-utils jq curl ffmpeg gcc</code></pre>
</details>

<details>
<summary>OpenSUSE (Leap / Tumbleweed)</summary>
<pre><code>sudo zypper refresh
sudo zypper install pipewire jq curl ffmpeg gcc</code></pre>
</details>

<details>
<summary>Void Linux</summary>
<pre><code>sudo xbps-install -S
sudo xbps-install pipewire jq curl ffmpeg gcc</code></pre>
</details>

**Note:** `wl-clipboard` (Wayland) or `xclip` (X11) required for non-ASCII but usually pre-installed.

### Setup

1. **Add user to input group** to access `/dev/uinput`:
```sh
sudo usermod -aG input $USER
```
Then **log out and log back in** (restart is safer) for the group change to take effect.

Check by running:

```sh
groups
```

You should see `input` in the output.

2. **Get a Groq API key** from [console.groq.com](https://console.groq.com) (free tier available) and add to `~/.env`:
```sh
GROQ_API_KEY=<your_API_key>
```

3. Clone the repository and install:
```sh
git clone --depth 1 https://github.com/lv10/xhisper.git
cd xhisper && make
make config
sudo make install
```

4. Bind `xhisper` binary to your favorite key:

<details>
<summary>keyd</summary>

```ini
[main]
capslock = layer(dictate)

[dictate:C]
d = macro(xhisper)
```
</details>

<details>
<summary>sxhkd</summary>

```
super + d
    xhisper
```
</details>

<details>
<summary>i3 / sway</summary>

```
bindsym $mod+d exec xhisper
```
</details>

<details>
<summary>Hyprland</summary>

```
bind = $mainMod, D, exec, xhisper
```
</details>

<details>
<summary>Gnome</summary>

```sh
# In your terminal:

name="xhisper"
binding="<CTRL><SHIFT>X"
action="/usr/local/bin/xhisper"

media_keys=org.gnome.settings-daemon.plugins.media-keys
custom_kbd=org.gnome.settings-daemon.plugins.media-keys.custom-keybinding
kbd_path=/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/$name/
new_bindings=`gsettings get $media_keys custom-keybindings | sed -e"s>'\]>','$kbd_path']>"| sed -e"s>@as \[\]>['$kbd_path']>"`
gsettings set $media_keys custom-keybindings "$new_bindings"
gsettings set $custom_kbd:$kbd_path name "$name"
gsettings set $custom_kbd:$kbd_path binding "$binding"
gsettings set $custom_kbd:$kbd_path command "$action"
```
</details>

---

## Usage

Simply run `xhisper` twice (via your favorite keybinding):
- **First run**: Starts recording
- **Second run**: Stops and transcribes

The transcription will be typed at your cursor position.

**View logs:**
```sh
xhisper --log
```

**Non-QWERTY layouts:**

For non-QWERTY layouts (e.g. Dvorak, International), set up an input switch key to QWERTY (e.g. rightalt). Then instead of binding to `xhisper`, bind to:
```sh
xhisper --<your-input-switch-key>
```

**Available input switch keys:** `--leftalt`, `--rightalt`, `--leftctrl`, `--rightctrl`, `--leftshift`, `--rightshift`, `--super`

Key chords (like ctrl-space) not available yet.

---

## Configuration

Configuration is read from `~/.config/xhisper/xhisperrc`.

You can initialize it by running:
```sh
make config
```

Or manually:
```sh
mkdir -p ~/.config/xhisper
cp default_xhisperrc ~/.config/xhisper/xhisperrc
```

To view your current configuration:
```sh
xhisper --config
# or
make show
```

| Option | Default | Description |
|--------|---------|-------------|
| `long-recording-threshold` | `1000` | Duration in ms above which the larger `whisper-large-v3` model is used instead of `whisper-large-v3-turbo` |
| `transcription-prompt` | _(empty)_ | Context hint passed to Whisper to improve accuracy (e.g. common words, names, or domain vocabulary) |
| `non-ascii-initial-delay` | `0.15` | Seconds to wait before pasting the first non-ASCII clipboard chunk — increase if the first character is wrong |
| `non-ascii-default-delay` | `0.025` | Seconds to wait before subsequent non-ASCII clipboard chunks |
| `silence-threshold` | `-50` | Max volume in dB to consider a recording silent (e.g. `-50` means anything quieter is discarded) |
| `silence-percentage` | `95` | Percentage of the recording that must be below `silence-threshold` to be considered silent |

## Troubleshooting

**Terminal Applications**: Clipboard paste uses Ctrl+V, which doesn't work in terminal emulators (they require Ctrl+Shift+V). Temporary workaround is to remap Ctrl+V to paste in your terminal emulator's settings. Note that *this limitation only affects international/Unicode characters*. ASCII characters (a-z, A-Z, 0-9, punctuation) are typed directly and are unaffected.

**Non-ASCII characters come out wrong**: Increase `non-ascii-initial-delay` (and `non-ascii-default-delay`) to give the Wayland compositor more time to process the clipboard update before the paste keystroke arrives.

**Clipboard content is lost after dictation**: This should not happen — xhisper saves and restores your clipboard around any non-ASCII paste operations. If you see this, please open an issue.

---

## Development

Run the test suite:

```sh
make check
```

This compiles and runs the C unit tests (`tests/test_xhispertool.c`) followed by the shell tests (`tests/test_paste.sh`). No external test framework is required.

---

<p align="center">
  <em>Low complexity dictation for Linux</em>
</p>
