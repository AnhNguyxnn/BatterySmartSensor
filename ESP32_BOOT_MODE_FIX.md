# Hướng dẫn Fix ESP32 Boot Mode Issue

## 🎯 Vấn đề
```
A fatal error occurred: Failed to connect to ESP32: Wrong boot mode detected (0xb)!
The chip needs to be in download mode.
```

## ✅ Giải pháp

### **Phương pháp 1: Manual Boot Mode Reset**

#### **Bước 1: Chuẩn bị ESP32**
```bash
# Kiểm tra ESP32 có được nhận diện không
lsusb | grep -i serial
dmesg | grep -i tty
ls -la /dev/ttyUSB*
```

#### **Bước 2: Reset ESP32 về Download Mode**
```bash
# Nhấn và giữ nút BOOT (GPIO0)
# Nhấn nút RESET (EN)
# Thả nút RESET trước
# Thả nút BOOT sau
# ESP32 sẽ vào download mode
```

#### **Bước 3: Upload ngay lập tức**
```bash
# Upload ngay sau khi reset
pio run -t upload -e esp32dev --upload-port /dev/ttyUSB0
```

### **Phương pháp 2: esptool với Auto Reset**

#### **Bước 1: Reset ESP32**
```bash
# Reset ESP32 về download mode như trên
```

#### **Bước 2: Upload với esptool**
```bash
# Upload với esptool và auto reset
esptool.py --port /dev/ttyUSB0 --baud 115200 --before default_reset --after hard_reset --chip esp32 write_flash 0x10000 .pio/build/esp32dev/firmware.bin
```

### **Phương pháp 3: esptool với Chip Reset**

#### **Bước 1: Reset ESP32**
```bash
# Reset ESP32 về download mode
```

#### **Bước 2: Upload với chip reset**
```bash
# Upload với chip reset
esptool.py --port /dev/ttyUSB0 --baud 115200 --before default_reset --after hard_reset --chip esp32 --before default_reset --after hard_reset write_flash 0x10000 .pio/build/esp32dev/firmware.bin
```

### **Phương pháp 4: Hardware Reset**

#### **Bước 1: Tắt nguồn ESP32**
```bash
# Tắt nguồn ESP32
```

#### **Bước 2: Reset ESP32**
```bash
# Nhấn và giữ nút BOOT (GPIO0)
# Bật nguồn ESP32
# Thả nút BOOT
# ESP32 sẽ vào download mode
```

#### **Bước 3: Upload ngay lập tức**
```bash
# Upload ngay sau khi reset
pio run -t upload -e esp32dev --upload-port /dev/ttyUSB0
```

## 🔧 Manual Commands

### **Check ESP32 Status**
```bash
# Check USB devices
lsusb | grep -i serial

# Check tty devices
dmesg | grep -i tty

# Check ttyUSB devices
ls -la /dev/ttyUSB*
```

### **Reset ESP32**
```bash
# Manual reset steps:
# 1. Hold BOOT button (GPIO0)
# 2. Press RESET button (EN)
# 3. Release RESET first
# 4. Release BOOT second
# 5. ESP32 enters download mode
```

### **Upload Firmware**
```bash
# PlatformIO upload
pio run -t upload -e esp32dev --upload-port /dev/ttyUSB0

# esptool upload
esptool.py --port /dev/ttyUSB0 --baud 115200 write_flash 0x10000 .pio/build/esp32dev/firmware.bin

# esptool with reset
esptool.py --port /dev/ttyUSB0 --baud 115200 --before default_reset --after hard_reset --chip esp32 write_flash 0x10000 .pio/build/esp32dev/firmware.bin
```

## 📊 Expected Results

### **ESP32 Status**
```
Bus 001 Device 003: ID 10c4:ea60 Silicon Labs CP210x UART Bridge
```

### **ttyUSB Device**
```
crw-rw---- 1 root dialout 188, 0 Oct 21 10:00 /dev/ttyUSB0
```

### **Upload Success**
```
Writing at 0x00010000... (100%)
Wrote 1048576 bytes (100.0%, 100.0% free) in 8.9s (at 115200 baud)
Hash of data verified.

Leaving...
Hard resetting via RTS pin...
```

## 🚨 Troubleshooting

### **1. "Wrong boot mode detected"**
- **Cause**: ESP32 không ở download mode
- **Solution**: Reset ESP32 về download mode

### **2. "Failed to connect to ESP32"**
- **Cause**: ESP32 không được nhận diện
- **Solution**: Kiểm tra kết nối USB và driver

### **3. "Permission denied"**
- **Cause**: Không có quyền truy cập port
- **Solution**: Thêm user vào group dialout

### **4. "Port busy"**
- **Cause**: Port đang được sử dụng
- **Solution**: Đóng các ứng dụng đang sử dụng port

## ✅ Success Checklist

- [ ] ESP32 được nhận diện qua USB
- [ ] Port /dev/ttyUSB0 accessible
- [ ] ESP32 ở download mode
- [ ] Upload command chạy thành công
- [ ] Firmware được upload thành công
- [ ] ESP32 restart và chạy firmware mới

## 🎯 Next Steps

1. **Reset ESP32** về download mode
2. **Upload firmware** ngay lập tức
3. **Test ESP32** hoạt động
4. **Test firmware update** qua OTA
5. **Monitor system** hoạt động
