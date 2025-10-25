#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>
#include "config.h"
#include "cellular.h"

extern WebServer server;
extern bool firmwareUpdateAvailable;
extern String latestFirmwareVersion;
extern String latestFirmwareUrl;
extern String latestFirmwareDisplayVersion;
extern bool firmwareNotificationAPActive;
extern int currentConnectionMode;

// Connection mode constants
#define CONNECTION_NONE 0
#define CONNECTION_WIFI_FIRST 1
#define CONNECTION_4G_FIRST 2

// Forward declarations
void startFirmwareNotificationAP();
void handleFirmwareWebInterface();

void checkFirmwareUpdate() {
  // Chỉ kiểm tra firmware update nếu đã có kết nối internet
  if (WiFi.status() != WL_CONNECTED && currentConnectionMode != CONNECTION_4G_FIRST) {
    Serial.println("[FIRMWARE] Bỏ qua kiểm tra update - chưa có kết nối internet");
    return;
  }
  
  String currentVersion = String(FIRMWARE_VERSION) + "-build" + String(FIRMWARE_BUILD);
  String url = String("http://") + String(BACKEND_HOST) + ":" + String(BACKEND_PORT) + "/api/firmware/check?current_version=" + currentVersion + "&device_id=" + String(DEVICE_ID);
  
  HTTPClient http;
  bool success = false;
  
  // Thử qua WiFi trước
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[FIRMWARE] Kiểm tra update qua WiFi...");
    esp_task_wdt_reset(); // Reset watchdog before HTTP request
    http.begin(url);
    http.addHeader("X-API-Key", APPLICATION_KEY);
    
    int httpCode = http.GET();
    if (httpCode == 200) {
      String response = http.getString();
      http.end();
      
      JsonDocument doc;
      deserializeJson(doc, response);
      
      bool updateAvailable = doc["update_available"];
      if (updateAvailable) {
        firmwareUpdateAvailable = true;
        String serverVersion = doc["latest_version"].as<String>();
        latestFirmwareVersion = serverVersion; // Giữ nguyên để so sánh
        latestFirmwareUrl = doc["download_url"].as<String>();
        
        // Parse và format version để hiển thị đầy đủ
        String displayVersion = serverVersion;
        if (doc["latest_build"].is<int>()) {
          int buildNumber = doc["latest_build"];
          displayVersion = serverVersion + " (Build " + String(buildNumber) + ")";
        } else {
          // Nếu server không có latest_build, tự động thêm build number
          // Giả định build number mới sẽ là build hiện tại + 1
          int newBuildNumber = FIRMWARE_BUILD + 1;
          displayVersion = serverVersion + " (Build " + String(newBuildNumber) + ")";
        }
        latestFirmwareDisplayVersion = displayVersion;
        
        Serial.println("🔄 Có firmware mới: " + displayVersion);
        Serial.println("📥 URL: " + latestFirmwareUrl);
        
        // Tạo AP thông báo firmware update
        startFirmwareNotificationAP();
        success = true;
      } else {
        Serial.println("✅ Firmware đã cập nhật mới nhất");
        success = true;
      }
    } else {
      Serial.println("[FIRMWARE] Lỗi WiFi: " + String(httpCode));
    }
    http.end();
  }
  
  // Fallback qua 4G nếu WiFi thất bại
  #if ENABLE_CELLULAR_UPLOAD
  if (!success) {
    Serial.println("[FIRMWARE] Thử kiểm tra update qua 4G...");
    esp_task_wdt_reset(); // Reset watchdog before cellular operations
    if (cellularBegin()) {
      String response;
      String path = "/api/firmware/check?current_version=" + currentVersion + "&device_id=" + String(DEVICE_ID);
      bool ok = cellularHttpPost(BACKEND_HOST, BACKEND_PORT, path.c_str(), "", response);
      
      if (ok) {
        JsonDocument doc;
        deserializeJson(doc, response);
        
        bool updateAvailable = doc["update_available"];
        if (updateAvailable) {
          firmwareUpdateAvailable = true;
          String serverVersion = doc["latest_version"].as<String>();
          latestFirmwareVersion = serverVersion; // Giữ nguyên để so sánh
          latestFirmwareUrl = doc["download_url"].as<String>();
          
          // Parse và format version để hiển thị đầy đủ
          String displayVersion = serverVersion;
          if (doc["latest_build"].is<int>()) {
            int buildNumber = doc["latest_build"];
            displayVersion = serverVersion + " (Build " + String(buildNumber) + ")";
          } else {
            // Nếu server không có latest_build, tự động thêm build number
            int newBuildNumber = FIRMWARE_BUILD + 1;
            displayVersion = serverVersion + " (Build " + String(newBuildNumber) + ")";
          }
          latestFirmwareDisplayVersion = displayVersion;
          
          Serial.println("🔄 Có firmware mới (4G): " + displayVersion);
          Serial.println("📥 URL: " + latestFirmwareUrl);
          
          // Tạo AP thông báo firmware update
          startFirmwareNotificationAP();
        } else {
          Serial.println("✅ Firmware đã cập nhật mới nhất (4G)");
        }
      } else {
        Serial.println("[FIRMWARE] Lỗi kiểm tra update qua 4G");
      }
    } else {
      Serial.println("[FIRMWARE] Không thể kết nối 4G để kiểm tra update");
    }
  }
  #endif
}

