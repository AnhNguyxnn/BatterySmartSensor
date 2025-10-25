#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <WebServer.h>
#include <driver/adc.h>
#include <HTTPClient.h>
#include "cellular.h"
#include <ArduinoJson.h>
#include "config.h"
#include <time.h>
#include <Update.h>
#include <WiFiClientSecure.h>
#include "firmware_update.h"
#include <SPIFFS.h>
#include <esp_task_wdt.h>

// Cấu hình WiFi
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// HTTP Server
WebServer server(HTTP_SERVER_PORT);

// Khởi tạo cảm biến nhiệt độ
OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature tempSensor(&oneWire);

// Biến lưu trữ dữ liệu
float temperature = 0.0;
int smokeValue = 0;
bool smokeConnected = true;
bool fireDetected = false;
bool alertActive = false;

// Biến cho MQ2 filtering và preheat
int smokeHistory[MOVING_AVERAGE_SIZE];
int smokeHistoryIndex = 0;
bool mq2Preheated = false;
unsigned long mq2StartTime = 0;
int lastSmokeValue = 0;

// Biến thời gian
unsigned long lastSensorRead = 0;
unsigned long lastDataSend = 0;
unsigned long lastNtpSync = 0;
bool timeSynced = false;

// Biến firmware update
unsigned long lastFirmwareCheck = 0;
bool firmwareUpdateAvailable = false;
String latestFirmwareVersion = "";
String latestFirmwareUrl = "";
String latestFirmwareDisplayVersion = "";
bool firmwareNotificationAPActive = false;

// Biến trạng thái kết nối
enum ConnectionMode {
  CONNECTION_NONE = 0,
  CONNECTION_WIFI_FIRST = 1,  // Đã kết nối WiFi từ đầu, fallback 4G nếu WiFi mất
  CONNECTION_4G_FIRST = 2     // Đã kết nối 4G từ đầu, không thử WiFi nữa
};
ConnectionMode currentConnectionMode = CONNECTION_NONE;
bool connectionEstablished = false;

// Khai báo các hàm
void readSensors();
void checkAlerts();
void activateAlerts();
void deactivateAlerts();
void startNetworking();
void startWebServer();
String renderHtml();
void handleRoot();
void handleApiStatus();
void testSensors();
int medianFilter(int values[], int size);
int movingAverage(int newValue);
void tryBackendUpload();
void uploadImmediate();
void syncNTP();
unsigned long getCurrentTimestamp();
void checkFirmwareUpdate();
void startFirmwareNotificationAP();
void stopFirmwareNotificationAP();
bool performOTAUpdate(String url, String method);
void handleFirmwareWebInterface();
void startMainAP();
void handleWiFiScan();
void handleWiFiConnect();
void handleWiFiReset();
void saveWiFiConfig(String ssid, String password);
void handleFirmwareUpdateWiFi();
void handleFirmwareUpdate4G();
void handleFirmwareCheck();
void forceSyncNTP();
void networkTask(void* param);
void startMainAP();

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  Serial.println("=== ESP32 CHIP INFO ===");
  Serial.println("Chip Model: " + String(ESP.getChipModel()));
  Serial.println("Chip Revision: " + String(ESP.getChipRevision()));
  Serial.println("Chip Cores: " + String(ESP.getChipCores()));
  Serial.println("Chip Frequency: " + String(ESP.getCpuFreqMHz()) + " MHz");
  Serial.println("Flash Size: " + String(ESP.getFlashChipSize() / 1024 / 1024) + " MB");
  Serial.println("Flash Speed: " + String(ESP.getFlashChipSpeed() / 1000000) + " MHz");
  Serial.println("Free Heap: " + String(ESP.getFreeHeap()) + " bytes");
  Serial.println("MAC Address: " + WiFi.macAddress());
  Serial.println("🚀 ESP32 Battery Monitor - Fast Boot Starting...");
  
  // Khởi tạo watchdog với timeout dài hơn
  esp_task_wdt_init(60, true);
  esp_task_wdt_add(NULL);
  
  // Fast Boot Path - chỉ khởi tạo tối thiểu
  Serial.println("⚡ Fast Boot Path - khởi tạo tối thiểu...");
  
  // SPIFFS mount
  if (!SPIFFS.begin(true)) {
    Serial.println("❌ SPIFFS mount failed");
  } else {
    Serial.println("✅ SPIFFS đã khởi tạo");
  }
  
  // GPIO setup
  #if FIRE_INPUT_PULLUP
    pinMode(FIRE_SENSOR_PIN, INPUT_PULLUP);
  #else
    pinMode(FIRE_SENSOR_PIN, INPUT);
  #endif
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  
  // Bật LED báo đang boot
  digitalWrite(LED_PIN, HIGH);
  
  // Khởi tạo cảm biến nhiệt độ
  tempSensor.begin();
  
  // Khởi tạo ADC cho MQ2
  analogReadResolution(12);
  analogSetPinAttenuation(SMOKE_SENSOR_PIN, ADC_11db);
  
  // Khởi tạo MQ2 preheat timer
  mq2StartTime = millis();
  
  // Start AP management ngay để user có thể truy cập web sớm
  startMainAP();
  startWebServer();
  
  // Bật task riêng cho phần network để chạy song song
  xTaskCreatePinnedToCore(networkTask, "networkTask", 8192, NULL, 1, NULL, 1);
  
  Serial.println("⚡ Fast Boot Path done (<5s) - Web interface ready!");
  Serial.println("🌐 Network initialization running in background...");
}

