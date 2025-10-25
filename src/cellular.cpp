#include "cellular.h"
#include <esp_task_wdt.h>
#include <Update.h>

#define TINY_GSM_DEBUG Serial

TinyGsm modem(CELL_UART);
TinyGsmClient gsmClient(modem);

static bool isModemReady = false;
static bool isDataConnected = false;  // Track if data connection is active
static SemaphoreHandle_t cellularHttpMutex = NULL; // serialize 4G HTTP

// Hàm reset modem hoàn toàn khi gặp lỗi
void cellularReset() {
  Serial.println("[CELL] Reset modem hoàn toàn...");
  isModemReady = false;
  isDataConnected = false;
  
  // Tắt modem
  pinMode(CELL_PWRKEY_PIN, OUTPUT);
  #if CELL_PWRKEY_ACTIVE_LOW
    digitalWrite(CELL_PWRKEY_PIN, HIGH);
  #else
    digitalWrite(CELL_PWRKEY_PIN, LOW);
  #endif
  
  delay(2000);
  
  // Bật lại modem
  cellularPowerOn();
  delay(3000);
  
  Serial.println("[CELL] Modem đã được reset");
}

static void logCEER() {
  modem.sendAT("+CEER");
  unsigned long t0 = millis();
  String line;
  while (millis() - t0 < 1200) {
    while (modem.stream.available()) {
      char c = modem.stream.read();
      if (c == '\r') continue;
      if (c == '\n') {
        if (line.length()) {
          Serial.print("[CELL][AT][CEER] ");
          Serial.println(line);
          line = "";
        }
      } else {
        line += c;
      }
    }
    delay(10);
  }
}

bool cellularPowerOn() {
  pinMode(CELL_PWRKEY_PIN, OUTPUT);
  #if CELL_PWRKEY_ACTIVE_LOW
    digitalWrite(CELL_PWRKEY_PIN, HIGH);
  #else
    digitalWrite(CELL_PWRKEY_PIN, LOW);
  #endif
  delay(100);
  #if CELL_PWRKEY_ACTIVE_LOW
    digitalWrite(CELL_PWRKEY_PIN, LOW);
    delay(CELL_PWRKEY_ACTIVE_MS);
    digitalWrite(CELL_PWRKEY_PIN, HIGH);
  #else
    digitalWrite(CELL_PWRKEY_PIN, HIGH);
    delay(CELL_PWRKEY_ACTIVE_MS);
    digitalWrite(CELL_PWRKEY_PIN, LOW);
  #endif
  delay(1500);
  return true;
}

