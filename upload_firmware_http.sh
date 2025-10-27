#!/bin/bash
# Script upload firmware qua HTTP (không cần SSH)

echo "📤 Upload Firmware qua HTTP (Không cần SSH)"
echo "=========================================="

# Cấu hình
BASE_URL="http://cloud.anhnguyxn.io.vn:8000"
API_KEY="battery_monitor_2024_secure_key"

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
echo "🌐 Server: $BASE_URL"
echo ""

# Kiểm tra kết nối server
echo "🔍 Kiểm tra kết nối server..."
if ! curl -s --connect-timeout 10 "$BASE_URL/" > /dev/null; then
    echo "❌ Không thể kết nối đến server: $BASE_URL"
    echo "   Kiểm tra lại URL và kết nối mạng"
    exit 1
fi
echo "✅ Server accessible"

# Upload firmware
echo "🔄 Đang upload firmware..."
response=$(curl -s -X POST "$BASE_URL/api/firmware/upload" \
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
    echo "curl \"$BASE_URL/api/firmware/check?current_version=1.0.0-build1&device_id=battery_monitor_001\""
else
    echo ""
    echo "❌ Upload thất bại!"
    echo "Backend chưa có firmware endpoints. Cần restart backend với code mới."
    echo ""
    echo "🛠️ Giải pháp:"
    echo "1. Restart backend trên server với code mới"
    echo "2. Hoặc sử dụng phương pháp upload trực tiếp"
    echo ""
    echo "📋 Backend cần có các endpoints:"
    echo "  - POST /api/firmware/upload"
    echo "  - GET /api/firmware/check"
    echo "  - GET /api/firmware/download/{version}"
    echo "  - GET /api/firmware/info"
    echo ""
    echo "🔧 Commands để restart backend:"
    echo "  ssh root@cloud.anhnguyxn.io.vn"
    echo "  docker stop battery-backend && docker rm battery-backend"
    echo "  docker build -t battery-backend ./backend"
    echo "  docker run -d --name battery-backend -p 8000:8000 -e BATTERY_API_KEY=\"battery_monitor_2024_secure_key\" battery-backend"
    exit 1
fi