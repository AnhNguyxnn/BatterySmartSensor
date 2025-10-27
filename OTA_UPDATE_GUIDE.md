# Hướng dẫn OTA Firmware Update System

## 🎯 Mục tiêu
Tạo hệ thống OTA (Over-The-Air) update để ESP32 có thể tự động update firmware mà không cần USB.

## ✅ Giải pháp

### **Phương pháp 1: Sử dụng Backend OTA System**

#### **Bước 1: Upload Firmware lên Backend**
```bash
# Tạo firmware file
echo "Test firmware content" > firmware.bin

# Upload firmware
curl -X POST http://cloud.anhnguyxn.io.vn:8000/api/firmware/upload \
  -H "X-API-Key: battery_monitor_2024_secure_key" \
  -F "file=@firmware.bin" \
  -F "version=1.0.1" \
  -F "build=1" \
  -F "release_notes=Bug fixes"
```

#### **Bước 2: ESP32 Check Firmware Update**
```bash
# ESP32 sẽ tự động check firmware update
curl "http://cloud.anhnguyxn.io.vn:8000/api/firmware/check?current_version=1.0.0-build1&device_id=battery_monitor_001"
```

#### **Bước 3: ESP32 Download Firmware**
```bash
# ESP32 download firmware mới
curl -o firmware.bin "http://cloud.anhnguyxn.io.vn:8000/api/firmware/download/1.0.1"
```

### **Phương pháp 2: Sử dụng ESP32 OTA Library**

#### **Bước 1: Enable OTA trong ESP32**
```cpp
#include <ArduinoOTA.h>

void setup() {
  // OTA setup
  ArduinoOTA.setHostname("battery-monitor");
  ArduinoOTA.setPassword("battery123");
  ArduinoOTA.begin();
}

void loop() {
  ArduinoOTA.handle();
  // Your code here
}
```

#### **Bước 2: Upload qua WiFi**
```bash
# Upload firmware qua WiFi
pio run -t upload -e esp32dev --upload-port 192.168.1.100
```

### **Phương pháp 3: Sử dụng Web Server OTA**

#### **Bước 1: Tạo Web Server trên ESP32**
```cpp
#include <WebServer.h>
#include <Update.h>

WebServer server(80);

void handleUpdate() {
  server.sendHeader("Connection", "close");
  server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
  ESP.restart();
}

void handleUpload() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    Update.begin(UPDATE_SIZE_UNKNOWN);
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    Update.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    Update.end(true);
  }
}

void setup() {
  server.on("/update", HTTP_POST, handleUpdate, handleUpload);
  server.begin();
}
```

#### **Bước 2: Upload qua Web Interface**
```bash
# Upload firmware qua web interface
curl -X POST -F "firmware=@firmware.bin" http://192.168.1.100/update
```

## 🔧 Manual Commands

### **Upload Firmware to Backend**
```bash
curl -X POST http://cloud.anhnguyxn.io.vn:8000/api/firmware/upload \
  -H "X-API-Key: battery_monitor_2024_secure_key" \
  -F "file=@firmware.bin" \
  -F "version=1.0.1" \
  -F "build=1" \
  -F "release_notes=Bug fixes"
```

### **Check Firmware Update**
```bash
curl "http://cloud.anhnguyxn.io.vn:8000/api/firmware/check?current_version=1.0.0-build1&device_id=battery_monitor_001"
```

### **Download Firmware**
```bash
curl -o firmware.bin "http://cloud.anhnguyxn.io.vn:8000/api/firmware/download/1.0.1"
```

### **Upload via WiFi**
```bash
pio run -t upload -e esp32dev --upload-port 192.168.1.100
```

## 📊 Expected Results

### **Firmware Upload Response**
```json
{
  "status": "success",
  "message": "Firmware v1.0.1 uploaded successfully",
  "firmware_info": {
    "version": "1.0.1",
    "build": 1,
    "download_url": "/api/firmware/download/1.0.1",
    "file_size": 1048576,
    "release_notes": "Bug fixes",
    "checksum": "abc123def456...",
    "uploaded_at": "2025-01-21T...",
    "filename": "battery_monitor_v1.0.1.bin"
  }
}
```

### **Firmware Check Response**
```json
{
  "update_available": true,
  "latest_version": "1.0.1",
  "latest_build": 1,
  "current_version": "1.0.0-build1",
  "download_url": "/api/firmware/download/1.0.1",
  "file_size": 1048576,
  "release_notes": "Bug fixes",
  "checksum": "abc123def456..."
}
```

## 🚨 Troubleshooting

### **1. Firmware upload failed**
- **Cause**: Backend có bug hoặc form data sai
- **Solution**: Kiểm tra backend logs và fix bug

### **2. ESP32 không check update**
- **Cause**: ESP32 code chưa implement OTA
- **Solution**: Thêm OTA code vào ESP32

### **3. Download firmware failed**
- **Cause**: Network issue hoặc URL sai
- **Solution**: Kiểm tra network và URL

## ✅ Success Checklist

- [ ] Backend có firmware endpoints
- [ ] Firmware upload thành công
- [ ] ESP32 có thể check firmware update
- [ ] ESP32 có thể download firmware
- [ ] ESP32 có thể install firmware mới
- [ ] ESP32 restart với firmware mới

## 🎯 Next Steps

1. **Fix backend** firmware upload bug
2. **Test ESP32** OTA update
3. **Monitor** OTA update process
4. **Deploy** production firmware
5. **Monitor** system hoạt động
