# Hướng dẫn ESP32 OTA Update khi không thể nạp USB

## 🎯 Vấn đề
- ESP32 không thể nạp code qua USB
- ESP32 không tự check firmware update
- Cần giải pháp để ESP32 tự update

## ✅ Giải pháp

### **Phương pháp 1: ESP32 Web Server OTA**

#### **Bước 1: Tạo ESP32 OTA Code**
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

#### **Bước 3: Sử dụng OTA Update**
```bash
# ESP32 tạo AP "ESP32-OTA"
# Connect vào AP với password "12345678"
# Mở browser: http://192.168.4.1
# Upload firmware file (.bin)
# ESP32 tự install firmware mới
```

### **Phương pháp 2: PlatformIO OTA**

#### **Bước 1: Enable OTA trong platformio.ini**
```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
upload_protocol = espota
upload_port = 192.168.1.100
```

#### **Bước 2: Upload qua WiFi**
```bash
# Upload firmware qua WiFi
pio run -t upload -e esp32dev --upload-port 192.168.1.100
```

### **Phương pháp 3: ESP32 OTA Library**

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

### **Upload via WiFi**
```bash
# Upload firmware qua WiFi
pio run -t upload -e esp32dev --upload-port 192.168.1.100
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

## ✅ Success Checklist

- [ ] ESP32 OTA code được upload
- [ ] ESP32 tạo AP "ESP32-OTA"
- [ ] Có thể connect vào AP
- [ ] Web interface hoạt động
- [ ] Upload firmware thành công
- [ ] ESP32 restart với firmware mới

## 🎯 Next Steps

1. **Upload ESP32 OTA code** lên ESP32
2. **Test ESP32 OTA** web interface
3. **Upload firmware** qua web interface
4. **Test ESP32** hoạt động với firmware mới
5. **Monitor** ESP32 hoạt động
