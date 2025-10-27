#!/bin/bash
# Script tạo firmware OTA update hoàn chỉnh

echo "🚀 Tạo Firmware OTA Update System"
echo "================================="

# Cấu hình
BASE_URL="http://cloud.anhnguyxn.io.vn:8000"
API_KEY="battery_monitor_2024_secure_key"
FIRMWARE_VERSION="1.0.1"
BUILD_NUMBER="1"
RELEASE_NOTES="OTA Update System - Bug fixes and improvements"

echo "📱 Firmware Version: $FIRMWARE_VERSION"
echo "🔢 Build Number: $BUILD_NUMBER"
echo "📝 Release Notes: $RELEASE_NOTES"
echo "🌐 Backend URL: $BASE_URL"
echo ""

# Tạo firmware file giả để test
echo "📁 Creating test firmware file..."
cat > firmware.bin << EOF
This is a test firmware file for OTA update system.
Version: $FIRMWARE_VERSION
Build: $BUILD_NUMBER
Release Notes: $RELEASE_NOTES
Created: $(date)
EOF

echo "✅ Test firmware file created"

# Upload firmware lên backend
echo "🔄 Uploading firmware to backend..."
response=$(curl -s -X POST "$BASE_URL/api/firmware/upload" \
    -H "X-API-Key: $API_KEY" \
    -F "file=@firmware.bin" \
    -F "version=$FIRMWARE_VERSION" \
    -F "build=$BUILD_NUMBER" \
    -F "release_notes=$RELEASE_NOTES")

echo "📤 Upload Response: $response"

# Kiểm tra kết quả upload
if echo "$response" | grep -q '"status":"success"'; then
    echo "✅ Firmware uploaded successfully!"
    
    # Test firmware info
    echo ""
    echo "🔍 Testing firmware info..."
    curl -s "$BASE_URL/api/firmware/info" | python3 -m json.tool
    
    # Test firmware check
    echo ""
    echo "🔍 Testing firmware check..."
    curl -s "$BASE_URL/api/firmware/check?current_version=1.0.0-build1&device_id=battery_monitor_001" | python3 -m json.tool
    
    # Test firmware download
    echo ""
    echo "🔍 Testing firmware download..."
    curl -s -o downloaded_firmware.bin "$BASE_URL/api/firmware/download/$FIRMWARE_VERSION"
    
    if [ -f "downloaded_firmware.bin" ]; then
        echo "✅ Firmware download successful!"
        echo "📊 Downloaded file size: $(stat -c%s downloaded_firmware.bin) bytes"
        echo "📊 Original file size: $(stat -c%s firmware.bin) bytes"
        
        # So sánh checksum
        original_checksum=$(md5sum firmware.bin | cut -d' ' -f1)
        downloaded_checksum=$(md5sum downloaded_firmware.bin | cut -d' ' -f1)
        
        if [ "$original_checksum" = "$downloaded_checksum" ]; then
            echo "✅ Checksum verification successful!"
        else
            echo "❌ Checksum verification failed!"
        fi
        
        # Cleanup
        rm -f downloaded_firmware.bin
    else
        echo "❌ Firmware download failed!"
    fi
    
else
    echo "❌ Firmware upload failed!"
    echo "Response: $response"
fi

# Cleanup
rm -f firmware.bin

echo ""
echo "🎯 OTA Update System Test Completed!"
echo ""
echo "📋 Next Steps:"
echo "1. ESP32 sẽ tự động check firmware update"
echo "2. Nếu có update, ESP32 sẽ download và install"
echo "3. ESP32 sẽ restart với firmware mới"
echo "4. Monitor logs để xem quá trình update"
