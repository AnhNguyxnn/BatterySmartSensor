#!/bin/bash
# Script upload firmware trực tiếp lên server (không cần backend)

echo "📤 Upload Firmware trực tiếp lên Server"
echo "======================================"

# Cấu hình
SERVER_HOST="cloud.anhnguyxn.io.vn"
SERVER_USER="root"
FIRMWARE_DIR="/root/backend/firmware"

# Kiểm tra tham số
if [ $# -lt 2 ]; then
    echo "Usage: $0 <firmware_file.bin> <version> [build_number] [release_notes]"
    echo ""
    echo "Examples:"
    echo "  $0 firmware.bin 1.0.1"
    echo "  $0 firmware.bin 1.0.1 2"
    echo "  $0 firmware.bin 1.0.1 2 \"Bug fixes\""
    exit 1
fi

FIRMWARE_FILE="$1"
VERSION="$2"
BUILD="${3:-1}"
RELEASE_NOTES="${4:-Bug fixes and improvements}"

# Kiểm tra file firmware
if [ ! -f "$FIRMWARE_FILE" ]; then
    echo "❌ File firmware không tồn tại: $FIRMWARE_FILE"
    exit 1
fi

if [[ ! "$FIRMWARE_FILE" == *.bin ]]; then
    echo "❌ File phải có extension .bin"
    exit 1
fi

echo "📁 Firmware file: $FIRMWARE_FILE"
echo "📱 Version: $VERSION"
echo "🔢 Build: $BUILD"
echo "📝 Release notes: $RELEASE_NOTES"
echo "🌐 Server: $SERVER_HOST"
echo "📂 Directory: $FIRMWARE_DIR"
echo ""

# Upload file lên server
echo "🔄 Uploading firmware to server..."
scp "$FIRMWARE_FILE" "$SERVER_USER@$SERVER_HOST:$FIRMWARE_DIR/battery_monitor_v$VERSION.bin"

if [ $? -eq 0 ]; then
    echo "✅ Firmware uploaded successfully!"
    
    # Tạo firmware info file
    echo "📝 Creating firmware info..."
    cat > firmware_info.json << EOF
{
  "version": "$VERSION",
  "build": $BUILD,
  "download_url": "/api/firmware/download/$VERSION",
  "file_size": $(stat -c%s "$FIRMWARE_FILE"),
  "release_notes": "$RELEASE_NOTES",
  "checksum": "$(md5sum "$FIRMWARE_FILE" | cut -d' ' -f1)",
  "uploaded_at": "$(date -Iseconds)",
  "filename": "battery_monitor_v$VERSION.bin"
}
EOF
    
    # Upload firmware info
    scp firmware_info.json "$SERVER_USER@$SERVER_HOST:$FIRMWARE_DIR/latest.json"
    
    if [ $? -eq 0 ]; then
        echo "✅ Firmware info uploaded successfully!"
        echo ""
        echo "🎯 ESP32 sẽ tự động phát hiện firmware mới trong lần check tiếp theo"
        echo ""
        echo "🔍 Test firmware check:"
        echo "curl \"http://$SERVER_HOST:8000/api/firmware/check?current_version=1.0.0-build1&device_id=battery_monitor_001\""
        echo ""
        echo "📊 Firmware info:"
        cat firmware_info.json
    else
        echo "❌ Failed to upload firmware info"
        exit 1
    fi
    
    # Cleanup
    rm -f firmware_info.json
else
    echo "❌ Failed to upload firmware"
    exit 1
fi