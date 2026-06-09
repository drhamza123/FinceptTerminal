#!/bin/bash
# ============================================================
# Compile & Deploy GuardianBridge EA via Wine on macOS
# Requires: Wine ($ brew install --cask wine-stable)
#           MetaTrader 5 installed in Wine prefix
# ============================================================
set -e

EA_NAME="${1:-GuardianBridge}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_FILE="$SCRIPT_DIR/../backend/mql5/$EA_NAME/$EA_NAME.mq5"

# Default Wine MT5 paths. MetaTrader's macOS package uses its own Wine prefix,
# while manual Wine installs usually live under ~/.wine.
WINE_PREFIX="${WINEPREFIX:-$HOME/Library/Application Support/net.metaquotes.wine.metatrader5}"
if [ ! -d "$WINE_PREFIX" ]; then
    WINE_PREFIX="$HOME/.wine"
fi
MT5_DIR="$WINE_PREFIX/drive_c/Program Files/MetaTrader 5"
METAEDITOR="$MT5_DIR/metaeditor64.exe"
EXPERTS_DIR="$MT5_DIR/MQL5/Experts"

# Check source exists
if [ ! -f "$SRC_FILE" ]; then
    echo "ERROR: Source not found at $SRC_FILE"
    exit 1
fi

# Check Wine
if ! command -v wine &>/dev/null; then
    echo "ERROR: Wine not found. Install it first:"
    echo "  brew install --cask wine-stable"
    exit 1
fi

# Check metaeditor
if [ ! -f "$METAEDITOR" ]; then
    echo "Searching for metaeditor64.exe..."
    METAEDITOR=$(find "$HOME/Library/Application Support/net.metaquotes.wine.metatrader5" "$HOME/.wine" -name "metaeditor64.exe" -o -name "metaeditor.exe" 2>/dev/null | head -1)
    if [ -z "$METAEDITOR" ]; then
        echo "ERROR: metaeditor64.exe not found. Is MT5 installed in Wine?"
        exit 1
    fi
    MT5_DIR="$(cd "$(dirname "$METAEDITOR")" && pwd)"
    EXPERTS_DIR="$MT5_DIR/MQL5/Experts"
fi
echo "MetaEditor: $METAEDITOR"

# Create temp dir
TMP_DIR=$(mktemp -d)
cp "$SRC_FILE" "$TMP_DIR/$EA_NAME.mq5"

# Compile
echo "Compiling $EA_NAME..."
# Convert path to Wine path
SRC_WINE="Z:$(printf '%s/%s.mq5' "$TMP_DIR" "$EA_NAME" | sed 's|/|\\|g')"
LOG_WINE="Z:$(printf '%s/compile.log' "$TMP_DIR" | sed 's|/|\\|g')"
wine "$METAEDITOR" "/compile:$SRC_WINE" "/log:$LOG_WINE" 2>/dev/null || true
cat "$TMP_DIR/compile.log" 2>/dev/null || true

# Deploy
if [ -f "$TMP_DIR/$EA_NAME.ex5" ]; then
    mkdir -p "$EXPERTS_DIR"
    cp "$TMP_DIR/$EA_NAME.ex5" "$EXPERTS_DIR/$EA_NAME.ex5"
    echo ""
    echo "SUCCESS: Deployed to $EXPERTS_DIR/$EA_NAME.ex5"
    echo ""
    echo "Next steps:"
    echo "1. In MetaTrader 5 (via Wine): open Navigator > Expert Advisors"
    echo "2. Drag GuardianBridge onto your chart"
    echo "3. Verify InpServerHost=127.0.0.1 and InpServerPort=5556"
    echo "4. Backend must be running (port 8150) with TCP on 5556"
else
    echo "ERROR: Compilation failed - check the log above"
fi

rm -rf "$TMP_DIR"
