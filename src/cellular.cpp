#include "cellular.h"
#include <esp_task_wdt.h>
#include <Update.h>

/**
 * @file cellular.cpp
 * @brief Hiện thực toàn bộ nghiệp vụ điều khiển modem 4G (khởi tạo, HTTP, OTA, reset).
 *
 * Mọi bước đều có log chi tiết và chú thích tiếng Việt để dễ truy vết khi debug ngoài hiện trường.
 */

#define TINY_GSM_DEBUG Serial  // Bật log TinyGSM ra Serial để tiện theo dõi

TinyGsm modem(CELL_UART);
TinyGsmClient gsmClient(modem);

// --------------------------------------------------------------------
// Trạng thái nội bộ của mô-đun 4G
// --------------------------------------------------------------------
static bool isModemReady = false;         // Đã khởi tạo modem chưa?
static bool isDataConnected = false;      // Đã mở kết nối data (NETOPEN/PDP) chưa?
static SemaphoreHandle_t cellularHttpMutex = NULL; // Mutex để tuần tự hóa mọi HTTP qua 4G

/**
 * @brief Reset modem hoàn toàn (power cycle) khi gặp lỗi nặng.
 *
 * Các bước:
 * 1. Kéo PWRKEY về trạng thái tắt.
 * 2. Chờ 2 giây cho modem xuống.
 * 3. Gọi lại cellularPowerOn() để bật lại.
 */
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

/**
 * @brief Gửi lệnh +CEER để ghi lại nguyên nhân lỗi kết nối gần nhất.
 *
 * Hàm này chỉ dùng nội bộ cho debug, do đó giữ static.
 */
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

/**
 * @brief Thao tác bật modem bằng cách giữ PWRKEY ở mức active trong thời gian cấu hình.
 */
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

/**
 * @brief Chu trình khởi động modem và thiết lập kết nối dữ liệu.
 *
 * Trình tự chính:
 * - Khởi tạo mutex (nếu chưa có) để các task dùng chung.
 * - Nếu modem đã sẵn sàng và data context còn hoạt động thì tái sử dụng.
 * - Nếu chưa, thử khởi tạo UART, kiểm tra AT, SIM, ép LTE (nếu cấu hình).
 * - Kết nối GPRS/APN, mở NETOPEN và cấu hình DNS.
 *
 * @return true nếu mọi bước thành công; false nếu cần thử lại sau.
 */
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

/**
 * @brief Gửi HTTP POST tiêu chuẩn qua đường 4G.
 *
 * Hàm không tự khởi tạo lại modem; caller phải đảm bảo đã gọi cellularBegin().
 * - Luôn dựng lại HttpClient mới (connection close) để tránh giữ session hỏng.
 * - Cho phép thử tối đa 2 lần đối với lỗi transport (-1/-2/-3) hoặc 400 (bad request do mất context).
 * - Reset watchdog thường xuyên để tránh WDT khi mạng chậm.
 *
 * @return true nếu nhận mã phản hồi 2xx, false nếu tất cả attempt đều thất bại.
 */
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

/**
 * @brief Biến thể POST cho phép cấu hình timeout/số lần thử/backoff tùy tình huống.
 *
 * Phù hợp với các API tốn thời gian (ví dụ: gửi log lớn, ota metadata).
 */
bool cellularHttpPostWithOptions(const char* host, uint16_t port, const char* path,
                                 const String& body, String& response,
                                 uint16_t timeoutMs, int attempts, uint16_t backoffMs) {
  if (cellularHttpMutex) xSemaphoreTake(cellularHttpMutex, portMAX_DELAY);
  gsmClient.stop();
  Serial.print("[CELL] HTTP POST(opt) to "); Serial.print(host); Serial.print(":"); Serial.println(port);
  esp_task_wdt_reset();

  HttpClient http(gsmClient, host, port);
  http.setTimeout(timeoutMs);
  esp_task_wdt_reset();

  for (int attempt = 1; attempt <= (attempts < 1 ? 1 : attempts); attempt++) {
    http.beginRequest();
    http.post(path);
    http.sendHeader("Content-Type", "application/json");
    http.sendHeader("X-API-Key", APPLICATION_KEY);
    http.sendHeader("Content-Length", body.length());
    http.sendHeader("Connection", "close");
    http.beginBody();
    esp_task_wdt_reset();

    http.print(body);
    http.endRequest();
    esp_task_wdt_reset();

    delay(20);
    esp_task_wdt_reset();

    int statusCode = http.responseStatusCode();
    esp_task_wdt_reset();
    if (statusCode >= 200 && statusCode < 300) {
      response = http.responseBody();
      http.stop();
      if (cellularHttpMutex) xSemaphoreGive(cellularHttpMutex);
      Serial.print("[CELL] HTTP "); Serial.println(statusCode);
      return true;
    }

    Serial.print("[CELL] HTTP "); Serial.println(statusCode);
    http.stop();
    if (attempt < attempts && (statusCode == -3 || statusCode == -2 || statusCode == -1 || statusCode == 400)) {
      isDataConnected = false;
      modem.sendAT("+NETCLOSE");
      modem.waitResponse(500);
      if (backoffMs > 0) {
        unsigned long t0 = millis();
        while (millis() - t0 < backoffMs) {
          delay(50);
          esp_task_wdt_reset();
        }
      }
      continue;
    }
    if (cellularHttpMutex) xSemaphoreGive(cellularHttpMutex);
    return false;
  }
  if (cellularHttpMutex) xSemaphoreGive(cellularHttpMutex);
  return false;
}

/**
 * @brief Gửi POST dạng "bắn nhanh" với timeout 2 giây, không retry.
 *
 * Dùng cho cảnh báo khẩn để tránh block task chính quá lâu.
 */
bool cellularHttpPostCritical(const char* host, uint16_t port, const char* path, const String& body, String& response) {
  // 2s timeout, 1 attempt, 0 backoff
  return cellularHttpPostWithOptions(host, port, path, body, response, 2000, 1, 0);
}

/**
 * @brief Gom thông tin cơ bản của modem để hiển thị lên giao diện web.
 *
 * Bao gồm: tên modem, IMEI, chất lượng sóng CSQ và IP hiện tại (qua AT).
 */
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

/**
 * @brief Gửi HTTP GET qua 4G để lấy dữ liệu (ví dụ kiểm tra firmware).
 *
 * Có cơ chế retry nhẹ tương tự POST mặc định để chống lỗi mạng tức thời.
 */
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

/**
 * @brief Tải firmware .bin qua 4G và ghi trực tiếp vào flash thông qua lớp Update.
 *
 * Chú ý: hàm sẽ tự khởi động lại thiết bị nếu Update.end() thành công.
 */
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

/**
 * @brief Thực thi HTTPS POST thông qua bộ lệnh AT +CHTTPS* (không dùng TinyGSM stack).
 *
 * Dành cho trường hợp cần TLS nhưng modem chưa hỗ trợ qua TinyGSM. Hàm sẽ cố gắng
 * khởi tạo phiên, gửi header, truyền payload và đọc phản hồi tối đa 512 byte.
 */
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
