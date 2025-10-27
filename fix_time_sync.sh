#!/bin/bash

echo "🕐 Fixing Time Sync Issues for ESP32 Battery Monitor"
echo "=================================================="

# Upload firmware với time sync fixes
echo "📤 Uploading firmware with time sync fixes..."
pio run --target upload

if [ $? -eq 0 ]; then
    echo "✅ Firmware upload thành công!"
    echo ""
    echo "🔧 Các cải tiến về thời gian đã được áp dụng:"
    echo "   • Sync NTP cho cả WiFi và 4G"
    echo "   • Kiểm tra timestamp hợp lệ (2020-2030)"
    echo "   • Force sync với retry logic (3 lần thử)"
    echo "   • Fallback về millis() nếu sync thất bại"
    echo "   • Thêm endpoint /api/time-sync để sync từ web"
    echo "   • Cải thiện logging timestamp"
    echo ""
    echo "🔄 Restarting backend để áp dụng template fixes..."
    
    # Restart backend
    cd backend
    docker-compose restart
    
    if [ $? -eq 0 ]; then
        echo "✅ Backend đã được restart!"
        echo ""
        echo "🌐 Backend fixes:"
        echo "   • Thêm filter timestamp_to_datetime"
        echo "   • Hiển thị Device Time thay vì ESP32 Time"
        echo "   • Convert timestamp thành datetime đọc được"
        echo "   • Hiển thị cả Server Time và Device Time"
        echo ""
        echo "📱 Truy cập dashboard: http://192.168.4.1"
        echo "🕐 Nhấn nút 'Sync Time' để đồng bộ thời gian"
        echo "📊 Monitor serial: pio device monitor"
    else
        echo "❌ Không thể restart backend!"
        echo "Thử: cd backend && docker-compose restart"
    fi
else
    echo "❌ Firmware upload thất bại!"
    echo "Kiểm tra kết nối USB và thiết bị ESP32"
    exit 1
fi
