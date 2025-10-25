#include "cellular.h"
#include <esp_task_wdt.h>

#define TINY_GSM_DEBUG Serial

TinyGsm modem(CELL_UART);
TinyGsmClient gsmClient(modem);

static bool isModemReady = false;

// Hàm reset modem hoàn toàn khi gặp lỗi
void cellularReset() {
  Serial.println("[CELL] Reset modem hoàn toàn...");
  isModemReady = false;
  
  // Tắt modem
  pinMode(CELL_PWRKEY_PIN, OUTPUT);
  #if CELL_PWRKEY_ACTIVE_LOW
    digitalWrite(CELL_PWRKEY_PIN, HIGH);
  #else
    digitalWrite(CELL_PWRKEY_PIN, LOW);
  #endif
  
  delay(2000); // Giảm từ 5000ms xuống 2000ms
  
  // Bật lại modem
  cellularPowerOn();
  delay(3000);
  
  Serial.println("[CELL] Modem đã được reset");
}

static void logCEER() {
  // In lỗi gần nhất từ modem
  modem.sendAT("+CEER");
  // chờ và đọc về tối đa ~1s
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
  // Trạng thái nhả nút
  #if CELL_PWRKEY_ACTIVE_LOW
    digitalWrite(CELL_PWRKEY_PIN, HIGH);
  #else
    digitalWrite(CELL_PWRKEY_PIN, LOW);
  #endif
  delay(100);
  // Nhấn PWRKEY theo mức active
  #if CELL_PWRKEY_ACTIVE_LOW
    digitalWrite(CELL_PWRKEY_PIN, LOW);
    delay(CELL_PWRKEY_ACTIVE_MS);
    digitalWrite(CELL_PWRKEY_PIN, HIGH);
  #else
    digitalWrite(CELL_PWRKEY_PIN, HIGH);
    delay(CELL_PWRKEY_ACTIVE_MS);
    digitalWrite(CELL_PWRKEY_PIN, LOW);
  #endif
  delay(1500); // Giảm từ 3000ms xuống 1500ms
  return true;
}