//=================================
// CLEAN LOGIC: cellularBegin()
// Initialize modem + establish data connection
// Return TRUE = ready to send HTTP
// Return FALSE = cannot connect, try again later
//=================================
bool cellularBegin() {
  if (cellularHttpMutex == NULL) {
    cellularHttpMutex = xSemaphoreCreateMutex();
  }
  // Always try to maintain connection - don't do aggressive re-init
  if (isModemReady && isDataConnected) {
    Serial.println("[CELL] ✅ Đã kết nối - reuse connection");
    return true;
  }

  unsigned long startTime = millis();
  const unsigned long timeout = 15000;
  
  // ===== PHASE 1: Initialize Modem (only if needed) =====
  if (!isModemReady) {
    Serial.println("[CELL] Khởi tạo modem...");
    cellularPowerOn();
    
    // Tăng bộ đệm RX để ổn định ở baud cao
    CELL_UART.setRxBufferSize(2048);
    CELL_UART.begin(CELL_BAUD, SERIAL_8N1, CELL_RX_PIN, CELL_TX_PIN);
    Serial.printf("[CELL] UART2 init: RX=%d, TX=%d, BAUD=%d\n", CELL_RX_PIN, CELL_TX_PIN, CELL_BAUD);
    delay(300);
    
    bool atOk = modem.testAT();
    if (!atOk) {
      Serial.println("[CELL] ⚠️ Không nhận được phản hồi AT. Thử các cấu hình chân khác...");
      const int cand[][2] = {{CELL_TX_PIN, CELL_RX_PIN}, {27, 26}, {26, 27}};
      unsigned long scanStart = millis();
      for (size_t i = 0; i < 3; i++) {
        if (millis() - scanStart > 8000) break;
        int rx = cand[i][0], tx = cand[i][1];
        if (rx == CELL_RX_PIN && tx == CELL_TX_PIN) continue;
        CELL_UART.end();
        delay(50);
        CELL_UART.begin(CELL_BAUD, SERIAL_8N1, rx, tx);
        Serial.printf("[CELL] Thử UART2 fallback: RX=%d, TX=%d...\n", rx, tx);
        delay(200);
        if (modem.testAT()) {
          Serial.printf("[CELL] ✅ Modem phản hồi với RX=%d, TX=%d\n", rx, tx);
          atOk = true;
          break;
        }
      }
      if (!atOk) {
        Serial.println("[CELL] ❌ Không tìm được UART hoạt động");
        return false;
      }
    }
    
    // Restart modem
    bool modemReady = false;
    for (int retry = 0; retry < 2; retry++) {
      Serial.printf("[CELL] Thử khởi động modem lần %d...\n", retry + 1);
      if (modem.restart() || modem.init()) {
        modemReady = true;
        Serial.println("[CELL] Modem restart/init thành công");
        delay(2000);
        
        // Clear buffer
        while (modem.stream.available()) {
          modem.stream.read();
          delayMicroseconds(100);
        }
        break;
      }
      delay(1000);
    }
    
    if (!modemReady) {
      Serial.println("[CELL] ❌ Modem không khởi động được");
      logCEER();
      return false;
    }
    
    String name = modem.getModemName();
    String info = modem.getModemInfo();
    Serial.println("[CELL] Modem: " + name + " | " + info);
    
    #if CELL_FORCE_LTE_ONLY
      modem.sendAT("+CNMP=38");  modem.waitResponse(2000);
      modem.sendAT("+CMNB=1");   modem.waitResponse(2000);
    #endif
    
    modem.sendAT("+CPSI?"); modem.waitResponse(2000);
    isModemReady = true;
    
    // Check SIM
    Serial.println("[CELL] Kiểm tra SIM card...");
    modem.sendAT("+CPIN?");
    int cpinCheck = modem.waitResponse(1000);
    
    if (cpinCheck != 1) {
      Serial.println("[CELL] ❌ SIM card NOT detected");
      return false;
    }
    Serial.println("[CELL] ✅ SIM card detected");
  }
  
  // ===== PHASE 2: Establish Data Connection =====
  Serial.print("[CELL] Kết nối dữ liệu với APN: ");
  Serial.println(CELL_APN);
  
  // GPRS Connect with retries
  bool gprsOk = false;
  for (int gprsRetry = 0; gprsRetry < 3; gprsRetry++) {
    if (modem.gprsConnect(CELL_APN, CELL_USER, CELL_PASS)) {
      gprsOk = true;
      break;
    }
    Serial.printf("[CELL] ❌ GPRS connect failed (attempt %d/3)\n", gprsRetry + 1);
    if (gprsRetry < 2) {
      delay(2000);  // Wait before retry
    }
  }
  
  if (!gprsOk) {
    Serial.println("[CELL] ❌ GPRS connect failed after 3 attempts");
    isDataConnected = false;
    return false;
  }
  Serial.println("[CELL] ✅ GPRS connected");
  
  // Network Open (activate data context)
  modem.sendAT("+NETCLOSE");  modem.waitResponse(1000);
  
  for (int i = 0; i < 2; i++) {
    Serial.printf("[CELL] Thử NETOPEN lần %d...\n", i + 1);
    modem.sendAT("+NETOPEN");
    if (modem.waitResponse(5000) == 1) {
      Serial.println("[CELL] ✅ NETOPEN thành công");
      break;
    }
    Serial.println("[CELL] NETOPEN retry...");
    delay(1000);
    
    if (i == 1) {
      Serial.println("[CELL] ❌ NETOPEN failed after 2 attempts");
      isDataConnected = false;
      return false;
    }
  }
  
  // DNS config
  modem.sendAT("+CDNSCFG=\"8.8.8.8\",\"1.1.1.1\"");
  modem.waitResponse(1000);
  esp_task_wdt_reset(); // Reset after DNS config
  
  // Check signal strength
  Serial.print("[CELL] 📶 Kiểm tra signal: ");
  modem.sendAT("+CSQ");
  modem.waitResponse(1000);
  esp_task_wdt_reset(); // Reset after signal check
  
  // Pre-resolve backend host DNS ONE TIME để cache (không resolve lại mỗi request)
  Serial.println("[CELL] 🌐 Resolve backend host...");
  modem.sendAT("+CDNSGIP=\"" BACKEND_HOST "\"");
  modem.waitResponse(5000);
  esp_task_wdt_reset(); // Reset after DNS resolution
  
  // ⏳ Chờ thêm cho HTTP client ổn định
  Serial.println("[CELL] ⏳ HTTP client initializing...");
  for (int i = 0; i < 2; i++) {  // 2 x 500ms = 1 giây (giảm từ 2s)
    delay(500);
    esp_task_wdt_reset(); // Reset watchdog during stabilization delays
  }
  
  isDataConnected = true;
  Serial.println("[CELL] ✅ Kết nối 4G hoàn tất!");
  return true;
}

