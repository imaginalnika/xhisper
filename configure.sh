#!/bin/bash

# configure.sh - Interactive configuration for xhisper

CONFIG_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/xhisper"
CONFIG_FILE="$CONFIG_DIR/xhisperrc"

echo "=== xhisper configuration ==="

# 1. Create config directory
mkdir -p "$CONFIG_DIR"

# 2. Copy default config if it doesn't exist
if [ ! -f "$CONFIG_FILE" ]; then
    cp default_xhisperrc "$CONFIG_FILE"
    echo "Created default configuration at $CONFIG_FILE"
else
    echo "Configuration file already exists at $CONFIG_FILE"
fi

# 3. Check for permissions and groups
echo ""
echo "Checking permissions..."

# Check if /dev/uinput exists
if [ ! -e /dev/uinput ]; then
    echo "Error: /dev/uinput does not exist."
    echo "You may need to load the uinput kernel module: 'sudo modprobe uinput'"
else
    # Check if user has write access to /dev/uinput
    if [ ! -w /dev/uinput ]; then
        echo "Error: You do not have write access to /dev/uinput."
        
        # Check if user is in 'input' group
        if groups | grep -q "\binput\b"; then
            echo "You are in the 'input' group, but still lack access."
            echo "Try checking the permissions of /dev/uinput: 'ls -l /dev/uinput'"
        else
            echo "You are NOT in the 'input' group."
            echo "Please run: 'sudo usermod -aG input \$USER' and then LOG OUT and LOG IN again."
        fi
    else
        echo "Write access to /dev/uinput verified."
    fi
fi

# 4. Check for dependencies
echo ""
echo "Checking dependencies..."
deps=("pw-record" "ffprobe" "ffmpeg" "curl" "jq" "bc")
missing=()

for dep in "${deps[@]}"; do
    if ! command -v "$dep" &> /dev/null; then
        missing+=("$dep")
    fi
done

# Check clipboard
if ! command -v wl-copy &> /dev/null && ! command -v xclip &> /dev/null; then
    missing+=("wl-clipboard or xclip")
fi

if [ ${#missing[@]} -ne 0 ]; then
    echo "Warning: Missing dependencies: ${missing[*]}"
    echo "Please install them using your package manager."
else
    echo "All dependencies found."
fi

# 4. Check for GROQ_API_KEY
echo ""
if [ -z "$GROQ_API_KEY" ]; then
    if [ -f "$HOME/.env" ] && grep -q "GROQ_API_KEY" "$HOME/.env"; then
        echo "GROQ_API_KEY found in $HOME/.env"
    else
        echo "GROQ_API_KEY is not set."
        echo "You can get one at https://console.groq.com/keys"
        read -p "Enter your Groq API Key (optional, press Enter to skip): " key
        if [ -n "$key" ]; then
            echo "GROQ_API_KEY=$key" >> "$HOME/.env"
            echo "Saved key to $HOME/.env"
        fi
    fi
else
    echo "GROQ_API_KEY is already set in your environment."
fi

echo ""
echo "Configuration complete. You can now run 'sudo make install'."