bool cellularBegin() {
  // Giảm timeout để setup nhanh hơn
  unsigned long startTime = millis();
  const unsigned long timeout = 15000; // Giảm từ 20s xuống 15s
  
  if (!isModemReady) {
    Serial.println("[CELL] Khởi tạo modem...");
    cellularPowerOn();
    // Cảnh báo nếu bo có PSRAM và đang dùng GPIO16/17
    #if defined(ESP32)
    if ((CELL_RX_PIN == 16 || CELL_TX_PIN == 17)) {
      if (psramFound() || ESP.getPsramSize() > 0) {
        Serial.println("[CELL] ⚠️ PSRAM detected. GPIO16/17 thường dành cho PSRAM → đổi sang chân khác (ví dụ RX=27, TX=26)");
      }
    }
    #endif
    CELL_UART.begin(CELL_BAUD, SERIAL_8N1, CELL_RX_PIN, CELL_TX_PIN);
    Serial.printf("[CELL] UART2 init: RX=%d, TX=%d, BAUD=%d\n", CELL_RX_PIN, CELL_TX_PIN, CELL_BAUD);
    delay(300); // Giảm từ 500ms xuống 300ms
    // Thử AT nhanh để phát hiện lỗi wiring/nguồn sớm
    bool atOk = modem.testAT();
    if (!atOk) {
      Serial.println("[CELL] ⚠️ Không nhận được phản hồi AT sau khi mở UART. Thử các cấu hình chân UART thay thế...");
      // Thử các cặp chân khác và đảo RX/TX
      const int cand[][2] = {
        {CELL_TX_PIN, CELL_RX_PIN}, // đảo ngược cấu hình hiện tại
        {27, 26},                   // RX=27, TX=26
        {26, 27},                   // RX=26, TX=27 (nếu hàn nhầm)
        {33, 25},                   // RX=33, TX=25
        {25, 33}                    // RX=25, TX=33
      };
      unsigned long scanStart = millis();
      for (size_t i = 0; i < sizeof(cand)/sizeof(cand[0]); i++) {
        // Giới hạn tổng thời gian quét chân để tránh WDT
        if (millis() - scanStart > 8000) { // 8s
          Serial.println("[CELL] Dừng quét UART fallback do quá thời gian");
          break;
        }
        int rx = cand[i][0];
        int tx = cand[i][1];
        if (rx == CELL_RX_PIN && tx == CELL_TX_PIN) continue; // đã thử
        CELL_UART.end();
        delay(50);
        CELL_UART.begin(CELL_BAUD, SERIAL_8N1, rx, tx);
        Serial.printf("[CELL] Thử UART2 fallback: RX=%d, TX=%d...\n", rx, tx);
        esp_task_wdt_reset();
        delay(200);
        esp_task_wdt_reset();
        if (modem.testAT()) {
          Serial.printf("[CELL] ✅ Modem phản hồi AT với RX=%d, TX=%d\n", rx, tx);
          atOk = true;
          break;
        }
      }
      if (!atOk) {
        Serial.println("[CELL] ❌ Không tìm được cặp chân UART hoạt động. Kiểm tra lại dây (UTX→ESP RX, URX→ESP TX), GND chung, nguồn và mức điện áp IO của module (1.8/2.8/3.3V).");
        return false;
      }
    }
    
    // Reset watchdog during long operations
    esp_task_wdt_reset();
    
    // Đừng timeout trước khi thử restart/init modem; chỉ cảnh báo nếu chậm
    if (millis() - startTime > timeout) {
      Serial.println("[CELL] ⚠️ Power-on taking longer than expected, tiếp tục thử...");
    }
    
    // Thử restart với retry logic (giảm số lần thử)
    bool modemReady = false;
    for (int retry = 0; retry < 2 && !modemReady; retry++) { // Giảm từ 3 xuống 2
      Serial.printf("[CELL] Thử khởi động modem lần %d...\n", retry + 1);
      
      if (modem.restart()) {
        modemReady = true;
        Serial.println("[CELL] Modem restart thành công");
        // Add delay for modem to stabilize after restart
        delay(2000);
        esp_task_wdt_reset();
        
        // Clear any garbage data in UART buffer
        while (modem.stream.available()) {
          modem.stream.read();
          delayMicroseconds(100);
        }
        esp_task_wdt_reset();
        
      } else {
        Serial.println("[CELL] restart failed, trying init()...");
        esp_task_wdt_reset(); // Reset watchdog during init
        delay(1000); // Giảm từ 2000ms xuống 1000ms
        if (modem.init()) {
          modemReady = true;
          Serial.println("[CELL] Modem init thành công");
          delay(2000);
          esp_task_wdt_reset();
          
          // Clear buffer after init too
          while (modem.stream.available()) {
            modem.stream.read();
            delayMicroseconds(100);
          }
          esp_task_wdt_reset();
        } else {
          Serial.printf("[CELL] Init thất bại lần %d\n", retry + 1);
          esp_task_wdt_reset();
          delay(2000); // Giảm từ 3000ms xuống 2000ms
        }
      }
    }
    
    if (!modemReady) {
      Serial.println("[CELL] Không thể khởi động modem sau 2 lần thử");
      logCEER();
      return false;
    }
    
    String name = modem.getModemName();
    String info = modem.getModemInfo();
    Serial.println("[CELL] Modem: " + name + " | " + info);

    // Buộc LTE-only nếu cấu hình yêu cầu
    #if CELL_FORCE_LTE_ONLY
      Serial.println("[CELL] Forcing LTE-only mode (CNMP=38, C preferred) ...");
      modem.sendAT("+CNMP=38"); modem.waitResponse(2000); // 38 = LTE only (SIMCOM)
      modem.sendAT("+CMNB=1"); modem.waitResponse(2000);  // CAT-M/NB pref if supported; harmless otherwise
      esp_task_wdt_reset(); // Reset watchdog after LTE mode setup
    #endif

    // Hiển thị RAT hiện tại để xác nhận công nghệ mạng
    modem.sendAT("+CPSI?"); modem.waitResponse(2000);
    esp_task_wdt_reset(); // Reset watchdog after checking RAT
    isModemReady = true;
    
    // EARLY CHECK: Kiểm tra SIM card NGAY sau khi modem khởi động
    Serial.println("[CELL] Kiểm tra SIM card...");
    modem.sendAT("+CPIN?");
    int cpinCheck = modem.waitResponse(1000);
    esp_task_wdt_reset();
    
    if (cpinCheck != 1) {
      Serial.println("[CELL] ❌ SIM card NOT detected! Cannot use 4G.");
      Serial.println("[CELL] Fallback: Hãy dùng WiFi hoặc kiểm tra kết nối SIM.");
      return false;  // Return immediately, don't try GPRS
    }
    Serial.println("[CELL] ✅ SIM card detected, continuing with 4G...");
    esp_task_wdt_reset();
    
    // DEBUG: Test GPRS connectivity NGAY TẠI ĐÂY
    Serial.println("[CELL] === DEBUG GPRS CONNECTIVITY ===");
    
    // 0. Monitor CSQ from start
    int csq0 = modem.getSignalQuality();
    Serial.printf("[CELL] CSQ BEFORE debug: %d\n", csq0);
    esp_task_wdt_reset();
    
    // 1. Activate PDP context
    Serial.println("[CELL] 1. Activating PDP context (+CGACT=1,1)...");
    modem.sendAT("+CGACT=1,1");
    int cgactResult = modem.waitResponse(3000);
    Serial.printf("[CELL] +CGACT response: %d\n", cgactResult);
    int csq1 = modem.getSignalQuality();
    Serial.printf("[CELL] CSQ after CGACT: %d\n", csq1);
    esp_task_wdt_reset();
    
    // 1b. Query PDP context status
    Serial.println("[CELL] 1b. Query PDP context status (+CGACT?)...");
    modem.sendAT("+CGACT?");
    int cgactQueryResult = modem.waitResponse(2000);
    Serial.printf("[CELL] +CGACT? response: %d\n", cgactQueryResult);
    int csq2 = modem.getSignalQuality();
    Serial.printf("[CELL] CSQ after CGACT?: %d\n", csq2);
    esp_task_wdt_reset();
    
    // 1c. Query PDP definition
    Serial.println("[CELL] 1c. Query PDP definition (+CGDCONT?)...");
    modem.sendAT("+CGDCONT?");
    int cgdcontResult = modem.waitResponse(2000);
    Serial.printf("[CELL] +CGDCONT? response: %d\n", cgdcontResult);
    int csq3 = modem.getSignalQuality();
    Serial.printf("[CELL] CSQ after CGDCONT?: %d\n", csq3);
    esp_task_wdt_reset();
    
    // 2. Get IP address
    Serial.println("[CELL] 2. Getting IP address (+CIFSR)...");
    modem.sendAT("+CIFSR");
    int cifResult = modem.waitResponse(2000);
    Serial.printf("[CELL] +CIFSR response: %d\n", cifResult);
    int csq4 = modem.getSignalQuality();
    Serial.printf("[CELL] CSQ after CIFSR: %d\n", csq4);
    esp_task_wdt_reset();
    
    // 2b. Get IP address details (+CGPADDR)
    Serial.println("[CELL] 2b. Getting PDP address details (+CGPADDR=1)...");
    modem.sendAT("+CGPADDR=1");
    int cgpaddrResult = modem.waitResponse(2000);
    Serial.printf("[CELL] +CGPADDR response: %d\n", cgpaddrResult);
    int csq5 = modem.getSignalQuality();
    Serial.printf("[CELL] CSQ after CGPADDR: %d\n", csq5);
    esp_task_wdt_reset();
    
    // 3. Test ping 8.8.8.8
    Serial.println("[CELL] 3. Testing ping to 8.8.8.8 (+CPING)...");
    modem.sendAT("+CPING=\"8.8.8.8\",16,32,10000");
    int pingResult = modem.waitResponse(15000);
    Serial.printf("[CELL] +CPING response: %d\n", pingResult);
    int csq6 = modem.getSignalQuality();
    Serial.printf("[CELL] CSQ after CPING: %d\n", csq6);
    esp_task_wdt_reset();
    
    Serial.println("[CELL] === END DEBUG GPRS ===");
    Serial.printf("[CELL] CSQ Summary: Start=%d, End=%d (Delta=%d)\n", csq0, csq6, (csq6 - csq0));
    if (csq0 < 31 && csq6 >= 31) {
      Serial.println("[CELL] ⚠️ WARNING: CSQ dropped from OK to 99 during debug!");
      Serial.println("[CELL] This suggests: SIM not really registered, OR modem lost connection");
    }
    Serial.println("[CELL] Possible cause: APN not configured, SIM not active, or network provider issue");
    esp_task_wdt_reset();
  }

  // Kiểm tra kết nối mạng với retry (giảm số lần thử)
  if (modem.isNetworkConnected()) {
    Serial.println("[CELL] Đã kết nối mạng");
    return true;
  }
  
  Serial.print("[CELL] Connecting cellular data with APN: ");
  Serial.println(CELL_APN);
  
  if (millis() - startTime > timeout) {
    Serial.println("[CELL] ⚠️ Khởi tạo kéo dài, vẫn tiếp tục GPRS connect để chẩn đoán");
  }
  
  // Thử kết nối GPRS với retry logic (giảm số lần thử)
  bool gprsConnected = false;
  for (int retry = 0; retry < 1 && !gprsConnected; retry++) { // Giảm từ 2 xuống 1
    Serial.printf("[CELL] Thử kết nối GPRS lần %d...\n", retry + 1);
    esp_task_wdt_reset(); // Reset watchdog before GPRS connect
    
    // Now try GPRS connect
    Serial.println("[CELL] Attempting modem.gprsConnect()...");
    if (modem.gprsConnect(CELL_APN, CELL_USER, CELL_PASS)) {
      gprsConnected = true;
      Serial.println("[CELL] GPRS connected");
      esp_task_wdt_reset(); // Reset watchdog after successful GPRS connect
    } else {
      Serial.printf("[CELL] GPRS connect thất bại lần %d\n", retry + 1);
      logCEER();
      esp_task_wdt_reset();
      delay(2000); // Giảm từ 3000ms xuống 2000ms
    }
  }
  
  if (!gprsConnected) {
    Serial.println("[CELL] Không thể kết nối GPRS sau các lần thử");
    // In thêm trạng thái mạng để khoanh vùng lỗi
    int csq = modem.getSignalQuality();
    Serial.printf("[CELL] CSQ cuối: %d\n", csq);
    modem.sendAT("+CPIN?"); modem.waitResponse(1000);
    modem.sendAT("+CREG?" ); modem.waitResponse(1000);
    modem.sendAT("+CGREG?"); modem.waitResponse(1000);
    modem.sendAT("+CEREG?" ); modem.waitResponse(1000);
    esp_task_wdt_reset(); // Reset watchdog after diagnostic AT commands
    logCEER();
    esp_task_wdt_reset(); // Reset watchdog after CEER log
    return false;
  }

  // Bật hiển thị lỗi chi tiết và chuẩn bị TCP/IP stack
  modem.sendAT("+CMEE=2"); modem.waitResponse(1000); // Giảm timeout
  esp_task_wdt_reset(); // Reset watchdog during AT commands
  
  // Đảm bảo NETOPEN đang chạy với retry logic (giảm số lần thử)
  modem.sendAT("+NETCLOSE"); modem.waitResponse(1000); // Giảm timeout
  esp_task_wdt_reset(); // Reset watchdog after NETCLOSE
  {
    bool netOpened = false;
    for (int i = 0; i < 2 && !netOpened; i++) { // Giảm từ 3 xuống 2
      Serial.printf("[CELL] Thử NETOPEN lần %d...\n", i + 1);
      modem.sendAT("+NETOPEN");
      int r = modem.waitResponse(5000); // Giảm từ 10000ms xuống 5000ms
      esp_task_wdt_reset(); // Reset watchdog during NETOPEN wait
      if (r == 1) { 
        netOpened = true; 
        Serial.println("[CELL] NETOPEN thành công");
        esp_task_wdt_reset(); // Reset watchdog after successful NETOPEN
        break; 
      }
      Serial.println("[CELL][AT] NETOPEN retry...");
      esp_task_wdt_reset(); // Reset watchdog during retries
      delay(1000); // Giảm từ 2000ms xuống 1000ms
    }
    if (!netOpened) { 
      Serial.println("[CELL][AT] NETOPEN fail sau 2 lần thử");
      esp_task_wdt_reset(); // Reset watchdog before returning false
      return false; 
    }
  }
  
  // Cấu hình DNS
  modem.sendAT("+CDNSCFG=\"8.8.8.8\",\"1.1.1.1\""); modem.waitResponse(1000); // Giảm timeout
  esp_task_wdt_reset(); // Reset watchdog before NTP sync
  
  // Đồng bộ thời gian (không chặn nếu lỗi) - giảm timeout
  Serial.println("[CELL] Đồng bộ thời gian...");
  modem.sendAT("+CNTP=\"pool.ntp.org\",0"); modem.waitResponse(500); // Giảm timeout
  modem.sendAT("+CNTP"); modem.waitResponse(2000); // Giảm từ 5000ms xuống 2000ms
  esp_task_wdt_reset(); // Reset watchdog after NTP attempt
  
  Serial.println("[CELL] Kết nối 4G hoàn tất!");
  return true;
}

