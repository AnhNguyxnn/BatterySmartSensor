#!/bin/bash

echo "🚀 Upload Firmware với Tối Ưu Hóa Setup và HTTP"
echo "================================================"

echo "📝 Các tối ưu hóa đã thực hiện:"
echo "   • Tăng HTTP timeout từ 15s lên 30s"
echo "   • Giảm NTP sync timeout từ 2.5s xuống 1s"
echo "   • Giảm forceSyncNTP từ 2 lần xuống 1 lần"
echo "   • Giảm cellular timeout từ 20s xuống 15s"
echo "   • Giảm GPRS retry từ 2 lần xuống 1 lần"
echo "   • Giảm NETOPEN timeout từ 10s xuống 8s"
echo "   • Giảm cellular NTP timeout từ 5s xuống 2s"
echo "   • Tối ưu hóa MQ2 preheat để không chặn setup"
echo ""

echo "🔧 Upload firmware..."
platformio run --target upload --environment esp32dev

if [ $? -eq 0 ]; then
    echo ""
    echo "✅ Upload thành công!"
    echo ""
    echo "📊 Kết quả mong đợi:"
    echo "   • Setup nhanh hơn (giảm từ ~60s xuống ~30s)"
    echo "   • HTTP connection ổn định hơn"
    echo "   • NTP sync không chặn setup"
    echo "   • MQ2 preheat trong background"
    echo ""
    echo "🔍 Monitor serial để xem kết quả:"
    echo "   platformio device monitor -e esp32dev"
    echo ""
    echo "📈 So sánh với trước:"
    echo "   • HTTP -3 error sẽ giảm"
    echo "   • Setup time sẽ nhanh hơn"
    echo "   • NTP sync sẽ không chặn"
    echo ""
else
    echo ""
    echo "❌ Upload thất bại!"
    echo "Kiểm tra:"
    echo "   • ESP32 đã kết nối USB"
    echo "   • Driver đã cài đặt"
    echo "   • Port không bị chiếm dụng"
    echo ""
fi
