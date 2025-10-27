#!/bin/bash
# Script test ESP32 firmware update

echo "🧪 Test ESP32 Firmware Update"
echo "============================"

# Cấu hình
BASE_URL="http://cloud.anhnguyxn.io.vn:8000"
API_KEY="battery_monitor_2024_secure_key"
CURRENT_VERSION="1.0.0-build1"
DEVICE_ID="battery_monitor_001"

echo "📱 Current Version: $CURRENT_VERSION"
echo "🆔 Device ID: $DEVICE_ID"
echo "🌐 Backend URL: $BASE_URL"
echo ""

# Test firmware check
echo "🔍 Testing firmware check..."
response=$(curl -s "$BASE_URL/api/firmware/check?current_version=$CURRENT_VERSION&device_id=$DEVICE_ID")
echo "📤 Response: $response"

# Parse response
if echo "$response" | grep -q '"update_available":true'; then
    echo "✅ Firmware update available!"
    
    # Extract version info
    latest_version=$(echo "$response" | python3 -c "import json, sys; data=json.load(sys.stdin); print(data.get('latest_version', 'N/A'))")
    download_url=$(echo "$response" | python3 -c "import json, sys; data=json.load(sys.stdin); print(data.get('download_url', 'N/A'))")
    
    echo "📱 Latest Version: $latest_version"
    echo "📥 Download URL: $download_url"
    
    # Test download
    echo ""
    echo "🔍 Testing firmware download..."
    curl -s -o test_firmware.bin "$BASE_URL$download_url"
    
    if [ -f "test_firmware.bin" ]; then
        echo "✅ Firmware download successful!"
        echo "📊 File size: $(stat -c%s test_firmware.bin) bytes"
        rm -f test_firmware.bin
    else
        echo "❌ Firmware download failed!"
    fi
    
else
    echo "ℹ️ No firmware update available"
fi

echo ""
echo "🔍 Testing ESP32 firmware check process..."
echo "========================================="

echo "1. ESP32 check firmware update mỗi FIRMWARE_CHECK_INTERVAL"
echo "2. ESP32 gửi request đến: $BASE_URL/api/firmware/check"
echo "3. ESP32 nhận response và parse JSON"
echo "4. Nếu có update, ESP32 tạo notification AP"
echo "5. ESP32 download và install firmware mới"

echo ""
echo "🔧 ESP32 Code Flow:"
echo "=================="
echo "setup() -> checkFirmwareUpdate()"
echo "loop() -> checkFirmwareUpdate() (mỗi FIRMWARE_CHECK_INTERVAL)"
echo "checkFirmwareUpdate() -> HTTP GET -> Parse JSON -> Start AP nếu có update"

echo ""
echo "🚨 Possible Issues:"
echo "=================="
echo "1. ESP32 chưa có code firmware update"
echo "2. ESP32 không thể kết nối WiFi"
echo "3. ESP32 không thể kết nối backend"
echo "4. Backend không trả về đúng response"
echo "5. ESP32 code chưa được upload"

echo ""
echo "🔧 Debug Steps:"
echo "=============="
echo "1. Kiểm tra ESP32 Serial Monitor"
echo "2. Kiểm tra ESP32 WiFi connection"
echo "3. Kiểm tra ESP32 backend connection"
echo "4. Kiểm tra ESP32 firmware update code"
echo "5. Test firmware update manually"

echo ""
echo "✅ Test completed!"
