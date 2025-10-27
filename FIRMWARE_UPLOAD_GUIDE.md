# Hướng dẫn Upload Firmware lên Server (Không cần PlatformIO)

## 🎯 Vấn đề
Server vật lý không có PlatformIO, nhưng cần upload firmware lên backend để ESP32 có thể OTA update.

## ✅ Giải pháp

### **Phương pháp 1: Web Interface (Dễ nhất)**
```bash
# Mở file HTML trong browser
open firmware_upload.html
# hoặc
firefox firmware_upload.html
# hoặc
google-chrome firmware_upload.html
```

**Cách sử dụng:**
1. Build firmware trên máy local: `pio run -e esp32dev`
2. Mở `firmware_upload.html` trong browser
3. Chọn file `.pio/build/esp32dev/firmware.bin`
4. Nhập version (ví dụ: 1.0.1)
5. Click "Upload Firmware"

### **Phương pháp 2: Bash Script**
```bash
# Cài đặt curl (nếu chưa có)
sudo apt install curl  # Ubuntu/Debian
sudo yum install curl   # CentOS/RHEL
brew install curl       # macOS

# Upload firmware
./upload_firmware_local.sh .pio/build/esp32dev/firmware.bin 1.0.1 http://cloud.anhnguyxn.io.vn:8000
```

### **Phương pháp 3: Python Script**
```bash
# Cài đặt requests (nếu chưa có)
pip3 install requests

# Upload firmware
python3 upload_firmware.py .pio/build/esp32dev/firmware.bin 1.0.1 http://cloud.anhnguyxn.io.vn:8000
```

### **Phương pháp 4: curl Command**
```bash
curl -X POST http://cloud.anhnguyxn.io.vn:8000/api/firmware/upload \
  -H "X-API-Key: battery_monitor_2024_secure_key" \
  -F "file=@.pio/build/esp32dev/firmware.bin" \
  -F "version=1.0.1" \
  -F "build=1" \
  -F "release_notes=Bug fixes and improvements"
```

## 🔧 Workflow hoàn chỉnh

### **Bước 1: Build firmware trên máy local**
```bash
# Có PlatformIO trên máy local
cd /mnt/sdb1/dev/Hoc/IOT/BatterySmartSensor
pio run -e esp32dev

# Firmware sẽ được tạo tại:
# .pio/build/esp32dev/firmware.bin
```

### **Bước 2: Upload lên server**
```bash
# Chọn một trong các phương pháp trên
# Ví dụ: Web interface
open firmware_upload.html
```

### **Bước 3: ESP32 tự động phát hiện**
1. ESP32 khởi động và check firmware update
2. Phát hiện có firmware mới
3. Tạo WiFi AP: `FirmwareUpdate-v1.0.0-1.0.1`
4. User kết nối và update qua web interface

## 📁 File Structure

```
BatterySmartSensor/
├── .pio/build/esp32dev/firmware.bin    # Firmware đã build
├── firmware_upload.html                # Web interface upload
├── upload_firmware_local.sh            # Bash script
├── upload_firmware.py                   # Python script
└── upload_firmware_server.sh           # Script trên server
```

## 🌐 Backend API Endpoints

### **Upload Firmware**
```http
POST /api/firmware/upload
Content-Type: multipart/form-data
X-API-Key: battery_monitor_2024_secure_key

file: firmware.bin
version: 1.0.1
build: 1
release_notes: Bug fixes
```

### **Check Firmware**
```http
GET /api/firmware/check?current_version=1.0.0-build1&device_id=battery_monitor_001
```

### **Download Firmware**
```http
GET /api/firmware/download/1.0.1
```

## 🛠️ Troubleshooting

### ❌ Upload thất bại
- **Kiểm tra file**: Phải có extension .bin
- **Kiểm tra server**: URL có đúng không
- **Kiểm tra API key**: Có đúng không
- **Kiểm tra kết nối**: Mạng có ổn định không

### ❌ ESP32 không phát hiện firmware mới
- **Kiểm tra version**: Version mới phải lớn hơn version cũ
- **Kiểm tra build**: Build number phải lớn hơn
- **Kiểm tra ESP32**: Có kết nối WiFi không
- **Kiểm tra logs**: Xem log ESP32 có check firmware không

### ❌ Web interface không load
- **Kiểm tra browser**: Có hỗ trợ HTML5 không
- **Kiểm tra file**: `firmware_upload.html` có tồn tại không
- **Kiểm tra JavaScript**: Có bị disable không

## 🔐 Security

### **API Key Management**
```bash
# Set API key mạnh
export BATTERY_API_KEY="your_secure_api_key_here"

# Hoặc sửa trong script
API_KEY="your_secure_api_key_here"
```

### **HTTPS cho Production**
```bash
# Thay đổi URL thành HTTPS
https://your-domain.com:8000
```

## 📊 Monitoring

### **Check firmware info**
```bash
curl http://cloud.anhnguyxn.io.vn:8000/api/firmware/info
```

### **Check update status**
```bash
curl "http://cloud.anhnguyxn.io.vn:8000/api/firmware/check?current_version=1.0.0-build1&device_id=battery_monitor_001"
```

## 🚀 Production Deployment

### **1. Setup Environment Variables**
```bash
# Trên server
export BATTERY_API_KEY="production_secure_key"
export BACKEND_URL="https://your-domain.com:8000"
```

### **2. Upload Production Firmware**
```bash
# Build production firmware
pio run -e esp32dev

# Upload với version production
./upload_firmware_local.sh .pio/build/esp32dev/firmware.bin 1.0.0 https://your-domain.com:8000
```

### **3. Monitor Deployment**
```bash
# Check firmware info
curl https://your-domain.com:8000/api/firmware/info

# Monitor ESP32 logs
pio device monitor
```

## 📝 Best Practices

1. **Version Management**: Sử dụng semantic versioning
2. **Testing**: Test firmware trên device thật trước khi release
3. **Rollback Plan**: Luôn có kế hoạch rollback
4. **Documentation**: Ghi chú release notes chi tiết
5. **Monitoring**: Monitor quá trình update

## 🎉 Kết luận

Với các phương pháp trên, bạn có thể:
- ✅ Build firmware trên máy local có PlatformIO
- ✅ Upload firmware lên server không có PlatformIO
- ✅ ESP32 tự động phát hiện và update firmware
- ✅ Quản lý firmware version dễ dàng
- ✅ Monitor quá trình update

Hệ thống OTA hoàn chỉnh và sẵn sàng cho production! 🚀