// Network Task chạy song song để khởi tạo mạng
void networkTask(void* param) {
  esp_task_wdt_add(NULL);
  
  Serial.println("🌐 Bắt đầu khởi tạo mạng nền...");

  // Ưu tiên 4G
  if (cellularBegin()) {
    currentConnectionMode = CONNECTION_4G_FIRST;
    connectionEstablished = true;
    Serial.println("✅ 4G connected (background)");
  } else {
    Serial.println("❌ 4G fail, thử WiFi...");
    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    WiFi.setAutoReconnect(true);
    WiFi.setAutoConnect(true);
    
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 8000) {
      delay(500);
      esp_task_wdt_reset();
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("✅ WiFi connected (background)");
      currentConnectionMode = CONNECTION_WIFI_FIRST;
      connectionEstablished = true;
    }
  }

  if (connectionEstablished) {
    Serial.println("🌐 Bắt đầu đồng bộ NTP & gửi dữ liệu khởi động...");
    esp_task_wdt_reset(); // Reset watchdog before NTP sync
    forceSyncNTP();
    esp_task_wdt_reset(); // Reset watchdog after NTP sync
    readSensors();
    esp_task_wdt_reset(); // Reset watchdog after sensor read
    uploadImmediate();
    esp_task_wdt_reset(); // Reset watchdog after upload
    
    // Dời firmware check sau upload đầu tiên
    Serial.println("🔍 Kiểm tra firmware update...");
    checkFirmwareUpdate();
    esp_task_wdt_reset(); // Reset watchdog after firmware check
  } else {
    Serial.println("❌ Không thể kết nối mạng");
  }

  digitalWrite(LED_PIN, LOW);
  Serial.println("🌐 Network task completed");
  // Remove this task from watchdog tracking before deletion to avoid WDT referencing a freed TCB
  esp_task_wdt_delete(NULL);
  vTaskDelete(NULL); // Kết thúc task
}

void loop() {
  // Reset watchdog timer để tránh crash - reset thường xuyên hơn
  esp_task_wdt_reset();
  
  unsigned long currentTime = millis();

  // Đọc dữ liệu từ các cảm biến
  if (currentTime - lastSensorRead >= SENSOR_INTERVAL) {
    readSensors();
    checkAlerts();
    lastSensorRead = currentTime;
    esp_task_wdt_reset(); // Reset after sensor operations
  }

  // Xử lý request HTTP
  server.handleClient();
  esp_task_wdt_reset(); // Reset after HTTP handling

  // Sync NTP định kỳ
  if (currentTime - lastNtpSync >= NTP_SYNC_INTERVAL) {
    syncNTP();
    lastNtpSync = currentTime;
    esp_task_wdt_reset(); // Reset after NTP sync
  }

  // Kiểm tra firmware update định kỳ (12 giờ) - chỉ khi có kết nối internet
  if (currentTime - lastFirmwareCheck >= FIRMWARE_CHECK_INTERVAL) {
    // Chỉ kiểm tra firmware nếu đã có kết nối internet ổn định
    if (WiFi.status() == WL_CONNECTED || currentConnectionMode == CONNECTION_4G_FIRST) {
      Serial.println("🔍 Kiểm tra firmware update định kỳ...");
      checkFirmwareUpdate();
    } else {
      Serial.println("[FIRMWARE] Bỏ qua kiểm tra - chưa có kết nối internet");
    }
    lastFirmwareCheck = currentTime;
    esp_task_wdt_reset(); // Reset after firmware check
  }

  // Upload dữ liệu định kỳ (ưu tiên WiFi, fallback 4G) qua HTTP
  tryBackendUpload();
  esp_task_wdt_reset(); // Reset after upload attempt

  // Tăng delay để giảm tải CPU và cho phép các task khác chạy
  delay(100);
}

void readSensors() {
  // Đọc nhiệt độ từ DS18B20
  tempSensor.requestTemperatures();
  temperature = tempSensor.getTempCByIndex(0);
  
  // // Kiểm tra cảm biến nhiệt độ
  // if (temperature == DEVICE_DISCONNECTED_C) {
  //   Serial.println("❌ Lỗi: Không tìm thấy cảm biến DS18B20!");
  //   Serial.println("Kiểm tra kết nối: VCC->3.3V, GND->GND, Data->GPIO23");
  //   Serial.println("Cần điện trở pull-up 4.7kΩ giữa Data và VCC");
  // }
  
  // Đọc giá trị cảm biến khói MQ2 với filtering nâng cao
  int rawSamples[MEDIAN_FILTER_SIZE];
  int minSample = 4095;
  int maxSample = 0;
  
  // Lấy nhiều mẫu để median filter
  for (int i = 0; i < MEDIAN_FILTER_SIZE; i++) {
    int v = analogRead(SMOKE_SENSOR_PIN);
    rawSamples[i] = v;
    if (v < minSample) minSample = v;
    if (v > maxSample) maxSample = v;
    // Reduce delay and add watchdog reset every few samples
    if (i % 5 == 0) {
      esp_task_wdt_reset();
    }
    delayMicroseconds(100); // Much shorter delay
  }
  
  // Áp dụng median filter để loại bỏ nhiễu
  int medianValue = medianFilter(rawSamples, MEDIAN_FILTER_SIZE);
  
  // Áp dụng moving average để làm mượt
  smokeValue = movingAverage(medianValue);
  
  // Kiểm tra kết nối cảm biến
  smokeConnected = (maxSample - minSample) < SMOKE_FLOAT_RANGE;
  
  // Kiểm tra preheat MQ2
  unsigned long preheatElapsed = millis() - mq2StartTime;
  if (!mq2Preheated && preheatElapsed >= MQ2_PREHEAT_TIME_MS) {
    // Kiểm tra độ ổn định trong 30 giây cuối
    if (abs(smokeValue - lastSmokeValue) < MQ2_STABLE_THRESHOLD) {
      mq2Preheated = true;
      Serial.println("✅ MQ2 đã preheat xong và sẵn sàng!");
    }
  }
  lastSmokeValue = smokeValue;
  
  // Đọc cảm biến cháy IR
  int irRawValue = digitalRead(FIRE_SENSOR_PIN);
  fireDetected = (irRawValue == FIRE_THRESHOLD);
  
  // In dữ liệu ra Serial Monitor
  // Serial.println("=== Dữ liệu cảm biến ===");
  // Serial.print("Nhiệt độ DS18B20: ");
  // Serial.print(temperature);
  // Serial.println(" °C");
  // Serial.print("Giá trị khói MQ2: ");
  // Serial.print(smokeValue);
  // Serial.print(" (raw: ");
  // Serial.print(medianValue);
  // Serial.print(") | trạng thái: ");
  // Serial.print(smokeConnected ? "đã kết nối" : "CHƯA KẾT NỐI");
  // Serial.print(" | preheat: ");
  // Serial.println(mq2Preheated ? "✅ OK" : "⏳ Đang preheat...");
  // Serial.print("Cảm biến cháy IR (raw): ");
  // Serial.println(irRawValue);
  // Serial.print("Cảm biến cháy IR: ");
  // Serial.println(fireDetected ? "CÓ CHÁY!" : "Bình thường");
  // Serial.println("========================");
}

