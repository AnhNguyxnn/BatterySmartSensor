#!/bin/bash
# Script upload ESP32 với auto reset

echo "🚀 Upload ESP32 với Auto Reset"
echo "=============================="

echo "📋 ESP32 Upload với Auto Reset"
echo "============================="

echo "1. 🔌 Đảm bảo ESP32 được cắm vào USB"
echo "2. 🔄 Reset ESP32 về download mode"
echo "3. 🚀 Chạy upload command ngay lập tức"

echo ""
echo "🔧 Manual Reset Steps:"
echo "====================="
echo "1. Nhấn và giữ nút BOOT (GPIO0)"
echo "2. Nhấn nút RESET (EN)"
echo "3. Thả nút RESET trước"
echo "4. Thả nút BOOT sau"
echo "5. ESP32 sẽ vào download mode"

echo ""
echo "🚀 Upload Commands:"
echo "=================="

echo ""
echo "🔧 PlatformIO Upload:"
echo "pio run -t upload -e esp32dev --upload-port /dev/ttyUSB0"

echo ""
echo "🔧 esptool Upload:"
echo "esptool.py --port /dev/ttyUSB0 --baud 115200 write_flash 0x10000 .pio/build/esp32dev/firmware.bin"

echo ""
echo "🔧 esptool với Reset:"
echo "esptool.py --port /dev/ttyUSB0 --baud 115200 --before default_reset --after hard_reset write_flash 0x10000 .pio/build/esp32dev/firmware.bin"

echo ""
echo "🔧 esptool với Chip Reset:"
echo "esptool.py --port /dev/ttyUSB0 --baud 115200 --before default_reset --after hard_reset --chip esp32 write_flash 0x10000 .pio/build/esp32dev/firmware.bin"

echo ""
echo "🔧 esptool với Delay:"
echo "esptool.py --port /dev/ttyUSB0 --baud 115200 --before default_reset --after hard_reset --chip esp32 --before default_reset --after hard_reset write_flash 0x10000 .pio/build/esp32dev/firmware.bin"

echo ""
echo "🚀 Auto Upload Script:"
echo "====================="

# Kiểm tra ESP32 có được nhận diện không
if [ -e "/dev/ttyUSB0" ]; then
    echo "✅ ESP32 detected at /dev/ttyUSB0"
else
    echo "❌ ESP32 not detected at /dev/ttyUSB0"
    echo "Please check USB connection and driver"
    exit 1
fi

# Kiểm tra permissions
if [ -r "/dev/ttyUSB0" ] && [ -w "/dev/ttyUSB0" ]; then
    echo "✅ ESP32 permissions OK"
else
    echo "❌ ESP32 permissions issue"
    echo "Please run: sudo chmod 666 /dev/ttyUSB0"
    exit 1
fi

echo ""
echo "🔄 Attempting upload with auto reset..."

# Thử upload với esptool
echo "🔧 Trying esptool upload..."
esptool.py --port /dev/ttyUSB0 --baud 115200 --before default_reset --after hard_reset --chip esp32 write_flash 0x10000 .pio/build/esp32dev/firmware.bin

if [ $? -eq 0 ]; then
    echo "✅ Upload successful!"
else
    echo "❌ Upload failed, trying alternative method..."
    
    # Thử upload với PlatformIO
    echo "🔧 Trying PlatformIO upload..."
    pio run -t upload -e esp32dev --upload-port /dev/ttyUSB0
    
    if [ $? -eq 0 ]; then
        echo "✅ Upload successful!"
    else
        echo "❌ Upload failed, please try manual reset"
        echo ""
        echo "🔧 Manual Reset Steps:"
        echo "1. Nhấn và giữ nút BOOT (GPIO0)"
        echo "2. Nhấn nút RESET (EN)"
        echo "3. Thả nút RESET trước"
        echo "4. Thả nút BOOT sau"
        echo "5. Chạy upload command ngay lập tức"
    fi
fi