bool cellularHttpPost(const char* host, uint16_t port, const char* path, const String& body, String& response) {
  // Nếu cổng 443, dùng HTTPS AT (CHTTPS)
  // ĐÃ TẮT HTTPS: luôn dùng HTTP thường

  // HTTP thường
  Serial.print("[CELL] HTTP to "); Serial.print(host); Serial.print(":"); Serial.println(port);
  esp_task_wdt_reset(); // Reset watchdog before HTTP operations
  
  // Thử resolve DNS
  modem.sendAT("+CDNSGIP=\"", host, "\"");
  modem.waitResponse(5000);
  esp_task_wdt_reset(); // Reset watchdog after DNS resolution
  
  HttpClient http(gsmClient, host, port);
  //http.connectionKeepAlive(false);
  http.setTimeout(30000); // Tăng timeout từ 15s lên 30s
  esp_task_wdt_reset(); // Reset watchdog before HTTP request
  http.beginRequest();
  http.post(path);
  http.sendHeader("Content-Type", "application/json");
  http.sendHeader("X-API-Key", APPLICATION_KEY);
  http.sendHeader("Content-Length", body.length());
  http.beginBody();
  esp_task_wdt_reset(); // Reset watchdog before sending body
  http.print(body);
  http.endRequest();
  esp_task_wdt_reset(); // Reset watchdog after request sent
  
  int statusCode = http.responseStatusCode();
  if (statusCode < 200 || statusCode >= 300) {
    Serial.print("[CELL] HTTP "); Serial.println(statusCode);
    logCEER();
    esp_task_wdt_reset(); // Reset watchdog before return
    return false;
  }
  response = http.responseBody();
  http.stop();
  Serial.print("[CELL] HTTP "); Serial.println(statusCode);
  esp_task_wdt_reset(); // Reset watchdog after successful response
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