void checkAlerts() {
  bool tempAlert = (temperature > TEMP_THRESHOLD);
  bool smokeAlert = (smokeValue > SMOKE_THRESHOLD);
  bool fireDetectedAlert = fireDetected;

  // Logic phát hiện cháy theo thứ tự ưu tiên:
  // 1. NHIỆT ĐỘ CAO = BÁO CHÁY NGAY LẬP TỨC (ưu tiên cao nhất)
  // 2. MQ2 + NHIỆT CAO = BÁO CHÁY (pin xì khí + nhiệt)
  // 3. IR FIRE = CHỈ LÀ ĐIỀU KIỆN PHỤ (ưu tiên thấp nhất)
  
  bool shouldAlert = false;
  String alertReason = "";

  if (tempAlert) {
    // Ưu tiên 1: Nhiệt độ cao = báo cháy ngay
    shouldAlert = true;
    alertReason = "🔥 NHIỆT ĐỘ CAO (" + String(temperature, 1) + "°C)";
    
    if (smokeAlert) {
      // Nhiệt cao + khí = pin có thể đang xì khí nguy hiểm
      alertReason += " + KHÍ ĐỘC HẠI (" + String(smokeValue) + ")";
    }
  } else if (smokeAlert && temperature > (TEMP_THRESHOLD * TEMP_SMOKE_THRESHOLD_MULTIPLIER)) {
    // Ưu tiên 2: MQ2 + nhiệt tương đối cao = pin xì khí
    shouldAlert = true;
    alertReason = "⚠️ PIN XÌ KHÍ: Khí độc (" + String(smokeValue) + ") + Nhiệt (" + String(temperature, 1) + "°C)";
  } else if (fireDetectedAlert && !tempAlert && !smokeAlert) {
    // Ưu tiên 3: IR Fire chỉ khi không có nhiệt/khí (có thể false positive)
    shouldAlert = true;
    alertReason = "🔍 CẢM BIẾN IR PHÁT HIỆN CHÁY (cần xác minh)";
  }

  // Kích hoạt/tắt cảnh báo
  if (shouldAlert) {
    if (!alertActive) {
      alertActive = true;
      Serial.println("🚨 CẢNH BÁO: " + alertReason);
      activateAlerts();
      // Gửi ngay lập tức khi có cảnh báo
      uploadImmediate();
    }
  } else {
    if (alertActive) {
      alertActive = false;
      Serial.println("✅ Tình trạng bình thường");
      deactivateAlerts();
    }
  }
}

void activateAlerts() {
  // Bật LED và còi cảnh báo
  digitalWrite(LED_PIN, HIGH);
  digitalWrite(BUZZER_PIN, HIGH);

  // In thông báo chi tiết theo thứ tự ưu tiên
  Serial.println("=== CHI TIẾT CẢNH BÁO ===");
  
  if (temperature > TEMP_THRESHOLD) {
    Serial.println("🔥 NHIỆT ĐỘ CAO: " + String(temperature, 1) + "°C (Nguy hiểm!)");
  }
  
  if (smokeValue > SMOKE_THRESHOLD) {
    Serial.println("💨 KHÍ ĐỘC HẠI: " + String(smokeValue) + " (Pin có thể xì khí)");
  }
  
  if (fireDetected) {
    Serial.println("🔍 IR FIRE: Phát hiện cháy (Cần xác minh)");
  }
  
  Serial.println("=========================");
}

void deactivateAlerts() {
  // Tắt LED và còi cảnh báo
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
}

void startNetworking() {
  Serial.println("🌐 Bắt đầu kết nối mạng...");
  
  // Bước 0: Luôn khởi tạo AP chính trước (đảm bảo AP luôn có)
  Serial.println("📡 Khởi tạo AP chính trước...");
  startMainAP();
  
  // Bước 1: Thử kết nối 4G trước (ưu tiên cao nhất)
  #if ENABLE_CELLULAR_UPLOAD
  Serial.println("📡 Thử kết nối 4G...");
  
  // Thử kết nối 4G với retry logic
  bool cellularConnected = false;
  for (int retry = 0; retry < 2 && !cellularConnected; retry++) {
    if (retry > 0) {
      Serial.printf("[CELL] Thử lại kết nối 4G lần %d...\n", retry + 1);
      cellularReset(); // Reset modem trước khi thử lại
      delay(5000); // Chờ modem khởi động lại
    }
    
    if (cellularBegin()) {
      cellularConnected = true;
      Serial.println("✅ 4G đã kết nối!");
      currentConnectionMode = CONNECTION_4G_FIRST;
      connectionEstablished = true;
      return; // Thành công với 4G, không cần WiFi
    } else {
      Serial.printf("❌ 4G kết nối thất bại lần %d\n", retry + 1);
    }
  }
  
  if (!cellularConnected) {
    Serial.println("❌ 4G kết nối thất bại sau 2 lần thử");
  }
  #endif
  
  // Bước 2: Thử kết nối WiFi từ config (fallback)
  WiFi.mode(WIFI_AP_STA); // Đảm bảo AP vẫn hoạt động
  WiFi.begin(ssid, password);
  Serial.print("📶 Đang kết nối WiFi: " + String(ssid));
  
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("✅ WiFi đã kết nối!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    currentConnectionMode = CONNECTION_WIFI_FIRST;
    connectionEstablished = true;
    return;
  }
  
  Serial.println("❌ WiFi kết nối thất bại");
  Serial.println("📡 Chỉ có AP chính hoạt động");
}

