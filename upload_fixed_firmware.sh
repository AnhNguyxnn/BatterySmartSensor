#!/bin/bash

echo "🚀 Uploading Fixed Firmware for ESP32 Battery Monitor"
echo "=================================================="

# Kiểm tra PlatformIO
if ! command -v pio &> /dev/null; then
    echo "❌ PlatformIO không được cài đặt!"
    echo "Cài đặt: pip install platformio"
    exit 1
fi

# Kiểm tra thiết bị ESP32
echo "🔍 Kiểm tra thiết bị ESP32..."
pio device list

# Upload firmware
echo "📤 Uploading firmware..."
pio run --target upload

if [ $? -eq 0 ]; then
    echo "✅ Upload thành công!"
    echo ""
    echo "🔧 Các cải tiến đã được áp dụng:"
    echo "   • Tăng watchdog timeout từ 30s lên 60s"
    echo "   • Thêm nhiều điểm reset watchdog trong loop"
    echo "   • Cải thiện retry logic cho kết nối 4G"
    echo "   • Thêm hàm reset modem khi gặp lỗi"
    echo "   • Tăng delay và timeout cho các thao tác modem"
    echo "   • Thêm logic tự động reset modem khi upload thất bại"
    echo ""
    echo "📱 Monitor serial để xem kết quả:"
    echo "   pio device monitor"
else
    echo "❌ Upload thất bại!"
    echo "Kiểm tra kết nối USB và thiết bị ESP32"
    exit 1
fi
