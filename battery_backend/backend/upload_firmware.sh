#!/bin/bash
# Script để upload firmware lên backend

echo "🔧 Battery Monitor Firmware Upload Tool"
echo "======================================"

# Kiểm tra tham số
if [ $# -lt 2 ]; then
    echo "Usage: $0 <firmware_file.bin> <version> [build_number] [release_notes]"
    echo ""
    echo "Examples:"
    echo "  $0 firmware.bin 1.0.1"
    echo "  $0 firmware.bin 1.0.1 2"
    echo "  $0 firmware.bin 1.0.1 2 \"Bug fixes and improvements\""
    exit 1
fi

FIRMWARE_FILE="$1"
VERSION="$2"
BUILD="${3:-1}"
RELEASE_NOTES="${4:-Bug fixes and improvements}"
BACKEND_URL="http://cloud.anhnguyxn.io.vn:8000"
API_KEY="battery_monitor_2024_secure_key"

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
echo "🌐 Backend URL: $BACKEND_URL"
echo ""

# Upload firmware
echo "🔄 Đang upload firmware..."
response=$(curl -s -X POST "$BACKEND_URL/api/firmware/upload" \
    -H "X-API-Key: $API_KEY" \
    -F "file=@$FIRMWARE_FILE" \
    -F "version=$VERSION" \
    -F "build=$BUILD" \
    -F "release_notes=$RELEASE_NOTES")

echo "📤 Response: $response"

# Kiểm tra kết quả
if echo "$response" | grep -q '"status":"success"'; then
    echo ""
    echo "✅ Upload thành công!"
    echo "📱 ESP32 sẽ tự động phát hiện firmware mới trong lần check tiếp theo"
    echo ""
    echo "🔍 Test firmware check:"
    echo "curl \"$BACKEND_URL/api/firmware/check?current_version=1.0.0-build1&device_id=battery_monitor_001\""
else
    echo ""
    echo "❌ Upload thất bại!"
    echo "Kiểm tra lại file firmware và kết nối mạng"
    exit 1
fi