void startMainAP() {
  // AP chính hoạt động liên tục để quản trị
  // Luôn giữ AP bật, không bị ảnh hưởng bởi STA
  WiFi.mode(WIFI_AP_STA);
  
  bool apStarted = WiFi.softAP(AP_SSID, AP_PASSWORD);
  
  if (apStarted) {
    IPAddress apIP = WiFi.softAPIP();
    Serial.println("📡 AP Quản trị đã khởi động:");
    Serial.print("SSID: "); Serial.println(AP_SSID);
    Serial.print("Password: "); Serial.println(AP_PASSWORD);
    Serial.print("IP: "); Serial.println(apIP);
    Serial.println("📱 Truy cập: http://192.168.4.1");
    Serial.println("🔒 AP này sẽ luôn hoạt động, không bị tắt khi kết nối WiFi");
    
    // Đăng ký routes cho quản trị
    server.on("/wifi-scan", handleWiFiScan);
    server.on("/wifi-connect", HTTP_POST, handleWiFiConnect);
    server.on("/wifi-reset", HTTP_POST, handleWiFiReset);
    server.on("/firmware-update", handleFirmwareWebInterface);
  } else {
    Serial.println("❌ Không thể khởi động AP quản trị!");
  }
}


void startWebServer() {
  server.on("/", handleRoot);
  server.on("/api/status", HTTP_GET, handleApiStatus);
  
  // WiFi setup routes (luôn có sẵn)
  server.on("/wifi-scan", handleWiFiScan);
  server.on("/wifi-connect", HTTP_POST, handleWiFiConnect);
  server.on("/wifi-reset", HTTP_POST, handleWiFiReset);
  
  // Firmware update routes
  server.on("/firmware-update", handleFirmwareWebInterface);
  server.on("/api/firmware/check", HTTP_GET, handleFirmwareCheck);
  server.on("/api/firmware/update-wifi", HTTP_POST, handleFirmwareUpdateWiFi);
  server.on("/api/firmware/update-4g", HTTP_POST, handleFirmwareUpdate4G);
  
  // Time sync route
  server.on("/api/time-sync", HTTP_POST, []() {
    Serial.println("🕐 Force sync NTP từ web interface...");
    forceSyncNTP();
    server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Time sync completed\"}");
  });
  
  server.begin();
  Serial.print("HTTP server lắng nghe tại port ");
  Serial.println(HTTP_SERVER_PORT);
}

String renderHtml() {
  String html = "<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>Battery Monitor - Admin Panel</title><style>body{font-family:Arial;padding:16px} .card{border:1px solid #ddd;border-radius:8px;padding:12px;margin:8px 0} .ok{color:#2e7d32}.warn{color:#d32f2f} .grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:12px} .network-status{padding:10px;border-radius:5px;margin:10px 0} .wifi-connected{background:#e8f5e8;border-left:4px solid #4caf50} .cellular-connected{background:#e3f2fd;border-left:4px solid #2196f3} .ap-mode{background:#fff3e0;border-left:4px solid #ff9800} .button{background:#2196f3;color:white;padding:8px 16px;border:none;border-radius:3px;cursor:pointer;margin:5px;text-decoration:none;display:inline-block} .firmware-notification{background:#ffebee;border:2px solid #f44336;border-radius:8px;padding:15px;margin:15px 0} .firmware-notification h3{color:#d32f2f;margin-top:0} .close-btn{float:right;background:#f44336;color:white;border:none;padding:5px 10px;border-radius:3px;cursor:pointer}</style></head><body>";
  html += "<h2>🔋 Battery Smart Sensor - Admin Panel</h2>";
  
  // Firmware notification banner
  if (firmwareUpdateAvailable) {
    html += "<div class='firmware-notification'>";
    html += "<button class='close-btn' onclick='this.parentElement.style.display=\"none\"'>✕</button>";
    html += "<h3>🔄 Firmware Update Available!</h3>";
    html += "<p><strong>New Version:</strong> " + latestFirmwareDisplayVersion + "</p>";
    html += "<p><strong>Current Version:</strong> " + String(FIRMWARE_VERSION) + " (Build " + String(FIRMWARE_BUILD) + ")</p>";
    html += "<a href='/firmware-update' class='button'>Update Now</a>";
    html += "<button class='button' onclick='this.parentElement.style.display=\"none\"'>Update Later</button>";
    html += "</div>";
  }
  
  // Network status
  html += "<div class='network-status ";
  if (currentConnectionMode == CONNECTION_4G_FIRST) {
    html += "cellular-connected'>";
    html += "<strong>📡 4G-First Mode</strong><br>";
    html += "Ưu tiên 4G, không thử WiFi<br>";
    #if ENABLE_CELLULAR_UPLOAD
    bool cellularConnected = cellularBegin();
    html += cellularConnected ? "✅ 4G Connected" : "❌ 4G Disconnected";
    #endif
  } else if (currentConnectionMode == CONNECTION_WIFI_FIRST) {
    html += "wifi-connected'>";
    html += "<strong>📶 WiFi-First Mode</strong><br>";
    if (WiFi.status() == WL_CONNECTED) {
      html += "SSID: " + WiFi.SSID() + "<br>";
      html += "IP: " + WiFi.localIP().toString() + "<br>";
      html += "✅ WiFi Connected";
    } else {
      html += "❌ WiFi Disconnected<br>";
      html += "Fallback to 4G";
    }
  } else {
    html += "ap-mode'>";
    html += "<strong>📡 AP Mode Only</strong><br>";
    html += "No internet connection<br>";
    html += "Configure WiFi connection";
  }
  html += "</div>";
  
  html += "<div class='grid'>";
  html += "<div class='card'><h3>🌡️ Temperature</h3><div>" + String(temperature, 1) + " °C</div></div>";
  html += "<div class='card'><h3>💨 Smoke</h3><div>" + String(smokeValue) + (smokeConnected ? "" : " (not connected)") + "</div><small>" + (mq2Preheated ? "✅ Ready" : "⏳ Preheating...") + "</small></div>";
  html += String("<div class='card'><h3>🔥 Fire</h3><div>") + (fireDetected ? "DETECTED" : "Normal") + "</div></div>";
  html += String("<div class='card'><h3>🚨 Alert</h3><div class='") + (alertActive ? "warn'>CẢNH BÁO" : "ok'>Bình thường") + "</div></div>";
  html += "</div>";
  
  // Admin actions
  html += "<div style='margin:20px 0'>";
  html += "<h3>🔧 Admin Actions</h3>";
  html += "<a href='/wifi-scan' class='button'>🔍 WiFi Setup</a>";
  html += "<button class='button' onclick='checkFirmwareUpdate()'>🔄 Firmware Update</button>";
  html += "<a href='/api/status' class='button'>📊 API Status</a>";
  html += "<button class='button' onclick='syncTime()'>🕐 Sync Time</button>";
  html += "</div>";
  
  html += "<p><small>Device: " DEVICE_ID " | Firmware: " + String(FIRMWARE_VERSION) + " (Build " + String(FIRMWARE_BUILD) + ")</small></p>";
  html += "<script>";
  html += "function syncTime() {";
  html += "  fetch('/api/time-sync', {method: 'POST'})";
  html += "    .then(r => r.json())";
  html += "    .then(d => {";
  html += "      alert('Time sync completed!');";
  html += "      location.reload();";
  html += "    })";
  html += "    .catch(e => alert('Time sync failed: ' + e));";
  html += "}";
  html += "function checkFirmwareUpdate() {";
  html += "  fetch('/api/firmware/check')";
  html += "    .then(r => r.json())";
  html += "    .then(d => {";
  html += "      if (d.update_available) {";
  html += "        if (confirm('Có firmware mới: ' + d.latest_version + '\\n\\nBạn có muốn cập nhật không?')) {";
  html += "          window.location.href = '/firmware-update';";
  html += "        }";
  html += "      } else {";
  html += "        alert('✅ Đã là phiên bản mới nhất: ' + d.current_version + ' (Build ' + d.current_build + ')');";
  html += "      }";
  html += "    })";
  html += "    .catch(e => alert('Kiểm tra firmware thất bại: ' + e));";
  html += "}";
  html += "setInterval(()=>fetch('/api/status').then(r=>r.json()).then(d=>{location.reload()}),5000)";
  html += "</script>";
  html += "</body></html>";
  return html;
}

