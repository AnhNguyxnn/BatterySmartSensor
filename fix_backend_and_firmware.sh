#!/bin/bash

echo "🔧 Fixing Backend Internal Server Error & Uploading ESP32 Firmware"
echo "================================================================="

# Upload firmware ESP32
echo "📤 Uploading ESP32 firmware with improved firmware update logic..."
pio run --target upload

if [ $? -eq 0 ]; then
    echo "✅ ESP32 firmware upload thành công!"
    echo ""
    echo "🔧 Backend fixes đã được áp dụng:"
    echo "   • Sửa filter timestamp_to_datetime với error handling tốt hơn"
    echo "   • Hiển thị timestamp có vấn đề thay vì crash"
    echo "   • Sử dụng created_at làm thời gian chính"
    echo "   • Device Time chỉ hiển thị khi cần"
    echo ""
    echo "🚀 ESP32 firmware update improvements:"
    echo "   • Thêm endpoint /api/firmware/check"
    echo "   • Kiểm tra firmware trước khi hiển thị giao diện"
    echo "   • Thông báo 'Đã là phiên bản mới nhất' nếu không có update"
    echo "   • Confirm dialog trước khi vào giao diện update"
    echo ""
    echo "🔄 CẦN RESTART BACKEND để áp dụng fixes:"
    echo ""
    echo "1. Nếu backend chạy trên Docker:"
    echo "   cd backend && docker compose restart"
    echo ""
    echo "2. Nếu backend chạy trên server khác:"
    echo "   ssh vào server và restart service"
    echo ""
    echo "3. Nếu backend chạy local:"
    echo "   Ctrl+C để dừng và chạy lại:"
    echo "   cd backend && python -m uvicorn app.main:app --host 0.0.0.0 --port 8000"
    echo ""
    echo "📱 Sau khi restart backend:"
    echo "   • Dashboard: http://cloud.anhnguyxn.io.vn:8000"
    echo "   • ESP32 Web: http://192.168.4.1"
    echo "   • Firmware update sẽ hoạt động thông minh hơn"
    echo ""
    echo "🔍 Monitor ESP32 serial:"
    echo "   pio device monitor"
else
    echo "❌ ESP32 firmware upload thất bại!"
    echo "Kiểm tra kết nối USB và thiết bị ESP32"
    exit 1
fi