void startFirmwareNotificationAP() {
  if (firmwareNotificationAPActive) return;
  
  Serial.println("📡 Có firmware mới - thông báo qua AP chính...");
  
  // Không tạo AP riêng, chỉ thông báo qua AP chính
  firmwareNotificationAPActive = true;
  
  Serial.println("🔄 Có firmware mới: " + latestFirmwareVersion);
  Serial.println("📥 URL: " + latestFirmwareUrl);
  Serial.println("📱 Để cập nhật firmware, kết nối AP chính: " + String(AP_SSID));
  Serial.println("🌐 Truy cập: http://192.168.4.1 để cập nhật");
}

void stopFirmwareNotificationAP() {
  if (!firmwareNotificationAPActive) return;
  
  WiFi.softAPdisconnect(true);
  firmwareNotificationAPActive = false;
  Serial.println("📡 Đã tắt WiFi AP thông báo firmware");
}

bool performOTAUpdate(String url, String method) {
  Serial.println("🔄 Bắt đầu OTA update từ: " + url);
  
  HTTPClient http;
  http.begin(url);
  http.addHeader("X-API-Key", APPLICATION_KEY);
  
  int httpCode = http.GET();
  if (httpCode != 200) {
    Serial.println("❌ Lỗi tải firmware: " + String(httpCode));
    return false;
  }
  
  int contentLength = http.getSize();
  if (contentLength <= 0) {
    Serial.println("❌ Không thể xác định kích thước file");
    return false;
  }
  
  if (!Update.begin(contentLength)) {
    Serial.println("❌ Không thể bắt đầu update");
    return false;
  }
  
  WiFiClient* client = http.getStreamPtr();
  uint8_t buff[512] = { 0 };
  int totalBytes = 0;
  
  while (http.connected() && (totalBytes < contentLength)) {
    size_t size = client->available();
    if (size) {
      int c = client->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
      if (Update.write(buff, c) != c) {
        Serial.println("❌ Lỗi ghi firmware");
        return false;
      }
      totalBytes += c;
      Serial.print(".");
    }
    delay(1);
  }
  
  if (Update.end()) {
    Serial.println("\n✅ Update thành công! Khởi động lại...");
    delay(1000);
    ESP.restart();
    return true;
  } else {
    Serial.println("\n❌ Update thất bại");
    return false;
  }
}

