#!/bin/bash
# setup_mt5_compiler.sh — Install MetaTrader 5 + metaeditor64.exe via Wine for macOS
# Usage: bash scripts/setup_mt5_compiler.sh [wine_prefix]
set -e

WINE_PREFIX="${1:-$HOME/.wine}"
MT5_DIR="$WINE_PREFIX/drive_c/Program Files/MetaTrader 5"
METAEDITOR="$MT5_DIR/metaeditor64.exe"
EXPERTS_DIR="$MT5_DIR/MQL5/Experts"

echo "=== MT5 Compiler Setup for macOS ==="
echo "Wine prefix: $WINE_PREFIX"
echo ""

# Check Wine
if ! command -v wine &>/dev/null; then
    echo "Installing Wine..."
    if command -v brew &>/dev/null; then
        brew install wine-stable || brew install --cask wine-stable
    else
        echo "ERROR: Homebrew not found. Install Wine manually: https://wiki.winehq.org/MacOS"
        exit 1
    fi
fi

# Initialize Wine prefix if needed
if [ ! -f "$WINE_PREFIX/system.reg" ]; then
    echo "Initializing Wine prefix..."
    WINEPREFIX="$WINE_PREFIX" wineboot -u 2>/dev/null || true
fi

# Check if MT5 already installed
if [ -f "$METAEDITOR" ]; then
    echo "MetaTrader 5 already installed at: $METAEDITOR"
else
    echo "Downloading MetaTrader 5..."
    MT5_INSTALLER="/tmp/mt5setup.exe"
    curl -L -o "$MT5_INSTALLER" "https://download.mql5.com/cdn/web/metaquotes.software.corp/mt5/mt5setup.exe" 2>&1 | tail -5
    
    if [ ! -f "$MT5_INSTALLER" ]; then
        echo "ERROR: Download failed. Trying alternative URL..."
        curl -L -o "$MT5_INSTALLER" "https://www.metatrader5.com/en/terminal/help/start" 2>&1 | tail -3 || true
        if [ ! -f "$MT5_INSTALLER" ]; then
            echo "Download failed. Manually download from: https://www.metatrader5.com/en/download"
            echo "Then run: wine /path/to/mt5setup.exe"
            exit 1
        fi
    fi

    echo "Installing MetaTrader 5 via Wine (silent mode)..."
    WINEPREFIX="$WINE_PREFIX" wine "$MT5_INSTALLER" /auto 2>&1 | tail -5 || true
    
    # Wait for installation to complete
    echo "Waiting for installation to finish..."
    for i in $(seq 1 30); do
        if [ -f "$METAEDITOR" ]; then
            echo "MetaTrader 5 installed successfully!"
            break
        fi
        sleep 2
    done
    
    rm -f "$MT5_INSTALLER"
fi

# Verify metaeditor64.exe
if [ -f "$METAEDITOR" ]; then
    echo ""
    echo "=== Compiler found ==="
    echo "Path: $METAEDITOR"
    echo "Size: $(du -h "$METAEDITOR" | cut -f1)"
    wine "$METAEDITOR" /? 2>&1 | head -5 || true
else
    echo "WARNING: metaeditor64.exe not found at $METAEDITOR"
    echo "You may need to install MT5 manually via Wine."
fi

# Create Experts directory
mkdir -p "$EXPERTS_DIR"
echo "Experts dir: $EXPERTS_DIR"

# Generate .env config
echo ""
echo "=== Environment Variables ==="
echo "Add to your backend/.env:"
echo "MT5_METAEDITOR_PATH=$METAEDITOR"
echo "MT5_EXPERTS_DIR=$EXPERTS_DIR"
echo ""
echo "Or export them before running the backend:"
echo "  export MT5_METAEDITOR_PATH=\"$METAEDITOR\""
echo "  export MT5_EXPERTS_DIR=\"$EXPERTS_DIR\""
echo "  cd backend && python3.11 run.py"
echo ""
echo "=== Setup Complete ==="