//=================================
// CLEAN LOGIC: cellularHttpPost()
// ONLY do HTTP - don't reconnect
// If connection lost, return false
//=================================
bool cellularHttpPost(const char* host, uint16_t port, const char* path, const String& body, String& response) {
  if (cellularHttpMutex) xSemaphoreTake(cellularHttpMutex, portMAX_DELAY);
  // Đảm bảo client ở trạng thái sạch trước mỗi request
  gsmClient.stop();
  Serial.print("[CELL] HTTP POST to ");
  Serial.print(host);
  Serial.print(":");
  Serial.println(port);
  esp_task_wdt_reset();
  
  // ✅ DNS đã cached trong cellularBegin() - không cần resolve lại
  // HTTP request với timeout
  HttpClient http(gsmClient, host, port);
  http.setTimeout(15000);  // giảm timeout để tránh treo lâu
  esp_task_wdt_reset();

  for (int attempt = 1; attempt <= 2; attempt++) {
    http.beginRequest();
    http.post(path);
    http.sendHeader("Content-Type", "application/json");
    http.sendHeader("X-API-Key", APPLICATION_KEY);
    http.sendHeader("Content-Length", body.length());
    http.sendHeader("Connection", "close"); // tránh keep-alive gây treo
    http.beginBody();
    esp_task_wdt_reset();

    http.print(body);
    http.endRequest();
    esp_task_wdt_reset();  // Reset sau endRequest (có thể chậm)

    // 🐕 Chờ response - có thể mất thời gian, reset watchdog thường xuyên
    delay(100);
    esp_task_wdt_reset();

    int statusCode = http.responseStatusCode();
    esp_task_wdt_reset();  // Reset sau responseStatusCode
    if (statusCode >= 200 && statusCode < 300) {
      response = http.responseBody();
      esp_task_wdt_reset();  // Reset sau responseBody
      delay(100);
      esp_task_wdt_reset();  // Reset trước khi stop
      http.stop();
      Serial.print("[CELL] HTTP ");
      Serial.println(statusCode);
      esp_task_wdt_reset();
      if (cellularHttpMutex) xSemaphoreGive(cellularHttpMutex);
      return true;
    }

    // Log và xử lý lỗi
    Serial.print("[CELL] HTTP ");
    Serial.println(statusCode);
    http.stop();

    // Với lỗi transport (-1/-2/-3) hoặc 400, đánh dấu mất kết nối dữ liệu và retry nhẹ
    if (attempt < 2 && (statusCode == -3 || statusCode == -2 || statusCode == -1 || statusCode == 400)) {
      isDataConnected = false;
      modem.sendAT("+NETCLOSE");
      modem.waitResponse(1000);
      Serial.println("[CELL] POST retry after backoff...");
      for (int i = 0; i < 5; i++) {
        delay(100);
        esp_task_wdt_reset();
      }
      continue;
    }
    if (cellularHttpMutex) xSemaphoreGive(cellularHttpMutex);
    return false;
  }
  if (cellularHttpMutex) xSemaphoreGive(cellularHttpMutex);
  return false;
}

String cellularStatusSummary() {
  String s;
  s += String("Modem: ") + modem.getModemName();
  s += String(" | IMEI: ") + modem.getIMEI();
  // Signal
  int16_t csq = modem.getSignalQuality();
  s += String(" | CSQ: ") + String(csq);
  // IP
  s += String(" | IP: ");
  modem.sendAT("+CGPADDR");
  modem.waitResponse(1000);
  // Leave generic; TinyGSM lacks direct PDP IP getter for all modules
  return s;
}