void handleRoot() {
  server.send(200, "text/html", renderHtml());
}

void handleApiStatus() {
  String json;
  {
    JsonDocument doc;
    // Không gửi timestamp trong API status, chỉ gửi dữ liệu cảm biến
    // doc["timestamp"] = getCurrentTimestamp();
    doc["temperature"] = temperature;
    doc["smoke_value"] = smokeValue;
    doc["smoke_connected"] = smokeConnected;
    doc["mq2_preheated"] = mq2Preheated;
    doc["fire_detected"] = fireDetected;
    doc["alert_active"] = alertActive;
    doc["device_id"] = DEVICE_ID;
    serializeJson(doc, json);
  }
  server.send(200, "application/json", json);
}

void tryBackendUpload() {
  static unsigned long lastUpload = 0;
  const unsigned long interval = 60000; // mỗi 60s
  unsigned long now = millis();
  if (now - lastUpload < interval) return;
  lastUpload = now;

  esp_task_wdt_reset(); // Reset watchdog before upload
  JsonDocument doc;
  // Không gửi timestamp, để server tự tạo
  // doc["timestamp"] = getCurrentTimestamp();
  doc["temperature"] = temperature;
  doc["smoke_value"] = smokeValue;
  doc["smoke_connected"] = smokeConnected;
  doc["mq2_preheated"] = mq2Preheated;
  doc["fire_detected"] = fireDetected;
  doc["alert_active"] = alertActive;
  doc["device_id"] = DEVICE_ID;
  String body;
  serializeJson(doc, body);

  // Logic upload theo chế độ kết nối
  if (currentConnectionMode == CONNECTION_4G_FIRST) {
    // Chế độ 4G-first: chỉ dùng 4G, không thử WiFi
    #if ENABLE_CELLULAR_UPLOAD
    static int uploadFailCount = 0;
    
    if (cellularBegin()) {
      String resp;
      bool ok = cellularHttpPost(BACKEND_HOST, BACKEND_PORT, BACKEND_PATH, body, resp);
      if (ok) {
        Serial.println(String("[CELL] Upload OK: ") + resp);
        uploadFailCount = 0; // Reset counter khi thành công
      } else {
        Serial.println("[CELL] Upload FAIL");
        uploadFailCount++;
        
        // Nếu upload thất bại nhiều lần, reset modem
        if (uploadFailCount >= 3) {
          Serial.println("[CELL] Upload thất bại nhiều lần, reset modem...");
          cellularReset();
          uploadFailCount = 0;
        }
      }
    } else {
      Serial.println("[CELL] Không thể kết nối mạng 4G");
      uploadFailCount++;
      
      // Nếu không kết nối được nhiều lần, reset modem
      if (uploadFailCount >= 2) {
        Serial.println("[CELL] Không kết nối được nhiều lần, reset modem...");
        cellularReset();
        uploadFailCount = 0;
      }
    }
    #endif
  } else if (currentConnectionMode == CONNECTION_WIFI_FIRST) {
    // Chế độ WiFi-first: ưu tiên WiFi, fallback 4G nếu WiFi mất
    if (WiFi.status() == WL_CONNECTED) {
      String url = String("http://") + String(BACKEND_HOST) + ":" + String(BACKEND_PORT) + String(BACKEND_PATH);
      HTTPClient http;
      http.begin(url);
      http.addHeader("Content-Type", "application/json");
      http.addHeader("X-API-Key", APPLICATION_KEY);
      int code = http.POST(body);
      String resp = http.getString();
      http.end();
      Serial.print("[WIFI] Upload "); Serial.println(code);
      if (code >= 200 && code < 300) return; // thành công thì không cần 4G
    }
    
    // Fallback 4G nếu WiFi thất bại
    #if ENABLE_CELLULAR_UPLOAD
    Serial.println("[WIFI] WiFi mất kết nối, thử 4G...");
    if (cellularBegin()) {
      String resp;
      bool ok = cellularHttpPost(BACKEND_HOST, BACKEND_PORT, BACKEND_PATH, body, resp);
      Serial.println(ok ? String("[CELL] Upload OK: ") + resp : "[CELL] Upload FAIL");
    } else {
      Serial.println("[CELL] Không thể kết nối mạng 4G");
    }
    #endif
  }
}

