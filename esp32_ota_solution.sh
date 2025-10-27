#!/bin/bash
# Script tạo giải pháp ESP32 tự check và update firmware

echo "🔧 Tạo Giải pháp ESP32 Tự Check và Update Firmware"
echo "================================================="

echo "📋 Vấn đề hiện tại:"
echo "1. ESP32 không thể nạp code qua USB"
echo "2. ESP32 không tự check firmware update"
echo "3. Cần giải pháp để ESP32 tự update"

echo ""
echo "✅ Giải pháp:"

echo ""
echo "🔧 Phương pháp 1: Sử dụng ESP32 Web Server OTA"
echo "=============================================="
echo "1. ESP32 tạo web server trên port 80"
echo "2. Upload firmware qua web interface"
echo "3. ESP32 tự install firmware mới"

echo ""
echo "🔧 Phương pháp 2: Sử dụng ESP32 OTA Library"
echo "=========================================="
echo "1. ESP32 connect WiFi"
echo "2. ESP32 check firmware update từ backend"
echo "3. ESP32 download và install firmware mới"

echo ""
echo "🔧 Phương pháp 3: Sử dụng ESP32 OTA via WiFi"
echo "==========================================="
echo "1. ESP32 tạo SoftAP"
echo "2. Connect vào ESP32 AP"
echo "3. Upload firmware qua web interface"

echo ""
echo "📱 ESP32 OTA Code Example:"
echo "========================="
cat << 'EOF'
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
  WiFi.softAP("ESP32-OTA", "12345678");
  server.on("/", handleRoot);
  server.on("/update", HTTP_POST, handleUpdate, handleUpload);
  server.begin();
}

void loop() {
  server.handleClient();
}
EOF

echo ""
echo "🚀 Cách sử dụng:"
echo "==============="
echo "1. Upload code này lên ESP32 (nếu có thể)"
echo "2. ESP32 tạo AP 'ESP32-OTA'"
echo "3. Connect vào AP với password '12345678'"
echo "4. Mở browser: http://192.168.4.1"
echo "5. Upload firmware file (.bin)"
echo "6. ESP32 tự install firmware mới"

echo ""
echo "🔧 Alternative: Sử dụng PlatformIO OTA"
echo "====================================="
echo "1. Enable OTA trong platformio.ini:"
echo "   upload_protocol = espota"
echo "   upload_port = 192.168.1.100"
echo ""
echo "2. Upload firmware qua WiFi:"
echo "   pio run -t upload -e esp32dev --upload-port 192.168.1.100"

echo ""
echo "🎯 Next Steps:"
echo "============="
echo "1. Thử upload code OTA lên ESP32"
echo "2. Nếu không được, sử dụng PlatformIO OTA"
echo "3. Test firmware update qua web interface"
echo "4. Monitor ESP32 hoạt động"