//=================================
// cellularHttpGet()
// For GET requests (firmware check, etc)
//=================================
bool cellularHttpGet(const char* host, uint16_t port, const char* path, String& response) {
  if (cellularHttpMutex) xSemaphoreTake(cellularHttpMutex, portMAX_DELAY);
  // Đảm bảo client ở trạng thái sạch trước mỗi request
  gsmClient.stop();
  Serial.print("[CELL] HTTP GET to ");
  Serial.print(host);
  Serial.print(":");
  Serial.println(port);
  esp_task_wdt_reset();
  
  // ✅ DNS đã cached trong cellularBegin() - không cần resolve lại
  // HTTP GET request với timeout
  HttpClient http(gsmClient, host, port);
  http.setTimeout(15000);  // giảm timeout để tránh treo lâu
  esp_task_wdt_reset();
  
  // Minimal retry loop to handle transient -3 (connection) errors
  for (int attempt = 1; attempt <= 2; attempt++) {
    http.beginRequest();
    http.get(path);
    http.sendHeader("X-API-Key", APPLICATION_KEY);
    http.sendHeader("Connection", "close");        // avoid keep-alive quirks
    http.sendHeader("Accept", "application/json");  // hint response type
    http.endRequest();
    esp_task_wdt_reset();  // Reset sau endRequest (có thể chậm)
    
    // 🐕 Chờ response - có thể mất thời gian, reset watchdog thường xuyên
    delay(100);
    esp_task_wdt_reset();
    
    int statusCode = http.responseStatusCode();
    esp_task_wdt_reset();  // Reset sau responseStatusCode
    if (statusCode >= 200 && statusCode < 300) {
      response = http.responseBody();
      esp_task_wdt_reset();  // Reset sau responseBody
      delay(100);
      esp_task_wdt_reset();  // Reset trước khi stop
      http.stop();
      Serial.print("[CELL] HTTP GET ");
      Serial.println(statusCode);
      esp_task_wdt_reset();
      if (cellularHttpMutex) xSemaphoreGive(cellularHttpMutex);
      return true;
    }
    
    // Log and retry once for transient errors
    Serial.print("[CELL] HTTP GET ");
    Serial.println(statusCode);
    http.stop();
    if (statusCode < 0) {
      // lỗi transport: đánh dấu mất data context để lần sau tái thiết lập
      isDataConnected = false;
      modem.sendAT("+NETCLOSE");
      modem.waitResponse(1000);
    }
    if (attempt < 2 && (statusCode == -3 || statusCode == 400)) {
      Serial.println("[CELL] GET retry after backoff...");
      for (int i = 0; i < 5; i++) { // 500ms backoff
        delay(100);
        esp_task_wdt_reset();
      }
      continue;
    }
    if (cellularHttpMutex) xSemaphoreGive(cellularHttpMutex);
    return false;
  }
  if (cellularHttpMutex) xSemaphoreGive(cellularHttpMutex);
  return false;
}

//=================================
// cellularOtaDownload()
// Download firmware via 4G and write to flash using Update
//=================================
bool cellularOtaDownload(const char* host, uint16_t port, const char* path) {
  if (cellularHttpMutex) xSemaphoreTake(cellularHttpMutex, portMAX_DELAY);
  gsmClient.stop();
  esp_task_wdt_reset();

  HttpClient http(gsmClient, host, port);
  http.setTimeout(30000);

  http.beginRequest();
  http.get(path);
  http.sendHeader("X-API-Key", APPLICATION_KEY);
  http.sendHeader("Connection", "close");
  http.endRequest();

  int status = http.responseStatusCode();
  if (status != 200) {
    Serial.print("[CELL][OTA] HTTP "); Serial.println(status);
    http.stop();
    if (cellularHttpMutex) xSemaphoreGive(cellularHttpMutex);
    return false;
  }

  int contentLength = http.contentLength();
  if (contentLength <= 0) {
    Serial.println("[CELL][OTA] Unknown content length (chunked?) - proceed streaming");
  }

  if (!Update.begin(contentLength > 0 ? contentLength : UPDATE_SIZE_UNKNOWN)) {
    Serial.println("[CELL][OTA] Update.begin failed");
    http.stop();
    if (cellularHttpMutex) xSemaphoreGive(cellularHttpMutex);
    return false;
  }

  const size_t bufSize = 512;
  uint8_t buf[bufSize];
  int written = 0;
  unsigned long lastProgress = millis();
  while (contentLength <= 0 || written < contentLength) {
    int avail = http.available();
    if (avail > 0) {
      int toRead = avail > (int)bufSize ? (int)bufSize : avail;
      int r = http.readBytes(buf, toRead);
      if (r <= 0) break;
      size_t w = Update.write(buf, r);
      if ((int)w != r) {
        Serial.println("[CELL][OTA] Update.write failed");
        Update.abort();
        http.stop();
        if (cellularHttpMutex) xSemaphoreGive(cellularHttpMutex);
        return false;
      }
      written += r;
      unsigned long now = millis();
      if (now - lastProgress > 1000) {
        Serial.printf("[CELL][OTA] Written %d bytes\n", written);
        lastProgress = now;
      }
      if ((written % 4096) == 0) esp_task_wdt_reset();
    } else {
      delay(10);
      esp_task_wdt_reset();
      // nếu server đóng kết nối và không còn dữ liệu khi không biết contentLength → thoát
      if (!http.connected() && http.available() <= 0 && contentLength <= 0) {
        break;
      }
    }
  }

  http.stop();

  if (!Update.end()) {
    Serial.println("[CELL][OTA] Update.end failed");
    if (cellularHttpMutex) xSemaphoreGive(cellularHttpMutex);
    return false;
  }

  Serial.println("[CELL][OTA] Update success, rebooting...");
  if (cellularHttpMutex) xSemaphoreGive(cellularHttpMutex);
  delay(500);
  ESP.restart();
  return true;
}