void uploadImmediate() {
  // Gửi ngay (không chờ interval), theo chế độ kết nối
  esp_task_wdt_reset(); // Reset watchdog before immediate upload
  JsonDocument doc;
  // Không gửi timestamp, để server tự tạo
  // doc["timestamp"] = getCurrentTimestamp();
  doc["temperature"] = temperature;
  doc["smoke_value"] = smokeValue;
  doc["smoke_connected"] = smokeConnected;
  doc["mq2_preheated"] = mq2Preheated;
  doc["fire_detected"] = fireDetected;
  doc["alert_active"] = alertActive;
  doc["device_id"] = DEVICE_ID;
  String body;
  serializeJson(doc, body);

  // Logic upload theo chế độ kết nối
  if (currentConnectionMode == CONNECTION_4G_FIRST) {
    // Chế độ 4G-first: chỉ dùng 4G
    #if ENABLE_CELLULAR_UPLOAD
    static int immediateFailCount = 0;
    
    if (cellularBegin()) {
      String resp;
      bool ok = cellularHttpPost(BACKEND_HOST, BACKEND_PORT, BACKEND_PATH, body, resp);
      if (ok) {
        Serial.println(String("[CELL] Upload immediate OK: ") + resp);
        immediateFailCount = 0; // Reset counter khi thành công
      } else {
        Serial.println("[CELL] Upload immediate FAIL");
        immediateFailCount++;
        
        // Nếu upload thất bại nhiều lần, reset modem
        if (immediateFailCount >= 2) {
          Serial.println("[CELL] Immediate upload thất bại nhiều lần, reset modem...");
          cellularReset();
          immediateFailCount = 0;
        }
      }
    } else {
      Serial.println("[CELL] Immediate: Không thể kết nối mạng 4G");
      immediateFailCount++;
      
      // Nếu không kết nối được nhiều lần, reset modem
      if (immediateFailCount >= 2) {
        Serial.println("[CELL] Immediate: Không kết nối được nhiều lần, reset modem...");
        cellularReset();
        immediateFailCount = 0;
      }
    }
    #endif
  } else if (currentConnectionMode == CONNECTION_WIFI_FIRST) {
    // Chế độ WiFi-first: ưu tiên WiFi, fallback 4G nếu WiFi mất
    if (WiFi.status() == WL_CONNECTED) {
      String url = String("http://") + String(BACKEND_HOST) + ":" + String(BACKEND_PORT) + String(BACKEND_PATH);
      HTTPClient http;
      http.begin(url);
      http.addHeader("Content-Type", "application/json");
      http.addHeader("X-API-Key", APPLICATION_KEY);
      int code = http.POST(body);
      String resp = http.getString();
      http.end();
      Serial.print("[WIFI] Upload immediate "); Serial.println(code);
      if (code >= 200 && code < 300) return; // thành công thì không cần 4G
    }
    
    // Fallback 4G nếu WiFi thất bại
    #if ENABLE_CELLULAR_UPLOAD
    Serial.println("[WIFI] WiFi mất kết nối, thử 4G immediate...");
    if (cellularBegin()) {
      String resp;
      bool ok = cellularHttpPost(BACKEND_HOST, BACKEND_PORT, BACKEND_PATH, body, resp);
      Serial.println(ok ? String("[CELL] Upload immediate OK: ") + resp : "[CELL] Upload immediate FAIL");
    } else {
      Serial.println("[CELL] Immediate: Không thể kết nối mạng 4G");
    }
    #endif
  }
}

void testSensors() {
  Serial.println("🔍 Kiểm tra cảm biến nhiệt độ DS18B20...");
  tempSensor.begin();
  int deviceCount = tempSensor.getDeviceCount();
  Serial.print("Số lượng cảm biến DS18B20 tìm thấy: ");
  Serial.println(deviceCount);
  
  if (deviceCount == 0) {
    Serial.println("❌ Không tìm thấy cảm biến DS18B20!");
    Serial.println("Kiểm tra:");
    Serial.println("- VCC kết nối với 3.3V hoặc 5V");
    Serial.println("- GND kết nối với GND");
    Serial.println("- Data kết nối với GPIO 4");
    Serial.println("- Có điện trở pull-up 4.7kΩ giữa Data và VCC");
  } else {
    tempSensor.requestTemperatures();
    float testTemp = tempSensor.getTempCByIndex(0);
    Serial.print("✅ Nhiệt độ đọc được: ");
    Serial.print(testTemp);
    Serial.println(" °C");
  }
  
  Serial.println("🔍 Kiểm tra cảm biến cháy IR...");
  int irValue = digitalRead(FIRE_SENSOR_PIN);
  Serial.print("Giá trị raw cảm biến IR: ");
  Serial.println(irValue);
  Serial.println("(0 = LOW, 1 = HIGH)");
  
  Serial.println("🔍 Kiểm tra cảm biến khói MQ2...");
  int smokeTest = analogRead(SMOKE_SENSOR_PIN);
  Serial.print("Giá trị analog MQ2: ");
  Serial.println(smokeTest);
  Serial.println("(0-4095, giá trị cao = nhiều khói)");
  
  Serial.println("🔍 Test LED và Buzzer...");
  digitalWrite(LED_PIN, HIGH);
  digitalWrite(BUZZER_PIN, HIGH);
  delay(500);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  Serial.println("✅ LED và Buzzer hoạt động bình thường");
  
  Serial.println("=== KẾT THÚC TEST ===");
}

// Hàm median filter để loại bỏ nhiễu
int medianFilter(int values[], int size) {
  // Sắp xếp mảng
  for (int i = 0; i < size - 1; i++) {
    for (int j = i + 1; j < size; j++) {
      if (values[i] > values[j]) {
        int temp = values[i];
        values[i] = values[j];
        values[j] = temp;
      }
    }
  }
  // Trả về giá trị ở giữa
  return values[size / 2];
}

// Hàm moving average để làm mượt dữ liệu
int movingAverage(int newValue) {
  smokeHistory[smokeHistoryIndex] = newValue;
  smokeHistoryIndex = (smokeHistoryIndex + 1) % MOVING_AVERAGE_SIZE;
  
  long sum = 0;
  for (int i = 0; i < MOVING_AVERAGE_SIZE; i++) {
    sum += smokeHistory[i];
  }
  return (int)(sum / MOVING_AVERAGE_SIZE);
}

