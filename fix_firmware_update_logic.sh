#!/bin/bash

echo "🔧 Fixing Internal Server Error & Improving Firmware Update Logic"
echo "==============================================================="

# Upload firmware với firmware update logic mới
echo "📤 Uploading firmware with improved firmware update logic..."
pio run --target upload

if [ $? -eq 0 ]; then
    echo "✅ Firmware upload thành công!"
    echo ""
    echo "🔧 Backend fixes:"
    echo "   • Sửa filter timestamp_to_datetime với error handling"
    echo "   • Kiểm tra timestamp hợp lệ (2020-2030)"
    echo "   • Thêm logging lỗi chi tiết"
    echo "   • Xử lý timezone an toàn hơn"
    echo ""
    echo "🚀 ESP32 firmware update improvements:"
    echo "   • Thêm endpoint /api/firmware/check"
    echo "   • Kiểm tra firmware trước khi hiển thị giao diện"
    echo "   • Thông báo 'Đã là phiên bản mới nhất' nếu không có update"
    echo "   • Confirm dialog trước khi vào giao diện update"
    echo "   • Force check firmware từ web interface"
    echo ""
    echo "🔄 Restarting backend để áp dụng fixes..."
    
    # Restart backend (nếu có Docker)
    if command -v docker &> /dev/null; then
        cd backend
        docker compose restart
        echo "✅ Backend đã được restart!"
    else
        echo "⚠️ Docker không có sẵn, cần restart backend thủ công"
        echo "   Hoặc restart service trên server khác"
    fi
    
    echo ""
    echo "📱 Cách sử dụng firmware update mới:"
    echo "   1. Truy cập: http://192.168.4.1"
    echo "   2. Nhấn nút '🔄 Firmware Update'"
    echo "   3. ESP32 sẽ kiểm tra firmware mới"
    echo "   4. Nếu có update: hiện confirm dialog"
    echo "   5. Nếu không có: thông báo 'Đã là phiên bản mới nhất'"
    echo ""
    echo "🔍 Monitor serial để xem logs:"
    echo "   pio device monitor"
    echo ""
    echo "✅ Các lỗi đã được sửa:"
    echo "   • Internal Server Error trên backend"
    echo "   • Firmware update logic cải thiện"
    echo "   • User experience tốt hơn"
else
    echo "❌ Firmware upload thất bại!"
    echo "Kiểm tra kết nối USB và thiết bị ESP32"
    exit 1
fi