// Triển khai HTTPS qua AT command (SIMCOM A7680C/SIM7600)
bool cellularHttpPostAT(const char* host, uint16_t port, const char* path, const String& body, String& response) {
  // Đảm bảo stack ở trạng thái sạch và START với retry
  modem.sendAT("+CHTTPSSTOP"); modem.waitResponse(2000);
  esp_task_wdt_reset(); // Reset watchdog during HTTPS setup
  
  // Cấu hình HTTPS trước khi START
  modem.sendAT("+CHTTPSCFG=\"sslversion\",3"); modem.waitResponse(2000); // auto
  modem.sendAT("+CHTTPSCFG=\"sni\",1"); modem.waitResponse(2000);       // bật SNI
  modem.sendAT("+CHTTPSCFG=\"timeout\",60"); modem.waitResponse(2000);   // timeout 60s
  modem.sendAT("+CHTTPSCFG=\"keepidle\",60"); modem.waitResponse(2000);
  esp_task_wdt_reset(); // Reset watchdog after HTTPS config
  {
    bool started = false;
    for (int i = 0; i < 3 && !started; i++) {
      modem.sendAT("+CHTTPSSTART");
      int r = modem.waitResponse(8000);
      if (r == 1) { started = true; break; }
      Serial.println("[CELL][AT] CHTTPSSTART retry...");
      logCEER();
      esp_task_wdt_reset(); // Reset watchdog during retries
      delay(2000);
      modem.sendAT("+CHTTPSSTOP"); modem.waitResponse(2000);
    }
    if (!started) { Serial.println("[CELL][AT] CHTTPSSTART fail"); return false; }
  }
  esp_task_wdt_reset(); // Reset watchdog before session open
  // Open session với SNI mode (tham số 3 = 2)
  modem.sendAT("+CHTTPSOPSE=\"", host, "\",", port, ",2");
  if (modem.waitResponse(12000) != 1) { Serial.println("[CELL][AT] CHTTPSOPSE fail"); goto CLOSE_STOP; }
  esp_task_wdt_reset(); // Reset watchdog after session open
  // Header
  modem.sendAT("+CHTTPSHEADER=1"); if (modem.waitResponse(2000) != 1) { Serial.println("[CELL][AT] HEADER on fail"); goto CLOSE_STOP; }
  modem.sendAT("+CHTTPSHEADER=" "\"Content-Type: application/json\""); if (modem.waitResponse(2000) != 1) { Serial.println("[CELL][AT] HEADER CT fail"); goto CLOSE_STOP; }
  // Thêm Host header để chắc SNI/Proxy nhận đúng
  modem.sendAT("+CHTTPSHEADER=" "\"Host: ", host, "\""); modem.waitResponse(2000);
  esp_task_wdt_reset(); // Reset watchdog before POST
  // Gửi POST
  modem.sendAT("+CHTTPSPOST=\"", path, "\",,", body.length());
  if (modem.waitResponse(15000, "+CHTTPSPOST: ") != 1) { Serial.println("[CELL][AT] POST start fail"); goto CLOSE_STOP; }
  modem.streamWrite(body.c_str());
  if (modem.waitResponse(20000) != 1) { Serial.println("[CELL][AT] POST resp fail"); goto CLOSE_STOP; }
  esp_task_wdt_reset(); // Reset watchdog before response read
  // Đọc trả lời
  modem.sendAT("+CHTTPSRECV=512");
  if (modem.waitResponse(12000, "+CHTTPSRECV: ") == 1) {
    // Đọc phần thân (đơn giản: đọc mọi thứ còn lại trong serial buffer)
    while (modem.stream.available()) {
      response += (char)modem.stream.read();
    }
  }
  modem.sendAT("+CHTTPSCLSE"); modem.waitResponse(2000);
  modem.sendAT("+CHTTPSSTOP"); modem.waitResponse(2000);
  Serial.println("[CELL][AT] HTTPS POST done");
  return true; // đơn giản: coi như thành công; có thể parse thêm mã trạng thái nếu cần

CLOSE_STOP:
  modem.sendAT("+CHTTPSCLSE"); modem.waitResponse(2000);
  modem.sendAT("+CHTTPSSTOP"); modem.waitResponse(2000);
  return false;
}