void syncNTP() {
  // Sync NTP cho cả WiFi và 4G
  bool hasInternet = (WiFi.status() == WL_CONNECTED) || (currentConnectionMode == CONNECTION_4G_FIRST);
  
  if (!hasInternet) {
    Serial.println("[NTP] Không có kết nối internet, bỏ qua sync");
    return;
  }
  
  Serial.println("[NTP] Đang sync thời gian...");
  
  // Reset timeSynced để force sync lại
  timeSynced = false;
  
  // Cấu hình timezone và NTP server
  configTime(TIMEZONE_OFFSET * 3600, 0, NTP_SERVER);
  
  // Chờ sync với timeout ngắn hơn để setup nhanh
  int attempts = 0;
  while (!timeSynced && attempts < 20) { // Giảm từ 50 xuống 20 để nhanh hơn
    esp_task_wdt_reset(); // Reset watchdog during NTP sync
    time_t now = time(nullptr);
    
    // Kiểm tra timestamp hợp lệ (từ 2020 đến 2030)
    if (now > 1577836800 && now < 1893456000) { // 2020-01-01 đến 2030-01-01
      timeSynced = true;
      struct tm timeinfo;
      localtime_r(&now, &timeinfo);
      Serial.printf("[NTP] ✅ Thời gian đã sync: %04d-%02d-%02d %02d:%02d:%02d (UTC+%d)\n",
                   timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                   timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, TIMEZONE_OFFSET);
      Serial.printf("[NTP] Unix timestamp: %lu\n", (unsigned long)now);
      return;
    }
    delay(50); // Giảm từ 100ms xuống 50ms
    attempts++;
  }
  
  if (!timeSynced) {
    Serial.println("[NTP] ❌ Không thể sync thời gian sau 1 giây");
    Serial.printf("[NTP] Timestamp hiện tại: %lu (có thể không hợp lệ)\n", (unsigned long)time(nullptr));
  }
}

unsigned long getCurrentTimestamp() {
  if (timeSynced) {
    time_t now = time(nullptr);
    // Kiểm tra timestamp hợp lệ trước khi trả về
    if (now > 1577836800 && now < 1893456000) { // 2020-01-01 đến 2030-01-01
      return (unsigned long)now;
    } else {
      Serial.printf("[TIMESTAMP] ⚠️ Timestamp không hợp lệ: %lu, fallback về millis\n", (unsigned long)now);
      timeSynced = false; // Reset flag để sync lại
    }
  }
  
  // Fallback về millis() nếu chưa sync được hoặc timestamp không hợp lệ
  // Thêm offset để timestamp không quá nhỏ
  static unsigned long bootTime = 0;
  if (bootTime == 0) {
    bootTime = millis() / 1000;
  }
  
  unsigned long fallbackTime = (millis() / 1000) - bootTime + 1577836800; // Offset từ 2020-01-01 (cố định)
  Serial.printf("[TIMESTAMP] Fallback timestamp: %lu (từ millis + offset)\n", fallbackTime);
  return fallbackTime;
}

// Hàm force sync NTP với retry logic (tối ưu tốc độ)
void forceSyncNTP() {
  Serial.println("[NTP] 🔄 Force sync NTP...");
  
  for (int attempt = 1; attempt <= 1; attempt++) { // Chỉ thử 1 lần để setup nhanh
    Serial.printf("[NTP] Thử sync lần %d/1...\n", attempt);
    
    // Reset flag để force sync
    timeSynced = false;
    syncNTP();
    
    if (timeSynced) {
      Serial.println("[NTP] ✅ Sync thành công!");
      return;
    }
    
    Serial.printf("[NTP] Thất bại lần %d, bỏ qua để setup nhanh\n", attempt);
  }
  
  Serial.println("[NTP] ❌ Không thể sync sau 1 lần thử");
  Serial.println("[NTP] Sẽ sử dụng fallback timestamp từ millis()");
}

// WiFi Setup Functions
void handleWiFiScan() {
  Serial.println("🔍 Quét WiFi xung quanh...");
  
  int n = WiFi.scanNetworks();
  String html = "<!DOCTYPE html><html><head><meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>WiFi Networks</title>";
  html += "<style>body{font-family:Arial;padding:20px;background:#f5f5f5}";
  html += ".container{max-width:600px;margin:0 auto;background:white;padding:30px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1)}";
  html += ".network{border:1px solid #ddd;border-radius:5px;padding:15px;margin:10px 0}";
  html += ".network strong{color:#2196f3}";
  html += ".signal{color:#4caf50}";
  html += ".button{background:#2196f3;color:white;padding:8px 16px;border:none;border-radius:3px;cursor:pointer;margin:5px}";
  html += ".button:hover{background:#1976d2}</style></head><body>";
  
  html += "<div class='container'>";
  html += "<h1>🔍 WiFi Networks Found</h1>";
  html += "<p>Found " + String(n) + " networks:</p>";
  
  for (int i = 0; i < n; i++) {
    html += "<div class='network'>";
    html += "<strong>" + WiFi.SSID(i) + "</strong><br>";
    html += "<span class='signal'>Signal: " + String(WiFi.RSSI(i)) + " dBm</span><br>";
    html += "<span>Security: ";
    
    switch (WiFi.encryptionType(i)) {
      case WIFI_AUTH_OPEN:
        html += "Open";
        break;
      case WIFI_AUTH_WEP:
        html += "WEP";
        break;
      case WIFI_AUTH_WPA_PSK:
        html += "WPA-PSK";
        break;
      case WIFI_AUTH_WPA2_PSK:
        html += "WPA2-PSK";
        break;
      case WIFI_AUTH_WPA_WPA2_PSK:
        html += "WPA/WPA2-PSK";
        break;
      default:
        html += "Unknown";
        break;
    }
    
    html += "</span><br>";
    html += "<form method='POST' action='/wifi-connect' style='display:inline'>";
    html += "<input type='hidden' name='ssid' value='" + WiFi.SSID(i) + "'>";
    html += "<input type='password' name='password' placeholder='WiFi Password' required>";
    html += "<button type='submit' class='button'>Connect</button>";
    html += "</form>";
    html += "</div>";
  }
  
  html += "<a href='/' class='button'>← Back to Status</a>";
  html += "</div></body></html>";
  
  server.send(200, "text/html", html);
}

