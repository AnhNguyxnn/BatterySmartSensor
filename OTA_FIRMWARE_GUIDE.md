# Hướng dẫn Hệ thống OTA Firmware Update

## Tổng quan

Hệ thống OTA (Over-The-Air) cho phép cập nhật firmware ESP32 từ xa qua WiFi hoặc 4G mà không cần kết nối USB.

### 🔄 **Quy trình hoạt động**
1. **ESP32 khởi động**: Kiểm tra firmware update lần đầu
2. **Kiểm tra định kỳ**: Mỗi 12 giờ tự động check
3. **Phát hiện update**: Tạo WiFi AP thông báo
4. **Người dùng kết nối**: Truy cập web interface
5. **Chọn phương thức**: WiFi, 4G, hoặc upload thủ công
6. **Cập nhật**: Tải và cài đặt firmware mới

## Backend API

### 📡 **Endpoints**

#### Kiểm tra firmware update
```bash
GET /api/firmware/check?current_version=1.0.0-build1&device_id=battery_monitor_001
```

**Response:**
```json
{
  "update_available": true,
  "latest_version": "1.0.1",
  "latest_build": 2,
  "current_version": "1.0.0-build1",
  "download_url": "/api/firmware/download/1.0.1",
  "file_size": 1048576,
  "release_notes": "Bug fixes and improvements",
  "checksum": "abc123def456"
}
```

#### Tải firmware
```bash
GET /api/firmware/download/1.0.1
```

#### Upload firmware mới
```bash
POST /api/firmware/upload
Content-Type: multipart/form-data
X-API-Key: your_api_key

file: firmware.bin
version: 1.0.1
build: 2
release_notes: Bug fixes and improvements
```

## ESP32 Configuration

### ⚙️ **Cấu hình trong config.h**
```cpp
// Firmware version info
#define FIRMWARE_VERSION "1.0.0"
#define FIRMWARE_BUILD 1

// Firmware update settings
#define FIRMWARE_CHECK_INTERVAL 43200000  // 12 giờ
#define FIRMWARE_UPDATE_TIMEOUT 300000    // 5 phút timeout
#define FIRMWARE_NOTIFICATION_AP_SSID "FirmwareUpdate-v" FIRMWARE_VERSION
#define FIRMWARE_NOTIFICATION_AP_PASSWORD "update123"
```

### 🔄 **Tự động kiểm tra**
- **Lần đầu**: Khi ESP32 khởi động
- **Định kỳ**: Mỗi 12 giờ
- **Điều kiện**: Phải có kết nối WiFi

## WiFi Notification AP

### 📡 **Khi có firmware mới**
ESP32 sẽ tạo WiFi AP với tên:
```
FirmwareUpdate-v1.0.0-1.0.1
```

**Thông tin kết nối:**
- **SSID**: `FirmwareUpdate-v1.0.0-1.0.1`
- **Password**: `update123`
- **IP**: `192.168.4.1`
- **Web**: `http://192.168.4.1`

### 🌐 **Web Interface**
Giao diện web cho phép:
1. **Cập nhật qua WiFi**: Kết nối WiFi mới và tải firmware
2. **Cập nhật qua 4G**: Sử dụng dữ liệu di động
3. **Upload thủ công**: Tải file .bin và upload trực tiếp

## Upload Firmware

### 🛠️ **Sử dụng script**
```bash
cd backend
./upload_firmware.sh firmware.bin 1.0.1 2 "Bug fixes and improvements"
```

### 📤 **Upload thủ công**
```bash
curl -X POST http://cloud.anhnguyxn.io.vn:8000/api/firmware/upload \
  -H "X-API-Key: battery_monitor_2024_secure_key" \
  -F "file=@firmware.bin" \
  -F "version=1.0.1" \
  -F "build=2" \
  -F "release_notes=Bug fixes and improvements"
```

### 📁 **Cấu trúc thư mục**
```
backend/
├── firmware/
│   ├── latest.json          # Thông tin firmware mới nhất
│   └── battery_monitor_v1.0.1.bin  # File firmware
└── upload_firmware.sh       # Script upload
```

## Test Hệ thống

### 1. **Test firmware check**
```bash
curl "http://cloud.anhnguyxn.io.vn:8000/api/firmware/check?current_version=1.0.0-build1&device_id=battery_monitor_001"
```

### 2. **Test upload firmware**
```bash
# Tạo file firmware giả
echo "fake firmware" > test_firmware.bin

# Upload
./upload_firmware.sh test_firmware.bin 1.0.1 2 "Test firmware"
```

### 3. **Test ESP32**
1. Upload firmware với version cũ
2. Upload firmware mới lên backend
3. ESP32 sẽ tự động phát hiện và tạo WiFi AP
4. Kết nối vào WiFi AP và truy cập web interface

## Troubleshooting

### ❌ **ESP32 không kiểm tra update**
- Kiểm tra kết nối WiFi
- Kiểm tra `FIRMWARE_CHECK_INTERVAL` trong config.h
- Kiểm tra log: `[FIRMWARE] Đang kiểm tra update...`

### ❌ **Backend không nhận upload**
- Kiểm tra API key: `X-API-Key` header
- Kiểm tra file .bin có đúng format không
- Kiểm tra kích thước file (không quá lớn)

### ❌ **WiFi AP không xuất hiện**
- Kiểm tra `firmwareUpdateAvailable = true`
- Kiểm tra `startFirmwareNotificationAP()` được gọi
- Kiểm tra log: `📡 Khởi động WiFi AP...`

### ❌ **OTA update thất bại**
- Kiểm tra kết nối mạng ổn định
- Kiểm tra kích thước firmware không quá lớn
- Kiểm tra flash memory có đủ dung lượng không

## Bảo mật

### 🔐 **API Key**
- Sử dụng API key mạnh cho backend
- Không hardcode API key trong firmware
- Rotate API key định kỳ

### 🛡️ **Firmware Integrity**
- Kiểm tra checksum trước khi cài đặt
- Verify signature nếu có
- Rollback mechanism nếu update thất bại

### 🌐 **Network Security**
- Sử dụng HTTPS cho production
- Validate firmware source
- Rate limiting cho API endpoints

## Monitoring

### 📊 **Logs quan trọng**
```
[FIRMWARE] Đang kiểm tra update...
[FIRMWARE] Có firmware mới: 1.0.1
📡 Khởi động WiFi AP thông báo firmware update...
🌐 WiFi AP: FirmwareUpdate-v1.0.0-1.0.1
🔄 Bắt đầu OTA update từ: http://...
✅ Update thành công! Khởi động lại...
```

### 📈 **Metrics**
- Số lần check firmware
- Tỷ lệ thành công update
- Thời gian trung bình update
- Số lượng device đã update

## Best Practices

1. **Version Management**: Sử dụng semantic versioning
2. **Rollback Plan**: Luôn có kế hoạch rollback
3. **Testing**: Test firmware trên device thật trước khi release
4. **Monitoring**: Monitor quá trình update
5. **Documentation**: Ghi chú release notes chi tiết
