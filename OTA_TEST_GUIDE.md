# Hướng dẫn Test Hệ thống OTA Firmware Update

## ✅ Build thành công!

Firmware đã được build thành công với:
- **RAM**: 14.9% (48,868 bytes)
- **Flash**: 76.9% (1,008,065 bytes)
- **Size**: Còn đủ dung lượng cho OTA update

## 🧪 Test Hệ thống OTA

### 1. **Upload Firmware hiện tại**
```bash
/home/anhnguyen/.platformio/penv/bin/pio run -t upload -e esp32dev
```

### 2. **Tạo firmware mới để test**
```bash
# Tăng version trong config.h
# FIRMWARE_VERSION "1.0.1"
# FIRMWARE_BUILD 2

# Build firmware mới
/home/anhnguyen/.platformio/penv/bin/pio run -e esp32dev

# Upload firmware mới lên backend
cd backend
./upload_firmware.sh .pio/build/esp32dev/firmware.bin 1.0.1 2 "Test OTA update"
```

### 3. **Test ESP32 tự động phát hiện**
1. **Khởi động ESP32** với firmware cũ (1.0.0-build1)
2. **ESP32 sẽ tự động check** firmware update
3. **Nếu có firmware mới**, ESP32 sẽ tạo WiFi AP:
   - **SSID**: `FirmwareUpdate-v1.0.0-1.0.1`
   - **Password**: `update123`
   - **IP**: `192.168.4.1`

### 4. **Test Web Interface**
1. **Kết nối vào WiFi AP** `FirmwareUpdate-v1.0.0-1.0.1`
2. **Truy cập**: `http://192.168.4.1`
3. **Chọn phương thức update**:
   - **WiFi**: Nhập SSID/password WiFi mới
   - **4G**: Sử dụng dữ liệu di động
   - **Manual**: Upload file .bin trực tiếp

## 🔍 Monitor Logs

### ESP32 Serial Monitor
```
🔍 Kiểm tra firmware update...
[FIRMWARE] Đang kiểm tra update...
🔄 Có firmware mới: 1.0.1
📥 URL: /api/firmware/download/1.0.1
📡 Khởi động WiFi AP thông báo firmware update...
🌐 WiFi AP: FirmwareUpdate-v1.0.0-1.0.1
🔑 Password: update123
📱 Truy cập: http://192.168.4.1
```

### Backend Logs
```
🔑 API Key loaded: battery_...
INFO:     127.0.0.1:xxxxx - "GET /api/firmware/check?current_version=1.0.0-build1&device_id=battery_monitor_001 HTTP/1.1" 200 OK
INFO:     127.0.0.1:xxxxx - "GET /api/firmware/download/1.0.1 HTTP/1.1" 200 OK
```

## 🛠️ Troubleshooting

### ❌ ESP32 không tạo WiFi AP
- **Kiểm tra**: ESP32 có kết nối WiFi không
- **Kiểm tra**: Backend có firmware mới không
- **Kiểm tra**: API key có đúng không

### ❌ Web interface không load
- **Kiểm tra**: Kết nối WiFi AP thành công
- **Kiểm tra**: IP `192.168.4.1` có accessible không
- **Kiểm tra**: Browser cache (Ctrl+F5)

### ❌ OTA update thất bại
- **Kiểm tra**: Kết nối mạng ổn định
- **Kiểm tra**: Firmware size không quá lớn
- **Kiểm tra**: Flash memory có đủ dung lượng

## 📊 Test Cases

### Test Case 1: Auto Detection
```
Input: ESP32 khởi động với firmware cũ
Expected: Tự động check và phát hiện firmware mới
Result: ✅ PASS / ❌ FAIL
```

### Test Case 2: WiFi AP Creation
```
Input: Phát hiện firmware mới
Expected: Tạo WiFi AP với tên version mới
Result: ✅ PASS / ❌ FAIL
```

### Test Case 3: Web Interface
```
Input: Kết nối vào WiFi AP
Expected: Web interface load thành công
Result: ✅ PASS / ❌ FAIL
```

### Test Case 4: OTA Update
```
Input: Chọn phương thức update
Expected: Firmware được tải và cài đặt thành công
Result: ✅ PASS / ❌ FAIL
```

## 🚀 Production Deployment

### 1. **Backend Setup**
```bash
# Set API key mạnh
export BATTERY_API_KEY="your_secure_api_key_here"

# Deploy backend
sudo docker-compose up -d
```

### 2. **ESP32 Configuration**
```cpp
// Cập nhật config.h
#define FIRMWARE_VERSION "1.0.0"
#define FIRMWARE_BUILD 1
#define BACKEND_HOST "your-domain.com"
#define APPLICATION_KEY "your_secure_api_key_here"
```

### 3. **Firmware Release**
```bash
# Build production firmware
pio run -e esp32dev

# Upload lên backend
./upload_firmware.sh firmware.bin 1.0.1 2 "Production release"
```

## 📈 Monitoring

### Metrics quan trọng
- **Firmware check frequency**: Mỗi 12 giờ
- **Update success rate**: Tỷ lệ thành công
- **Update duration**: Thời gian trung bình
- **Rollback rate**: Tỷ lệ cần rollback

### Alerts
- **Firmware check failed**: Không thể check update
- **Update failed**: OTA update thất bại
- **AP creation failed**: Không thể tạo WiFi AP
- **High rollback rate**: Nhiều device cần rollback

## 🔐 Security Checklist

- [ ] **API Key**: Sử dụng key mạnh và unique
- [ ] **HTTPS**: Sử dụng HTTPS cho production
- [ ] **Firmware signing**: Verify firmware signature
- [ ] **Access control**: Giới hạn quyền upload firmware
- [ ] **Audit logs**: Log tất cả firmware operations

## 📝 Next Steps

1. **Test trên hardware thật**
2. **Deploy backend production**
3. **Setup monitoring và alerts**
4. **Tạo firmware release process**
5. **Documentation cho end users**

Hệ thống OTA đã sẵn sàng để test và deploy! 🎉
