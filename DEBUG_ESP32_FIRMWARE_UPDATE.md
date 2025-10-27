# Hướng dẫn Debug ESP32 Firmware Update

## 🎯 Vấn đề
ESP32 có code check firmware update nhưng không hoạt động vì:
1. ESP32 code chưa được upload (không thể upload qua USB)
2. Backend có bug với firmware upload (version bị rỗng)

## ✅ Giải pháp

### **Phương pháp 1: Upload ESP32 Code qua OTA**

#### **Bước 1: Tạo ESP32 OTA Code đơn giản**
```cpp
#include <WebServer.h>
#include <Update.h>
#include <WiFi.h>

WebServer server(80);

void handleRoot() {
  server.send(200, "text/html", 
    "<html><body>"
    "<h1>ESP32 Firmware Update</h1>"
    "<form method='POST' action='/update' enctype='multipart/form-data'>"
    "<input type='file' name='firmware'><br><br>"
    "<input type='submit' value='Update Firmware'>"
    "</form>"
    "</body></html>"
  );
}

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
  Serial.begin(115200);
  WiFi.softAP("ESP32-OTA", "12345678");
  server.on("/", handleRoot);
  server.on("/update", HTTP_POST, handleUpdate, handleUpload);
  server.begin();
  Serial.println("ESP32 OTA Server started");
  Serial.println("AP: ESP32-OTA");
  Serial.println("Password: 12345678");
  Serial.println("IP: 192.168.4.1");
}

void loop() {
  server.handleClient();
}
```

#### **Bước 2: Upload Code lên ESP32**
```bash
# Nếu có thể upload qua USB
pio run -t upload -e esp32dev

# Hoặc sử dụng Arduino IDE
# File -> Upload
```

#### **Bước 3: Upload Firmware qua Web Interface**
```bash
# ESP32 tạo AP "ESP32-OTA"
# Connect vào AP với password "12345678"
# Mở browser: http://192.168.4.1
# Upload firmware file (.bin)
# ESP32 tự install firmware mới
```

### **Phương pháp 2: Fix Backend Firmware Upload Bug**

#### **Bước 1: Kiểm tra Backend Code**
```python
# backend/app/main.py
@app.post("/api/firmware/upload")
async def upload_firmware(
    file: UploadFile = File(...),
    version: str = "",
    build: int = 0,
    release_notes: str = "",
    api_key: str = Depends(verify_api_key)
):
    # Bug: version, build, release_notes bị rỗng
    # Cần fix để nhận đúng form data
```

#### **Bước 2: Fix Backend Code**
```python
# Fix backend để nhận đúng form data
@app.post("/api/firmware/upload")
async def upload_firmware(
    file: UploadFile = File(...),
    version: str = Form(...),
    build: int = Form(...),
    release_notes: str = Form(...),
    api_key: str = Depends(verify_api_key)
):
    # Fix: Sử dụng Form(...) thay vì ""
```

### **Phương pháp 3: Test ESP32 Firmware Update**

#### **Bước 1: Kiểm tra ESP32 Serial Monitor**
```bash
# Mở Serial Monitor
# Kiểm tra ESP32 có check firmware update không
# Kiểm tra ESP32 có kết nối WiFi không
# Kiểm tra ESP32 có kết nối backend không
```

#### **Bước 2: Test Firmware Check**
```bash
# Test firmware check từ ESP32
curl "http://cloud.anhnguyxn.io.vn:8000/api/firmware/check?current_version=1.0.0-build1&device_id=battery_monitor_001"
```

#### **Bước 3: Test Firmware Download**
```bash
# Test firmware download từ ESP32
curl -o firmware.bin "http://cloud.anhnguyxn.io.vn:8000/api/firmware/download/1.0.1"
```

## 🔧 Manual Commands

### **Upload ESP32 OTA Code**
```bash
# Upload code OTA lên ESP32
pio run -t upload -e esp32dev

# Hoặc sử dụng Arduino IDE
# File -> Upload
```

### **Test ESP32 OTA**
```bash
# ESP32 tạo AP "ESP32-OTA"
# Connect vào AP với password "12345678"
# Mở browser: http://192.168.4.1
# Upload firmware file (.bin)
```

### **Test Firmware Check**
```bash
# Test firmware check
curl "http://cloud.anhnguyxn.io.vn:8000/api/firmware/check?current_version=1.0.0-build1&device_id=battery_monitor_001"
```

### **Test Firmware Download**
```bash
# Test firmware download
curl -o firmware.bin "http://cloud.anhnguyxn.io.vn:8000/api/firmware/download/1.0.1"
```

## 📊 Expected Results

### **ESP32 OTA Server**
```
ESP32 OTA Server started
AP: ESP32-OTA
Password: 12345678
IP: 192.168.4.1
```

### **Web Interface**
```
ESP32 Firmware Update
[File Upload Form]
[Update Firmware Button]
```

### **Upload Success**
```
OK
ESP32 restarting...
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
  "release_notes": "Test firmware for ESP32",
  "checksum": "abc123def456..."
}
```

## 🚨 Troubleshooting

### **1. ESP32 không tạo AP**
- **Cause**: Code chưa được upload
- **Solution**: Upload code OTA lên ESP32

### **2. Không thể connect AP**
- **Cause**: Password sai hoặc AP không hoạt động
- **Solution**: Kiểm tra password và AP status

### **3. Upload firmware failed**
- **Cause**: Firmware file sai hoặc ESP32 lỗi
- **Solution**: Kiểm tra firmware file và ESP32 status

### **4. Backend firmware upload bug**
- **Cause**: Backend không nhận đúng form data
- **Solution**: Fix backend code để nhận đúng form data

## ✅ Success Checklist

- [ ] ESP32 OTA code được upload
- [ ] ESP32 tạo AP "ESP32-OTA"
- [ ] Có thể connect vào AP
- [ ] Web interface hoạt động
- [ ] Upload firmware thành công
- [ ] ESP32 restart với firmware mới
- [ ] Backend firmware upload hoạt động
- [ ] ESP32 firmware update hoạt động

## 🎯 Next Steps

1. **Upload ESP32 OTA code** lên ESP32
2. **Test ESP32 OTA** web interface
3. **Upload firmware** qua web interface
4. **Fix backend** firmware upload bug
5. **Test ESP32** firmware update
6. **Monitor** ESP32 hoạt động
