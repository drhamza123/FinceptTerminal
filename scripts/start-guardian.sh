#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BACKEND_DIR="$SCRIPT_DIR/backend"
APP_BUNDLE="$SCRIPT_DIR/fincept-qt/build/macos-release/FinceptTerminal.app"
APP_BINARY="$APP_BUNDLE/Contents/MacOS/FinceptTerminal"
PORT=${PORT:-8150}

echo "============================================"
echo "  AI Stock Guardian — Local Launch"
echo "============================================"
echo ""

# 1. Configure Qt app to use local backend
echo "[1/4] Pointing Qt app to http://localhost:$PORT ..."
# QSettings("Guardian", "AIStockGuardian") on macOS writes to Guardian.AIStockGuardian
defaults write Guardian.AIStockGuardian "api/base_url" "http://localhost:$PORT" 2>/dev/null || true
# Also try bundle ID (alternate QSettings behavior)
defaults write com.guardian.aistockguardian "api/base_url" "http://localhost:$PORT" 2>/dev/null || true
echo "      Done."

# 2. Kill any existing backend on the port
echo "[2/4] Starting backend server on port $PORT ..."
lsof -ti:$PORT 2>/dev/null | xargs kill -9 2>/dev/null || true
sleep 1

cd "$BACKEND_DIR"
source .env 2>/dev/null || true
python3.11 run.py &
BACKEND_PID=$!
echo "      Backend PID: $BACKEND_PID"

# Wait for backend to be ready
echo "      Waiting for backend..."
for i in $(seq 1 30); do
    if curl -s http://localhost:$PORT/ > /dev/null 2>&1; then
        echo "      Backend is ready."
        break
    fi
    sleep 1
done

# 3. Verify the app bundle exists
echo "[3/4] Launching AI Stock Guardian Qt app..."
if [ ! -f "$APP_BINARY" ]; then
    echo "      ERROR: App binary not found at $APP_BINARY"
    echo "      Build it first with: cd fincept-qt && cmake --preset macos-release && cmake --build --preset macos-release"
    kill $BACKEND_PID 2>/dev/null
    exit 1
fi

# 4. Launch the app
echo "[4/4] Opening $APP_BUNDLE ..."
open "$APP_BUNDLE"

echo ""
echo "============================================"
echo "  Backend:  http://localhost:$PORT"
echo "  API Docs: http://localhost:$PORT/docs"
echo "  App:      AI Stock Guardian"
echo "============================================"
echo ""
echo "Press Ctrl+C to stop the backend (app continues running)."

# Trap Ctrl+C to clean up backend
cleanup() {
    echo ""
    echo "Shutting down backend..."
    kill $BACKEND_PID 2>/dev/null
    wait $BACKEND_PID 2>/dev/null
    echo "Backend stopped."
}
trap cleanup INT TERM

# Keep script running until Ctrl+C
wait $BACKEND_PID
