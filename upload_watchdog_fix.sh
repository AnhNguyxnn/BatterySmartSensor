#!/bin/bash

# Watchdog Fix Upload Script
# This script compiles and uploads the watchdog timeout fix to ESP32

set -e

PIO_PATH="/home/anhnguyen/.platformio/penv/bin/pio"
UPLOAD_PORT="${1:-/dev/ttyUSB0}"

echo "╔══════════════════════════════════════════════════════╗"
echo "║   ESP32 Battery Smart Sensor - Watchdog Fix Upload   ║"
echo "╚══════════════════════════════════════════════════════╝"
echo ""

# Check if platformio is available
if [ ! -f "$PIO_PATH" ]; then
    echo "❌ Error: PlatformIO not found at $PIO_PATH"
    echo "Please check your PlatformIO installation."
    exit 1
fi

echo "📋 Using upload port: $UPLOAD_PORT"
echo ""

# Step 1: Compile
echo "🔨 Step 1: Compiling firmware..."
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
if $PIO_PATH run -e esp32dev; then
    echo "✅ Compilation successful!"
    echo ""
else
    echo "❌ Compilation failed!"
    exit 1
fi

# Step 2: Upload
echo "📤 Step 2: Uploading firmware to ESP32..."
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "⚠️  Make sure:"
echo "   - ESP32 is connected to $UPLOAD_PORT"
echo "   - Serial monitor is NOT running"
echo "   - Device is not in use"
echo ""
read -p "Ready to upload? (y/n) " -n 1 -r
echo ""
if [[ $REPLY =~ ^[Yy]$ ]]; then
    if $PIO_PATH run -e esp32dev --target upload --upload-port "$UPLOAD_PORT"; then
        echo ""
        echo "✅ Upload successful!"
        echo ""
        echo "╔══════════════════════════════════════════════════════╗"
        echo "║              Upload Completed Successfully!          ║"
        echo "╚══════════════════════════════════════════════════════╝"
        echo ""
        echo "📺 To verify the fix, start the serial monitor:"
        echo "   $PIO_PATH device monitor -e esp32dev"
        echo ""
        echo "Watch for these signs of successful fix:"
        echo "   ✓ Device boots without errors (~5 seconds)"
        echo "   ✓ Network initialization completes"
        echo "   ✓ No 'task_wdt' error after ~125 seconds"
        echo "   ✓ Stable operation (no reboots)"
    else
        echo "❌ Upload failed!"
        echo "Troubleshooting:"
        echo "   1. Check the USB cable connection"
        echo "   2. Verify the port is correct: $UPLOAD_PORT"
        echo "   3. Check device permissions: ls -la $UPLOAD_PORT"
        echo "   4. Try: sudo chmod 666 $UPLOAD_PORT"
        exit 1
    fi
else
    echo "⚠️  Upload cancelled."
    exit 0
fi
