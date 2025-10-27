#!/bin/bash

echo "🕐 Switching to Server Time for ESP32 Battery Monitor"
echo "===================================================="

# Upload firmware không gửi timestamp
echo "📤 Uploading firmware without timestamp..."
pio run --target upload

if [ $? -eq 0 ]; then
    echo "✅ Firmware upload thành công!"
    echo ""
    echo "🕐 Backend changes:"
    echo "   • Server tự tạo timestamp khi nhận dữ liệu"
    echo "   • Không phụ thuộc vào thời gian ESP32"
    echo "   • Timestamp luôn chính xác theo server"
    echo "   • Schema cho phép timestamp optional"
    echo ""
    echo "📱 ESP32 changes:"
    echo "   • Không gửi timestamp trong JSON"
    echo "   • Giảm kích thước dữ liệu gửi"
    echo "   • Không cần sync NTP phức tạp"
    echo "   • Setup nhanh hơn (bỏ qua sync time)"
    echo ""
    echo "🔄 Restarting backend để áp dụng changes..."
    
    # Restart backend
    cd backend
    docker-compose restart
    
    if [ $? -eq 0 ]; then
        echo "✅ Backend đã được restart!"
        echo ""
        echo "🌐 Frontend changes:"
        echo "   • Hiển thị 'Time' (server timestamp)"
        echo "   • Hiển thị 'Server Received' (created_at)"
        echo "   • Thời gian luôn chính xác"
        echo ""
        echo "📊 Kết quả:"
        echo "   • Thời gian luôn đúng theo server"
        echo "   • Không còn vấn đề sync NTP"
        echo "   • Setup ESP32 nhanh hơn"
        echo "   • Dữ liệu gửi nhẹ hơn"
        echo ""
        echo "📱 Truy cập dashboard: http://192.168.4.1"
        echo "📊 Monitor serial: pio device monitor"
        echo ""
        echo "⏱️ Thời gian hiển thị sẽ là thời gian server nhận dữ liệu!"
    else
        echo "❌ Không thể restart backend!"
        echo "Thử: cd backend && docker-compose restart"
    fi
else
    echo "❌ Firmware upload thất bại!"
    echo "Kiểm tra kết nối USB và thiết bị ESP32"
    exit 1
fi