void handleWiFiConnect() {
  if (server.hasArg("ssid") && server.hasArg("password")) {
    String ssid = server.arg("ssid");
    String password = server.arg("password");
    
    Serial.println("🔗 Đang kết nối WiFi: " + ssid);
    
    // Thử kết nối WiFi mới
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
      delay(500);
      Serial.print(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("✅ WiFi đã kết nối!");
      Serial.print("IP: "); Serial.println(WiFi.localIP());
      
      // Lưu cấu hình WiFi vào SPIFFS
      saveWiFiConfig(ssid, password);
      
      String html = "<!DOCTYPE html><html><head><meta charset='utf-8'>";
      html += "<title>Connected</title></head><body>";
      html += "<h2>✅ WiFi Connected!</h2>";
      html += "<p>SSID: " + ssid + "</p>";
      html += "<p>IP: " + WiFi.localIP().toString() + "</p>";
      html += "<p>ESP32 sẽ khởi động lại...</p>";
      html += "<script>setTimeout(() => window.location.href='/', 3000);</script>";
      html += "</body></html>";
      
      server.send(200, "text/html", html);
      
      delay(2000);
      ESP.restart();
    } else {
      Serial.println("❌ WiFi kết nối thất bại");
      
      String html = "<!DOCTYPE html><html><head><meta charset='utf-8'>";
      html += "<title>Connection Failed</title></head><body>";
      html += "<h2>❌ WiFi Connection Failed</h2>";
      html += "<p>SSID: " + ssid + "</p>";
      html += "<p>Please check password and try again.</p>";
      html += "<a href='/wifi-scan'>← Back to WiFi Scan</a>";
      html += "</body></html>";
      
      server.send(200, "text/html", html);
    }
  } else {
    server.send(400, "text/plain", "Missing SSID or password");
  }
}

void handleWiFiReset() {
  Serial.println("🔄 Reset WiFi config...");
  
  // Xóa cấu hình WiFi đã lưu
  SPIFFS.remove("/wifi_config.json");
  
  String html = "<!DOCTYPE html><html><head><meta charset='utf-8'>";
  html += "<title>Reset Complete</title></head><body>";
  html += "<h2>WiFi Config Reset</h2>";
  html += "<p>ESP32 sẽ khởi động lại...</p>";
  html += "<script>setTimeout(() => window.location.href='/', 3000);</script>";
  html += "</body></html>";
  
  server.send(200, "text/html", html);
  
  delay(2000);
  ESP.restart();
}

void saveWiFiConfig(String ssid, String password) {
  // Khởi tạo SPIFFS nếu chưa có
  if (!SPIFFS.begin(true)) {
    Serial.println("❌ Lỗi khởi tạo SPIFFS");
    return;
  }
  
  File file = SPIFFS.open("/wifi_config.json", "w");
  if (file) {
    JsonDocument doc;
    doc["ssid"] = ssid;
    doc["password"] = password;
    doc["saved"] = true;
    
    serializeJson(doc, file);
    file.close();
    Serial.println("✅ Đã lưu cấu hình WiFi: " + ssid);
  } else {
    Serial.println("❌ Không thể lưu cấu hình WiFi");
  }
}

void handleFirmwareUpdateWiFi() {
  Serial.println("🔄 Bắt đầu cập nhật firmware qua WiFi...");
  
  if (!firmwareUpdateAvailable) {
    server.send(400, "application/json", "{\"error\":\"No firmware update available\"}");
    return;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    server.send(400, "application/json", "{\"error\":\"WiFi not connected\"}");
    return;
  }
  
  // Tạo URL đầy đủ cho firmware
  String fullUrl = String("http://") + String(BACKEND_HOST) + ":" + String(BACKEND_PORT) + latestFirmwareUrl;
  
  Serial.println("📥 Tải firmware từ: " + fullUrl);
  
  // Thực hiện OTA update
  bool success = performOTAUpdate(fullUrl, "WiFi");
  
  if (success) {
    server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Firmware update completed\"}");
  } else {
    server.send(500, "application/json", "{\"error\":\"Firmware update failed\"}");
  }
}

void handleFirmwareUpdate4G() {
  Serial.println("🔄 Bắt đầu cập nhật firmware qua 4G...");
  
  if (!firmwareUpdateAvailable) {
    server.send(400, "application/json", "{\"error\":\"No firmware update available\"}");
    return;
  }
  
  #if ENABLE_CELLULAR_UPLOAD
  if (!cellularBegin()) {
    server.send(400, "application/json", "{\"error\":\"4G not connected\"}");
    return;
  }
  
  // Tạo URL đầy đủ cho firmware
  String fullUrl = String("http://") + String(BACKEND_HOST) + ":" + String(BACKEND_PORT) + latestFirmwareUrl;
  
  Serial.println("📥 Tải firmware từ: " + fullUrl);
  
  // Thực hiện OTA update qua 4G
  bool success = performOTAUpdate(fullUrl, "4G");
  
  if (success) {
    server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Firmware update completed\"}");
  } else {
    server.send(500, "application/json", "{\"error\":\"Firmware update failed\"}");
  }
  #else
  server.send(400, "application/json", "{\"error\":\"4G not enabled\"}");
  #endif
}

// Hàm kiểm tra firmware update từ web interface
void handleFirmwareCheck() {
  Serial.println("🔍 Kiểm tra firmware update từ web interface...");
  
  // Force check firmware update
  checkFirmwareUpdate();
  
  // Trả về kết quả kiểm tra
  String json;
  {
    JsonDocument doc;
    doc["current_version"] = FIRMWARE_VERSION;
    doc["current_build"] = FIRMWARE_BUILD;
    doc["update_available"] = firmwareUpdateAvailable;
    
    if (firmwareUpdateAvailable) {
      doc["latest_version"] = latestFirmwareDisplayVersion;
      doc["latest_url"] = latestFirmwareUrl;
    } else {
      doc["latest_version"] = FIRMWARE_VERSION;
      doc["latest_url"] = "";
    }
    
    serializeJson(doc, json);
  }
  
  server.send(200, "application/json", json);
}