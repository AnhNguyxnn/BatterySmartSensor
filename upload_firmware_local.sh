#!/bin/bash
# Script upload firmware từ máy local lên backend server

echo "🔧 Battery Monitor Firmware Upload Tool (Local to Server)"
echo "========================================================"

# Kiểm tra tham số
if [ $# -lt 3 ]; then
    echo "Usage: $0 <firmware_file.bin> <version> <server_url> [build_number] [release_notes]"
    echo ""
    echo "Examples:"
    echo "  $0 firmware.bin 1.0.1 http://cloud.anhnguyxn.io.vn:8000"
    echo "  $0 firmware.bin 1.0.1 http://cloud.anhnguyxn.io.vn:8000 2"
    echo "  $0 firmware.bin 1.0.1 http://cloud.anhnguyxn.io.vn:8000 2 \"Bug fixes\""
    echo ""
    echo "Environment variables:"
    echo "  BATTERY_API_KEY - API key for authentication"
    echo "  BACKEND_URL - Default backend URL"
    exit 1
fi

FIRMWARE_FILE="$1"
VERSION="$2"
SERVER_URL="$3"
BUILD="${4:-1}"
RELEASE_NOTES="${5:-Bug fixes and improvements}"

# Sử dụng environment variables nếu có
API_KEY="${BATTERY_API_KEY:-battery_monitor_2024_secure_key}"
BACKEND_URL="${BACKEND_URL:-$SERVER_URL}"

# Kiểm tra file firmware
if [ ! -f "$FIRMWARE_FILE" ]; then
    echo "❌ File firmware không tồn tại: $FIRMWARE_FILE"
    exit 1
fi

if [[ ! "$FIRMWARE_FILE" == *.bin ]]; then
    echo "❌ File phải có extension .bin"
    exit 1
fi

# Kiểm tra curl
if ! command -v curl &> /dev/null; then
    echo "❌ curl không được cài đặt. Vui lòng cài đặt curl:"
    echo "   Ubuntu/Debian: sudo apt install curl"
    echo "   CentOS/RHEL: sudo yum install curl"
    echo "   macOS: brew install curl"
    exit 1
fi

echo "📁 Firmware file: $FIRMWARE_FILE"
echo "📱 Version: $VERSION"
echo "🔢 Build: $BUILD"
echo "📝 Release notes: $RELEASE_NOTES"
echo "🌐 Server URL: $BACKEND_URL"
echo "🔑 API Key: ${API_KEY:0:8}...${API_KEY: -8}"
echo ""

# Kiểm tra kết nối server
echo "🔍 Kiểm tra kết nối server..."
if ! curl -s --connect-timeout 10 "$BACKEND_URL/api/firmware/info" > /dev/null; then
    echo "❌ Không thể kết nối đến server: $BACKEND_URL"
    echo "   Kiểm tra lại URL và kết nối mạng"
    exit 1
fi
echo "✅ Server accessible"

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
    echo ""
    echo "📊 Firmware info:"
    curl -s "$BACKEND_URL/api/firmware/info" | python3 -m json.tool 2>/dev/null || echo "$(curl -s "$BACKEND_URL/api/firmware/info")"
else
    echo ""
    echo "❌ Upload thất bại!"
    echo "Kiểm tra lại:"
    echo "  - File firmware có đúng format không"
    echo "  - API key có đúng không"
    echo "  - Server có hoạt động không"
    echo "  - Kết nối mạng có ổn định không"
    exit 1
fi
