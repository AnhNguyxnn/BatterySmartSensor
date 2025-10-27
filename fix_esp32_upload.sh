#!/bin/bash
# Script fix ESP32 upload issue

echo "🔧 Fix ESP32 Upload Issue"
echo "========================"

echo "📋 ESP32 Upload Troubleshooting"
echo "=============================="

echo "1. 🔌 Kiểm tra kết nối ESP32:"
echo "   - Đảm bảo ESP32 được cắm vào USB"
echo "   - Kiểm tra driver USB-Serial"
echo "   - Thử port khác nếu cần"

echo ""
echo "2. 🔄 Reset ESP32 về Download Mode:"
echo "   - Nhấn và giữ nút BOOT (GPIO0)"
echo "   - Nhấn nút RESET (EN)"
echo "   - Thả nút RESET trước"
echo "   - Thả nút BOOT sau"
echo "   - ESP32 sẽ vào download mode"

echo ""
echo "3. 🛠️ Upload với esptool:"
echo "   - Sử dụng esptool trực tiếp"
echo "   - Thêm delay giữa các lệnh"
echo "   - Sử dụng baud rate thấp hơn"

echo ""
echo "4. 🔧 PlatformIO Commands:"
echo "   - pio run -t upload -e esp32dev"
echo "   - pio run -t upload -e esp32dev --upload-port /dev/ttyUSB0"
echo "   - pio run -t upload -e esp32dev --upload-port /dev/ttyUSB0 --upload-speed 115200"

echo ""
echo "5. 🚨 Manual Upload Steps:"
echo "   - Reset ESP32 về download mode"
echo "   - Chạy upload command ngay lập tức"
echo "   - Nếu fail, thử lại với delay"

echo ""
echo "6. 🔍 Check ESP32 Status:"
echo "   - lsusb | grep -i serial"
echo "   - dmesg | grep -i tty"
echo "   - ls -la /dev/ttyUSB*"

echo ""
echo "✅ Troubleshooting completed!"