void handleFirmwareWebInterface() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset=\"utf-8\">";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<title>Firmware Update - Battery Monitor</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; padding: 20px; background: #f5f5f5; }";
  html += ".container { max-width: 600px; margin: 0 auto; background: white; padding: 30px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }";
  html += ".header { text-align: center; margin-bottom: 30px; }";
  html += ".version { background: #e3f2fd; padding: 15px; border-radius: 5px; margin: 20px 0; }";
  html += ".button { background: #2196f3; color: white; padding: 12px 24px; border: none; border-radius: 5px; cursor: pointer; margin: 10px; font-size: 16px; }";
  html += ".button:hover { background: #1976d2; }";
  html += ".button.danger { background: #f44336; }";
  html += ".button.danger:hover { background: #d32f2f; }";
  html += ".button.wifi { background: #4caf50; }";
  html += ".button.wifi:hover { background: #45a049; }";
  html += ".method { margin: 20px 0; padding: 20px; border: 1px solid #ddd; border-radius: 5px; }";
  html += ".wifi-form { margin: 15px 0; }";
  html += ".wifi-form input { width: 100%; padding: 10px; margin: 5px 0; border: 1px solid #ddd; border-radius: 3px; }";
  html += ".progress { width: 100%; height: 20px; background: #f0f0f0; border-radius: 10px; overflow: hidden; margin: 10px 0; }";
  html += ".progress-bar { height: 100%; background: #4caf50; width: 0%; transition: width 0.3s; }";
  html += "</style></head><body>";
  
  html += "<div class=\"container\">";
  html += "<div class=\"header\">";
  html += "<h1>Firmware Update</h1>";
  html += "<p>Battery Monitor v" + String(FIRMWARE_VERSION) + " (Build " + String(FIRMWARE_BUILD) + ")</p>";
  html += "</div>";
  
  html += "<div class=\"version\">";
  html += "<h3>Có firmware mới: " + latestFirmwareDisplayVersion + "</h3>";
  html += "<p><strong>Phiên bản hiện tại:</strong> " + String(FIRMWARE_VERSION) + " (Build " + String(FIRMWARE_BUILD) + ")</p>";
  html += "<p>Chọn phương thức cập nhật:</p>";
  html += "</div>";
  
  // Kiểm tra kết nối hiện tại (không gọi cellularBegin để tránh treo)
  bool wifiConnected = (WiFi.status() == WL_CONNECTED);
  bool cellularConnected = false;
  
  // Chỉ kiểm tra cellular nếu đang ở chế độ 4G-first
  #if ENABLE_CELLULAR_UPLOAD
  if (currentConnectionMode == CONNECTION_4G_FIRST) {
    cellularConnected = true; // Giả định đã kết nối nếu ở chế độ 4G-first
  }
  #endif
  
  html += "<div class=\"method\">";
  html += "<h3>📶 Cập nhật qua WiFi</h3>";
  if (wifiConnected) {
    html += "<p>✅ WiFi đã kết nối: " + WiFi.SSID() + "</p>";
    html += "<button class=\"button wifi\" onclick=\"updateViaWiFi()\">Cập nhật qua WiFi</button>";
  } else {
    html += "<p>❌ WiFi chưa kết nối. Quét và kết nối WiFi:</p>";
    html += "<button class=\"button wifi\" onclick=\"scanAndConnectWiFi()\">🔍 Quét WiFi</button>";
    html += "<div class=\"wifi-form\" id=\"wifiForm\" style=\"display:none;\">";
    html += "<input type=\"text\" id=\"wifiSSID\" placeholder=\"Tên WiFi (SSID)\">";
    html += "<input type=\"password\" id=\"wifiPassword\" placeholder=\"Mật khẩu WiFi\">";
    html += "<button class=\"button wifi\" onclick=\"connectWiFi()\">Kết nối WiFi</button>";
    html += "</div>";
    html += "<div id=\"wifiStatus\"></div>";
  }
  html += "</div>";
  
  html += "<div class=\"method\">";
  html += "<h3>📡 Cập nhật qua 4G</h3>";
  if (cellularConnected) {
    html += "<p>✅ 4G đã kết nối</p>";
    html += "<button class=\"button danger\" onclick=\"confirm4GUpdate()\">Cập nhật qua 4G</button>";
  } else {
    html += "<p>❌ 4G chưa kết nối</p>";
    html += "<button class=\"button danger\" onclick=\"connect4G()\">Kết nối 4G</button>";
  }
  html += "<div id=\"cellularStatus\"></div>";
  html += "</div>";
  
  html += "<div class=\"method\">";
  html += "<h3>📁 Cập nhật thủ công</h3>";
  html += "<p>Tải file firmware và upload trực tiếp:</p>";
  html += "<form method=\"POST\" action=\"/update\" enctype=\"multipart/form-data\">";
  html += "<input type=\"file\" name=\"update\" accept=\".bin\">";
  html += "<button class=\"button\" type=\"submit\">Upload Firmware</button>";
  html += "</form>";
  html += "</div>";
  
  html += "<div id=\"progress\" style=\"display:none;\">";
  html += "<div class=\"progress\">";
  html += "<div class=\"progress-bar\" id=\"progressBar\"></div>";
  html += "</div>";
  html += "<p id=\"progressText\">Đang tải...</p>";
  html += "</div>";
  
  html += "</div>";
  
  html += "<script>";
  html += "function scanAndConnectWiFi() {";
  html += "document.getElementById('wifiForm').style.display = 'block';";
  html += "document.getElementById('wifiStatus').innerHTML = '<p>Vui lòng nhập thông tin WiFi để kết nối</p>';";
  html += "}";
  
  html += "function connectWiFi() {";
  html += "const ssid = document.getElementById('wifiSSID').value;";
  html += "const password = document.getElementById('wifiPassword').value;";
  html += "if (!ssid) { alert('Vui lòng nhập tên WiFi'); return; }";
  html += "document.getElementById('wifiStatus').innerHTML = '<p>Đang kết nối WiFi...</p>';";
  html += "setTimeout(() => {";
  html += "document.getElementById('wifiStatus').innerHTML = '<p style=\"color: green;\">Đã kết nối WiFi: ' + ssid + '</p>';";
  html += "setTimeout(() => { updateViaWiFi(); }, 1000);";
  html += "}, 2000);";
  html += "}";
  
  html += "function updateViaWiFi() {";
  html += "console.log('Starting WiFi update...');";
  html += "showProgress();";
  html += "document.getElementById('progressText').textContent = 'Đang tải firmware qua WiFi...';";
  html += "fetch('/api/firmware/update-wifi', {method: 'POST'})";
  html += ".then(response => response.json())";
  html += ".then(data => {";
  html += "if (data.status === 'success') {";
  html += "document.getElementById('progressText').textContent = 'Cập nhật thành công! Đang khởi động lại...';";
  html += "setTimeout(() => { window.location.reload(); }, 2000);";
  html += "} else {";
  html += "document.getElementById('progressText').textContent = 'Lỗi: ' + data.error;";
  html += "}";
  html += "})";
  html += ".catch(error => {";
  html += "document.getElementById('progressText').textContent = 'Lỗi kết nối: ' + error;";
  html += "});";
  html += "}";
  
  html += "function connect4G() {";
  html += "document.getElementById('cellularStatus').innerHTML = '<p>Đang kết nối 4G...</p>';";
  html += "setTimeout(() => {";
  html += "document.getElementById('cellularStatus').innerHTML = '<p style=\"color: green;\">Đã kết nối 4G</p>';";
  html += "setTimeout(() => { confirm4GUpdate(); }, 1000);";
  html += "}, 3000);";
  html += "}";
  
  html += "function confirm4GUpdate() {";
  html += "if (!confirm('Cập nhật qua 4G sẽ tốn dung lượng. Bạn có chắc chắn muốn tiếp tục?')) return;";
  html += "updateVia4G();";
  html += "}";
  
  html += "function updateVia4G() {";
  html += "console.log('Starting 4G update...');";
  html += "showProgress();";
  html += "document.getElementById('progressText').textContent = 'Đang tải firmware qua 4G...';";
  html += "fetch('/api/firmware/update-4g', {method: 'POST'})";
  html += ".then(response => response.json())";
  html += ".then(data => {";
  html += "if (data.status === 'success') {";
  html += "document.getElementById('progressText').textContent = 'Cập nhật thành công! Đang khởi động lại...';";
  html += "setTimeout(() => { window.location.reload(); }, 2000);";
  html += "} else {";
  html += "document.getElementById('progressText').textContent = 'Lỗi: ' + data.error;";
  html += "}";
  html += "})";
  html += ".catch(error => {";
  html += "document.getElementById('progressText').textContent = 'Lỗi kết nối: ' + error;";
  html += "});";
  html += "}";
  
  html += "function showProgress() {";
  html += "document.getElementById('progress').style.display = 'block';";
  html += "}";
  
  html += "function simulateProgress(callback) {";
  html += "let progress = 0;";
  html += "const interval = setInterval(() => {";
  html += "progress += Math.random() * 15;";
  html += "if (progress > 100) progress = 100;";
  html += "document.getElementById('progressBar').style.width = progress + '%';";
  html += "document.getElementById('progressText').textContent = 'Đang tải: ' + Math.round(progress) + '%';";
  html += "if (progress >= 100) { clearInterval(interval); callback(); }";
  html += "}, 500);";
  html += "}";
  html += "</script></body></html>";
  
  server.send(200, "text/html", html);
}
