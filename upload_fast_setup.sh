#!/bin/bash

echo "⚡ Uploading FAST SETUP Firmware for ESP32 Battery Monitor"
echo "========================================================"

# Upload firmware với fast setup optimizations
echo "📤 Uploading firmware with fast setup optimizations..."
pio run --target upload

if [ $? -eq 0 ]; then
    echo "✅ Firmware upload thành công!"
    echo ""
    echo "⚡ Các tối ưu hóa tốc độ đã được áp dụng:"
    echo "   • Giảm timeout cellular từ 60s xuống 20s"
    echo "   • Giảm số lần retry từ 3-5 xuống 2-3"
    echo "   • Giảm delay giữa các lần thử"
    echo "   • Giảm timeout NTP sync từ 20s xuống 2.5s"
    echo "   • Giảm delay setup từ 3s xuống 1s"
    echo "   • Bỏ qua test cảm biến trong fast mode"
    echo "   • Bỏ qua firmware check nếu chưa có kết nối"
    echo "   • Cải thiện fallback timestamp với offset"
    echo ""
    echo "🕐 Cải thiện thời gian:"
    echo "   • Fallback timestamp có offset từ 2020"
    echo "   • Kiểm tra timestamp hợp lệ (2020-2030)"
    echo "   • Sync NTP nhanh hơn với timeout ngắn"
    echo "   • Force sync chỉ 2 lần thử thay vì 3"
    echo ""
    echo "📱 Monitor serial để xem tốc độ setup:"
    echo "   pio device monitor"
    echo ""
    echo "🌐 Truy cập web interface: http://192.168.4.1"
    echo "🕐 Nhấn 'Sync Time' để đồng bộ thời gian nếu cần"
    echo ""
    echo "⏱️ Thời gian setup dự kiến:"
    echo "   • WiFi mode: ~10-15 giây"
    echo "   • 4G mode: ~15-25 giây"
    echo "   • AP only mode: ~5 giây"
else
    echo "❌ Firmware upload thất bại!"
    echo "Kiểm tra kết nối USB và thiết bị ESP32"
    exit 1
fi
