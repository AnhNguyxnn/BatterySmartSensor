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

/**
 * @file main.cpp
 * @brief Firmware chính cho thiết bị Battery Smart Sensor (ESP32).
 *
 * Chức năng chính:
 * - Đọc dữ liệu nhiệt độ, khí, lửa và xử lý cảnh báo theo ngưỡng cấu hình.
 * - Upload dữ liệu lên backend qua Wi-Fi hoặc modem 4G (TinyGSM).
 * - Cung cấp giao diện web tại chỗ để theo dõi, cấu hình Wi-Fi và cập nhật firmware OTA.
 * - Vận hành theo kiến trúc đa task (networkTask, uploadTask) để giữ giao diện luôn phản hồi.
 */

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
bool alertActive = false;
int fireValue10 = 0; // KY-026 analog value (0..1023)
bool tempAlertFlag = false;
bool smokeAlertFlag = false;
bool fireAlertFlag = false;
static bool buzzerIsOn = false;
static bool relayActiveLowRuntime = RELAY_ACTIVE_LOW; // Cho phép đổi mode tại runtime

// Biến cho MQ-135 filtering (không dùng preheat gating)
int smokeHistory[MOVING_AVERAGE_SIZE];
int smokeHistoryIndex = 0;
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
// Trạng thái lần kiểm tra firmware gần nhất (để tránh báo sai khi offline)
volatile bool lastFirmwareCheckSuccess = false;
String lastFirmwareCheckError = "";
  // Concurrency guard & rate limiting for firmware check
volatile bool firmwareCheckInProgress = false;
unsigned long lastFirmwareCheckRequestAt = 0;
const unsigned long FIRMWARE_CHECK_MIN_INTERVAL_MS = 30000; // 30s debounce

// Biến trạng thái kết nối
enum ConnectionMode {
  CONNECTION_NONE = 0,
  CONNECTION_WIFI_FIRST = 1,  // Đã kết nối WiFi từ đầu, fallback 4G nếu WiFi mất
  CONNECTION_4G_FIRST = 2     // Đã kết nối 4G từ đầu, không thử WiFi nữa
};
ConnectionMode currentConnectionMode = CONNECTION_NONE;
bool connectionEstablished = false;
bool networkTaskCompleted = false;  // CẤM UPLOAD TRƯỚC KHI NETWORK TASK XONG

// Debounce WiFi scan to avoid spam
static volatile bool wifiScanInProgress = false;
static unsigned long wifiScanLastStartMs = 0;

// Biến quản lý upload async
static bool uploadPending = false;
static bool urgentUploadPending = false;
static String uploadBody;
static SemaphoreHandle_t uploadMutex = NULL;  // Thread-safe protection

// Chime khởi động sau khi setup mạng
#if STARTUP_CHIME_ENABLED
static bool startupChimeQueued = false;
static bool startupChimeDone = false;
static uint8_t startupChimeStep = 0;
static unsigned long startupChimeNextAt = 0;
static bool startupChimeWas4G = false; // true: 1 beep; false: 2 beeps with 0.5s gap
#endif

// Khai báo các hàm
void readSensors();
void checkAlerts();
void activateAlerts();
void deactivateAlerts();
void buzzerOn();
void buzzerOff();
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
void uploadImmediateCritical();
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
void ensureAdminAP();
void handleFirmwareUpdateWiFi();
void handleFirmwareUpdate4G();
void handleFirmwareCheck();
void forceSyncNTP();
void networkTask(void* param);
void startMainAP();
void handleFirmwareUploadData();
void handleFirmwareUploadComplete();

/**
 * @brief Bật còi cảnh báo thông qua relay (theo cấu hình active-high/low).
 */
void buzzerOn() {
  // Đơn giản hóa: ON = kích relay theo mức ACTIVE
  digitalWrite(RELAY_PIN, relayActiveLowRuntime ? LOW : HIGH);
  buzzerIsOn = true;
}

/**
 * @brief Tắt còi cảnh báo thông qua relay.
 */
void buzzerOff() {
  // Đơn giản hóa: OFF = thả relay (mức INACTIVE)
  digitalWrite(RELAY_PIN, relayActiveLowRuntime ? HIGH : LOW);
  buzzerIsOn = false;
}

/**
 * @brief Task nền chuyên xử lý upload dữ liệu lên backend mà không chặn loop chính.
 *
 * Quy trình:
 * - Chờ cờ `uploadPending` hoặc `urgentUploadPending`.
 * - Sao chép payload dưới mutex để tránh race condition.
 * - Ưu tiên 4G nếu hệ thống đang chạy ở mode 4G; fallback Wi-Fi khi có.
 * - Gửi xong thì hạ cờ, tiếp tục vòng lặp.
 */
void uploadTask(void* param) {
  // Disable watchdog cho uploadTask vì nó chạy HTTP operations
  esp_task_wdt_delete(NULL);

  Serial.println("[UPLOAD] Task khởi động...");

  while (true) {
    // ƯU TIÊN URGENT: Kiểm tra urgent trước, nếu có thì bỏ qua upload thường
    bool isUrgent = urgentUploadPending;
    bool isNormal = uploadPending && !urgentUploadPending; // Chỉ upload thường nếu không có urgent
    
    if (isUrgent || isNormal) {
      if (!networkTaskCompleted) {
        // Chưa sẵn sàng mạng, đợi lần sau
        delay(500);
        continue;
      }
      
      if (isUrgent) {
        Serial.println("[UPLOAD] Bắt đầu upload (URGENT - CẢNH BÁO)...");
      } else {
        Serial.println("[UPLOAD] Bắt đầu upload...");
      }

      // Copy uploadBody với mutex protection - copy ngay trước khi gửi để lấy dữ liệu mới nhất
      String localBody;
      bool wasUrgent = false;
      if (xSemaphoreTake(uploadMutex, portMAX_DELAY)) {
        localBody = uploadBody;  // Safe copy
        wasUrgent = urgentUploadPending; // Lưu trạng thái urgent tại thời điểm copy
        xSemaphoreGive(uploadMutex);
      }

      Serial.println("[UPLOAD] Data size: " + String(localBody.length()) + " bytes");

      // Upload từ từ mà không block main loop
      bool uploadSuccess = false;
      if (currentConnectionMode == CONNECTION_4G_FIRST) {
        Serial.println("[UPLOAD] Trying 4G upload...");
        if (cellularBegin()) {
          String resp;
          bool ok = false;
          if (wasUrgent) {
            ok = cellularHttpPostCritical(BACKEND_HOST, BACKEND_PORT, BACKEND_PATH, localBody, resp);
          } else {
            ok = cellularHttpPost(BACKEND_HOST, BACKEND_PORT, BACKEND_PATH, localBody, resp);
          }
          if (ok) {
            Serial.println(String("[UPLOAD] Upload 4G OK: ") + resp);
            uploadSuccess = true;
          } else {
            Serial.println("[UPLOAD] Upload 4G FAIL");
          }
        } else {
          Serial.println("[UPLOAD] 4G not connected");
        }
      } else if (WiFi.status() == WL_CONNECTED) {
        Serial.println("[UPLOAD] Trying WiFi upload...");
        HTTPClient http;
        String url = String("http://") + String(BACKEND_HOST) + ":" + String(BACKEND_PORT) + String(BACKEND_PATH);
        Serial.println("[UPLOAD] URL: " + url);

        http.begin(url);
        http.addHeader("Content-Type", "application/json");
        http.addHeader("X-API-Key", APPLICATION_KEY);

        int httpCode = http.POST(localBody);
        Serial.println("[UPLOAD] HTTP Code: " + String(httpCode));

        if (httpCode == 200) {
          String response = http.getString();
          Serial.println("[UPLOAD] Upload WiFi OK: " + response);
          uploadSuccess = true;
        } else {
          Serial.println("[UPLOAD] Upload WiFi FAIL: " + String(httpCode));
        }
        http.end();
      } else {
        Serial.println("[UPLOAD] No connection (4G or WiFi)");
      }

      // CHỈ RESET CỜ TƯƠNG ỨNG: Reset urgent nếu đã gửi urgent, reset normal nếu đã gửi normal
      // QUAN TRỌNG: Kiểm tra lại trạng thái trước khi reset để tránh mất dữ liệu cảnh báo mới
      if (xSemaphoreTake(uploadMutex, portMAX_DELAY)) {
        if (wasUrgent && urgentUploadPending) {
          // Chỉ reset nếu vẫn là urgent (có thể có urgent mới xuất hiện trong lúc upload)
          urgentUploadPending = false;
          Serial.println("[UPLOAD] Đã gửi dữ liệu cảnh báo, reset urgentUploadPending");
        }
        if (!wasUrgent && uploadPending && !urgentUploadPending) {
          // Chỉ reset normal nếu không có urgent mới
          uploadPending = false;
          Serial.println("[UPLOAD] Đã gửi dữ liệu định kỳ, reset uploadPending");
        }
        // Nếu có urgent mới xuất hiện trong lúc upload normal, giữ nguyên urgentUploadPending
        if (!wasUrgent && urgentUploadPending) {
          Serial.println("[UPLOAD] Phát hiện cảnh báo mới trong lúc upload, sẽ xử lý ngay ở lần lặp tiếp theo");
        }
        xSemaphoreGive(uploadMutex);
      }
    }

    delay(500);  // Check every 500ms
  }
}

/**
 * @brief Task nền để kiểm tra firmware mới mà không làm giật UI.
 *
 * Task chỉ chạy một lần rồi tự hủy (`vTaskDelete`) sau khi gọi `checkFirmwareUpdate()`.
 */
void firmwareCheckTask(void* param) {
  // Task này độc lập, không share với loopTask
  Serial.println("[FIRMWARE_TASK] Bắt đầu kiểm tra firmware trong background task...");

  // Gọi checkFirmwareUpdate từ task riêng
  checkFirmwareUpdate();

  Serial.println("[FIRMWARE_TASK] Kiểm tra firmware hoàn tất");
  vTaskDelete(NULL); // Xóa task sau khi hoàn thành
}

/**
 * @brief Hàm khởi tạo chính của ESP32.
 *
 * - Thiết lập Serial, watchdog, SPIFFS và GPIO.
 * - Khởi động cảm biến, SoftAP, web server.
 * - Tạo hai task nền: `networkTask` (khởi tạo kết nối) và `uploadTask`.
 */
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
  Serial.println("ESP32 Battery Monitor - Fast Boot Starting...");

  // Create upload mutex EARLY, before any task starts
  uploadMutex = xSemaphoreCreateMutex();
  if (uploadMutex == NULL) {
    Serial.println("Failed to create upload mutex!");
  } else {
    Serial.println("Upload mutex created");
  }

  // Khởi tạo watchdog với timeout dài hơn
  esp_task_wdt_init(60, true);
  esp_task_wdt_add(NULL);

  // Fast Boot Path - chỉ khởi tạo tối thiểu
  Serial.println("Fast Boot Path - khởi tạo tối thiểu...");

  // SPIFFS mount
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS mount failed");
  } else {
    Serial.println("SPIFFS đã khởi tạo");
  }

  // GPIO setup
  // Đặt mức an toàn trước rồi mới set OUTPUT để tránh glitch lúc boot
  digitalWrite(RELAY_PIN, relayActiveLowRuntime ? HIGH : LOW);
  pinMode(RELAY_PIN, OUTPUT);
  if (LED_PIN >= 0) pinMode(LED_PIN, OUTPUT);

  // Bật LED báo đang boot
  if (LED_PIN >= 0) digitalWrite(LED_PIN, HIGH);

  // Khởi tạo cảm biến nhiệt độ
  tempSensor.begin();

  // Khởi tạo ADC cho MQ-135 & KY-026 (Analog)
  analogReadResolution(12);
  analogSetPinAttenuation(SMOKE_SENSOR_PIN, ADC_11db);
  analogSetPinAttenuation(FIRE_SENSOR_ANALOG_PIN, ADC_11db);

  // Start AP management ngay để user có thể truy cập web sớm
  startMainAP();
  startWebServer();

  // Bật task riêng cho phần network để chạy song song
  xTaskCreatePinnedToCore(networkTask, "networkTask", 8192, NULL, 1, NULL, 1);

  // Bật task riêng cho phần upload để chạy song song
  xTaskCreatePinnedToCore(uploadTask, "uploadTask", 8192, NULL, 1, NULL, 1);

  Serial.println("Fast Boot Path done (<5s) - Web interface ready!");
  Serial.println("Network initialization running in background...");
}

/**
 * @brief Task nền chịu trách nhiệm thiết lập kết nối mạng khi khởi động thiết bị.
 *
 * Ưu tiên modem 4G; nếu thất bại sẽ giữ thiết bị ở chế độ AP-only để kỹ thuật viên can thiệp.
 */
void networkTask(void* param) {
  // DISABLE WATCHDOG - networkTask chỉ chạy 1 lần khi startup
  // HTTP operations có thể mất thời gian, không cần watchdog check
  esp_task_wdt_delete(NULL);

  Serial.println("Bắt đầu khởi tạo mạng nền...");

  // Ưu tiên 4G
  if (cellularBegin()) {
    currentConnectionMode = CONNECTION_4G_FIRST;
    // NỤ THÊM STABILIZATION DELAY SAU NETOPEN - KHÔNG CẦN RESET WATCHDOG
    Serial.println("[CELL] Chờ modem ổn định 3 giây...");
    for (int i = 0; i < 6; i++) {  // 6 x 500ms = 3 giây
      delay(500);
      // Watchdog disabled - không cần reset
    }

    connectionEstablished = true;
    Serial.println("4G connected (background)");
#if STARTUP_CHIME_ENABLED
    startupChimeWas4G = true;
#endif
  } else {
    // Không auto connect WiFi; giữ AP-only cho quản trị
    WiFi.setAutoConnect(false);
    WiFi.setAutoReconnect(false);
    Serial.println("4G fail. AP-only mode; skip WiFi auto-connect.");
  }

  if (connectionEstablished) {
    // KHÔNG SYNC NTP - SERVER XỬ LÝ TIMESTAMP
    Serial.println("Network setup hoàn tất - bắt đầu gửi dữ liệu...");

    readSensors();
    uploadImmediate();

    // Không tự động kiểm tra firmware trên boot để tránh gây khó chịu sau khi update
  } else {
    Serial.println("Không thể kết nối mạng - SẼ KHÔNG GỬI DỮ LIỆU");
    Serial.println("Chỉ log thông báo trực tiếp, không upload lên server");
  }

  if (LED_PIN >= 0) digitalWrite(LED_PIN, LOW);
  Serial.println("Network task completed");
  networkTaskCompleted = true;  // CHỈ ĐẶT LÀ TRUE KHI HOÀN TẤT
#if STARTUP_CHIME_ENABLED
  // Luôn xếp hàng chime sau khi setup mạng xong (mọi mode)
  if (!startupChimeDone) {
    startupChimeQueued = true;
    startupChimeStep = 0;
    startupChimeNextAt = millis();
  }
#endif
  // Remove this task from watchdog tracking before deletion to avoid WDT referencing a freed TCB
  esp_task_wdt_delete(NULL);
  vTaskDelete(NULL); // Kết thúc task
}

/**
 * @brief Vòng lặp chính: bảo trì AP, đọc cảm biến, xử lý HTTP và kiểm tra upload.
 */
void loop() {
  // Reset watchdog timer để tránh crash - reset thường xuyên hơn
  esp_task_wdt_reset();

  unsigned long currentTime = millis();

  // Giữ AP quản trị luôn hoạt động (AP-Only an toàn khi xa WiFi)
  static unsigned long lastEnsureAP = 0;
  if (currentTime - lastEnsureAP > 2000) { // mỗi 2s kiểm tra AP
    ensureAdminAP();
    lastEnsureAP = currentTime;
  }

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

  // KHÔNG SYNC NTP - SERVER XỬ LÝ TIMESTAMP
  // Kiểm tra firmware update định kỳ (12 giờ) - chỉ khi có kết nối internet
  // if (currentTime - lastFirmwareCheck >= FIRMWARE_CHECK_INTERVAL) {
  //   // Chỉ kiểm tra firmware nếu đã có kết nối internet ổn định
  //   if (WiFi.status() == WL_CONNECTED || currentConnectionMode == CONNECTION_4G_FIRST) {
  //     Serial.println("Kiểm tra firmware update định kỳ...");
  //     checkFirmwareUpdate();
  //   } else {
  //     Serial.println("[FIRMWARE] Bỏ qua kiểm tra - chưa có kết nối internet");
  //   }
  //   lastFirmwareCheck = currentTime;
  //   esp_task_wdt_reset(); // Reset after firmware check
  // }

  // Upload dữ liệu định kỳ (ưu tiên WiFi, fallback 4G) qua HTTP
  tryBackendUpload();
  esp_task_wdt_reset(); // Reset after upload attempt

#if STARTUP_CHIME_ENABLED
  // Chạy chime khi đã hoàn tất network task và có yêu cầu
  if (networkTaskCompleted && startupChimeQueued && !startupChimeDone) {
    if (currentTime >= startupChimeNextAt) {
      // Pattern theo yêu cầu:
      // - Có 4G: 1 beep (100ms)
      // - Không 4G: 2 beep, mỗi beep 100ms, cách nhau 500ms
      switch (startupChimeStep) {
        case 0: // delay ngắn trước khi beep
          startupChimeNextAt = currentTime + 100;
          startupChimeStep = 1;
          break;
        case 1: // Beep đầu tiên
          buzzerOn();
          startupChimeNextAt = currentTime + 100; // 100ms
          startupChimeStep = 2;
          break;
        case 2: // Kết thúc beep 1
          buzzerOff();
          if (startupChimeWas4G) {
            startupChimeDone = true; // chỉ 1 beep nếu 4G
          } else {
            startupChimeNextAt = currentTime + 500; // cách 0.5s
            startupChimeStep = 3;
          }
          break;
        case 3: // Beep thứ 2
          buzzerOn();
          startupChimeNextAt = currentTime + 100; // 100ms
          startupChimeStep = 4;
          break;
        case 4: // Kết thúc
          buzzerOff();
          startupChimeDone = true;
          break;
      }
    }
  }
#endif

  // Tăng delay để giảm tải CPU và cho phép các task khác chạy
  delay(100);
}

/**
 * @brief Đọc toàn bộ cảm biến và cập nhật biến toàn cục.
 *
 * - DS18B20: lấy nhiệt độ theo °C.
 * - MQ-135: đọc nhiều mẫu, áp dụng median filter + moving average để giảm nhiễu.
 * - KY-026: đọc ADC 12-bit, chuyển về thang 10-bit (0..1023) cho backend.
 */
void readSensors() {
  // Đọc nhiệt độ từ DS18B20
  tempSensor.requestTemperatures();
  temperature = tempSensor.getTempCByIndex(0);

  // // Kiểm tra cảm biến nhiệt độ
  // if (temperature == DEVICE_DISCONNECTED_C) {
  //   Serial.println("Lỗi: Không tìm thấy cảm biến DS18B20!");
  //   Serial.println("Kiểm tra kết nối: VCC->3.3V, GND->GND, Data->GPIO23");
  //   Serial.println("Cần điện trở pull-up 4.7kΩ giữa Data và VCC");
  // }

  // Đọc giá trị cảm biến khí MQ-135 với filtering nâng cao
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

  // Không dùng smokeConnected; chỉ lưu giá trị đo
  lastSmokeValue = smokeValue;

  // Đọc cảm biến lửa KY-026 qua Analog
  int fireSamples[5];
  for (int i = 0; i < 5; i++) {
    fireSamples[i] = analogRead(FIRE_SENSOR_ANALOG_PIN);
    if (i % 2 == 0) { esp_task_wdt_reset(); }
    delayMicroseconds(100);
  }
  int fireMedian = medianFilter(fireSamples, 5);
  int fireMedian10 = fireMedian >> 2; // chuyển 12-bit → 10-bit (0..1023)
  fireValue10 = fireMedian10;
}

/**
 * @brief Xác định trạng thái cảnh báo dựa trên các ngưỡng cấu hình.
 *
 * Khi bất kỳ mô-đun nào vượt ngưỡng, bật còi/LED và lên lịch upload gấp.
 * Đồng thời lưu lại cờ alert để gửi về backend.
 * Gửi lại cảnh báo định kỳ (mỗi 30 giây) khi cảnh báo vẫn còn active để đảm bảo backend nhận được dữ liệu.
 */
void checkAlerts() {
  static unsigned long lastAlertUpload = 0; // Thời gian gửi cảnh báo cuối cùng
  const unsigned long ALERT_REPEAT_INTERVAL = 30000; // Gửi lại cảnh báo mỗi 30 giây
  
  bool tempAlert = (temperature > TEMP_THRESHOLD);
  bool smokeAlert = (smokeValue > SMOKE_THRESHOLD);
  bool fireAlert = (fireValue10 < FIRE_ANALOG_THRESHOLD); // KY-026: giá trị thấp hơn = gần lửa

  int activeModules = 0;
  if (tempAlert) activeModules++;
  if (smokeAlert) activeModules++;
  if (fireAlert) activeModules++;

  // Bỏ SENSITIVITY: chỉ cần 1 mô-đun cảnh báo là báo
  bool shouldAlert = (activeModules >= 1);

  // Lưu cờ cho upload/backend
  tempAlertFlag = tempAlert;
  smokeAlertFlag = smokeAlert;
  fireAlertFlag = fireAlert;

  String alertReason = "";
  if (shouldAlert) {
    // Liệt kê các mô-đun đang cảnh báo (không dùng sensitivity)
    if (tempAlert) {
      alertReason += "NHIET DO CAO (" + String(temperature, 1) + "°C)";
    }
    if (smokeAlert) {
      if (alertReason.length()) alertReason += " | ";
      alertReason += "KHI DOC HAI (" + String(smokeValue) + ")";
    }
    if (fireAlert) {
      if (alertReason.length()) alertReason += " | ";
      alertReason += "LUA (KY-026)";
    }
  }

  // Kích hoạt/tắt cảnh báo và đảm bảo trạng thái còi/LED theo thời gian thực
  if (shouldAlert) {
    unsigned long now = millis();
    bool shouldUploadAlert = false;
    
    if (!alertActive) {
      // Cảnh báo mới xuất hiện - gửi ngay lập tức
      alertActive = true;
      Serial.println("CANH BAO: " + alertReason);
      shouldUploadAlert = true;
      lastAlertUpload = now;
    } else {
      // Cảnh báo vẫn còn active - gửi lại định kỳ để đảm bảo backend nhận được
      if (now - lastAlertUpload >= ALERT_REPEAT_INTERVAL) {
        Serial.println("CANH BAO (lap lai): " + alertReason);
        shouldUploadAlert = true;
        lastAlertUpload = now;
      }
    }
    
    // Gửi dữ liệu cảnh báo nếu cần
    if (shouldUploadAlert) {
      uploadImmediateCritical();
    }
    
    // Đảm bảo bật còi/LED khi đang trong trạng thái cảnh báo
    if (LED_PIN >= 0) digitalWrite(LED_PIN, HIGH);
    if (!buzzerIsOn) buzzerOn();
  } else {
    if (alertActive) {
      alertActive = false;
      lastAlertUpload = 0; // Reset timer khi hết cảnh báo
      Serial.println("Tinh trang binh thuong");
    }
    // Đảm bảo tắt còi/LED khi hết cảnh báo
    if (LED_PIN >= 0) digitalWrite(LED_PIN, LOW);
    if (buzzerIsOn) buzzerOff();
  }
}

/**
 * @brief Kích hoạt còi/LED và in log chi tiết nguyên nhân cảnh báo.
 *
 * Hàm được giữ lại cho trường hợp muốn ép bật cảnh báo thủ công (debug).
 */
void activateAlerts() {
  // Bật LED và còi cảnh báo
  if (LED_PIN >= 0) digitalWrite(LED_PIN, HIGH);
  buzzerOn();

  // In thông báo chi tiết theo thứ tự ưu tiên
  Serial.println("=== CHI TIẾT CẢNH BÁO ===");

  if (temperature > TEMP_THRESHOLD) {
    Serial.println("NHIET DO CAO: " + String(temperature, 1) + "°C (Nguy hiểm!)");
  }

  if (smokeValue > SMOKE_THRESHOLD) {
    Serial.println("KHI DOC HAI: " + String(smokeValue) + " (Pin có thể xì khí)");
  }
  if (fireValue10 < FIRE_ANALOG_THRESHOLD) {
    Serial.println("LUA (KY-026): " + String(fireValue10) + " (< ngưỡng " + String(FIRE_ANALOG_THRESHOLD) + ")");
  }

  Serial.println("=========================");
}

/**
 * @brief Tắt còi và LED cảnh báo, trả thiết bị về trạng thái yên lặng.
 */
void deactivateAlerts() {
  // Tắt LED và còi cảnh báo
  if (LED_PIN >= 0) digitalWrite(LED_PIN, LOW);
  buzzerOff();
}

/**
 * @brief Khởi tạo logic kết nối mạng ở chế độ tuần tự (không dùng trong fast boot).
 *
 * Hiện firmware sử dụng networkTask chạy nền, hàm này giữ lại để tham khảo và debug.
 */
void startNetworking() {
  Serial.println("Bắt đầu kết nối mạng...");

  // Bước 0: Luôn khởi tạo AP chính trước (đảm bảo AP luôn có)
  Serial.println("Khởi tạo AP chính trước...");
  startMainAP();

  // Bước 1: Thử kết nối 4G trước (ưu tiên cao nhất)
  #if ENABLE_CELLULAR_UPLOAD
  Serial.println("Thử kết nối 4G...");

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
      Serial.println("4G đã kết nối!");
      currentConnectionMode = CONNECTION_4G_FIRST;
      connectionEstablished = true;
#if STARTUP_CHIME_ENABLED
      startupChimeQueued = true; // Đánh dấu ready để chơi chime sau khi network task hoàn tất
#endif
      return; // Thành công với 4G, không cần WiFi
    } else {
      Serial.printf("4G kết nối thất bại lần %d\n", retry + 1);
    }
  }

  if (!cellularConnected) {
    Serial.println("4G kết nối thất bại sau 2 lần thử");
  }
  #endif

  // Bước 2: Bỏ auto WiFi fallback; giữ AP-only
  Serial.println("Bỏ qua WiFi fallback. Giữ AP quản trị hoạt động");
}

/**
 * @brief Khởi động SoftAP quản trị và đăng ký các route phục vụ cấu hình tại chỗ.
 *
 * AP này luôn bật kể cả khi thiết bị tham gia Wi-Fi khác, giúp kỹ thuật viên truy cập.
 */
void startMainAP() {
  // AP chính hoạt động liên tục để quản trị
  // Luôn giữ AP bật, không bị ảnh hưởng bởi STA
  WiFi.mode(WIFI_AP_STA);
  // Đảm bảo AP vẫn giữ nguyên khi xử lý STA
  WiFi.persistent(true);
  // Không tự động kết nối lại WiFi STA trừ khi người dùng yêu cầu
  WiFi.setAutoConnect(false);
  WiFi.setAutoReconnect(false);

  bool apStarted = WiFi.softAP(AP_SSID, AP_PASSWORD);

  if (apStarted) {
    IPAddress apIP = WiFi.softAPIP();
    Serial.println("AP Quản trị đã khởi động:");
    Serial.print("SSID: "); Serial.println(AP_SSID);
    Serial.print("Password: "); Serial.println(AP_PASSWORD);
    Serial.print("IP: "); Serial.println(apIP);
    Serial.println("Truy cập: http://192.168.4.1");
    Serial.println("AP này sẽ luôn hoạt động, không bị tắt khi kết nối WiFi");

    // Đăng ký routes cho quản trị
    server.on("/wifi-scan", handleWiFiScan);
    server.on("/wifi-connect", HTTP_POST, handleWiFiConnect);
    server.on("/wifi-reset", HTTP_POST, handleWiFiReset);
    server.on("/firmware-update", handleFirmwareWebInterface);
  } else {
    Serial.println("Không thể khởi động AP quản trị!");
  }
}

// Đảm bảo AP đang bật (khôi phục nếu bị tắt bởi driver)
/**
 * @brief Giữ cho AP quản trị luôn hoạt động, tránh trường hợp driver tắt khi không có client.
 */
void ensureAdminAP() {
  if (WiFi.getMode() != WIFI_AP_STA && WiFi.getMode() != WIFI_AP) {
    WiFi.mode(WIFI_AP_STA);
  }
  if (WiFi.softAPgetStationNum() == 0 && WiFi.softAPIP().toString() == String("0.0.0.0")) {
    WiFi.softAP(AP_SSID, AP_PASSWORD);
  }
}


/**
 * @brief Đăng ký toàn bộ endpoint cho giao diện web và API nội bộ của thiết bị.
 */
void startWebServer() {
  server.on("/", handleRoot);
  server.on("/api/status", HTTP_GET, handleApiStatus);
  // Removed test endpoints for buzzer and relay-mode per request

  // Generic GPIO test endpoint: /api/gpio?pin=18&level=1
  server.on("/api/gpio", HTTP_GET, [](){
    if (!server.hasArg("pin")) { server.send(400, "application/json", "{\"error\":\"missing pin\"}"); return; }
    int pin = server.arg("pin").toInt();
    pinMode(pin, OUTPUT);
    if (server.hasArg("level")) {
      int level = server.arg("level").toInt();
      digitalWrite(pin, level ? HIGH : LOW);
    }
    int lv = digitalRead(pin);
    String res = String("{\"pin\":") + pin + ",\"level\":" + lv + "}";
    server.send(200, "application/json", res);
  });
  // Common browser requests
  server.on("/favicon.ico", HTTP_GET, [](){ server.send(204); });
  server.on("/apple-touch-icon.png", HTTP_GET, [](){ server.send(204); });
  server.on("/apple-touch-icon-precomposed.png", HTTP_GET, [](){ server.send(204); });
  server.on("/robots.txt", HTTP_GET, [](){ server.send(200, "text/plain", "User-agent: *\nDisallow:"); });
  server.on("/generate_204", HTTP_GET, [](){ server.send(204); });
  server.on("/hotspot-detect.html", HTTP_GET, [](){ server.sendHeader("Location", "/"); server.send(302); });
  server.on("/ncsi.txt", HTTP_GET, [](){ server.send(200, "text/plain", "Microsoft NCSI"); });
  server.on("/update", HTTP_POST, [](){ server.send(410, "text/plain", "Manual upload removed"); });

  // WiFi setup routes (luôn có sẵn)
  server.on("/wifi-scan", handleWiFiScan);
  server.on("/wifi-connect", HTTP_POST, handleWiFiConnect);
  server.on("/wifi-connect", HTTP_GET, [](){ server.sendHeader("Location", "/wifi-scan"); server.send(302); });
  server.on("/wifi-reset", HTTP_POST, handleWiFiReset);

  // Firmware update routes
  server.on("/firmware-update", handleFirmwareWebInterface);
  server.on("/api/firmware/check", HTTP_GET, handleFirmwareCheck);
  server.on("/api/firmware/update-wifi", HTTP_POST, handleFirmwareUpdateWiFi);
  server.on("/api/firmware/update-wifi", HTTP_OPTIONS, [](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Headers", "*");
    server.sendHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    server.send(204);
  });
  server.on("/api/firmware/update-4g", HTTP_POST, handleFirmwareUpdate4G);

  // Catch-all
  server.onNotFound([](){
    server.send(404, "text/plain", "Not found");
  });

  server.begin();
  Serial.print("HTTP server lắng nghe tại port ");
  Serial.println(HTTP_SERVER_PORT);
}

/**
 * @brief Sinh nội dung HTML cho bảng điều khiển quản trị của thiết bị.
 *
 * Trang hiển thị số liệu cảm biến, trạng thái kết nối, thông báo firmware và các hành động quản trị.
 */
String renderHtml() {
  String html = "<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>Battery Monitor - Admin Panel</title><style>body{font-family:Arial;padding:16px} .card{border:1px solid #ddd;border-radius:8px;padding:12px;margin:8px 0} .ok{color:#2e7d32}.warn{color:#d32f2f} .grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:12px} .network-status{padding:10px;border-radius:5px;margin:10px 0} .wifi-connected{background:#e8f5e8;border-left:4px solid #4caf50} .cellular-connected{background:#e3f2fd;border-left:4px solid #2196f3} .ap-mode{background:#fff3e0;border-left:4px solid #ff9800} .button{background:#2196f3;color:white;padding:8px 16px;border:none;border-radius:3px;cursor:pointer;margin:5px;text-decoration:none;display:inline-block} .firmware-notification{background:#ffebee;border:2px solid #f44336;border-radius:8px;padding:15px;margin:15px 0} .firmware-notification h3{color:#d32f2f;margin-top:0} .close-btn{float:right;background:#f44336;color:white;border:none;padding:5px 10px;border-radius:3px;cursor:pointer}</style></head><body>";
  html += "<h2>Battery Smart Sensor - Admin Panel</h2>";
  // Flash message placeholder
  html += "<div id='flash'></div>";

  // Waiting overlay (inserted)
  html += "<div id='overlay' style=\"display:none;position:fixed;inset:0;background:rgba(0,0,0,.6);z-index:9999;color:#fff;\">";
  html += "<div style=\"position:absolute;top:50%;left:50%;transform:translate(-50%,-50%);text-align:center;max-width:90%\">";
  html += "<h2>Đang kiểm tra/cập nhật phiên bản phần mềm qua dữ liệu di động...</h2>";
  html += "<p>Vui lòng chờ 2-3 phút tùy chất lượng sóng. Đừng thoát hay reload trang.</p>";
  html += "<div style=\"margin-top:16px;font-size:14px;opacity:.9\" id='overlayHint'>Đang kết nối...</div>";
  html += "</div></div>";
  // Modal popup for generic messages
  html += "<div id='modalRoot' style=\"display:none;position:fixed;inset:0;background:rgba(0,0,0,.6);z-index:10000;color:#333;\">";
  html += "<div style=\"position:absolute;top:50%;left:50%;transform:translate(-50%,-50%);background:#fff;border-radius:8px;min-width:280px;max-width:90%;padding:16px;box-shadow:0 4px 16px rgba(0,0,0,.35);text-align:center\">";
  html += "<h3 id='modalTitle' style=\"margin:0 0 8px\"></h3>";
  html += "<div id='modalText' style=\"opacity:.85\"></div>";
  html += "</div></div>";

  // Firmware notification banner
  if (firmwareUpdateAvailable) {
    html += "<div class='firmware-notification'>";
    html += "<button class='close-btn' onclick='this.parentElement.style.display=\"none\"'>✕</button>";
    html += "<h3>Firmware Update Available!</h3>";
    html += "<p><strong>New Version:</strong> " + latestFirmwareDisplayVersion + "</p>";
    html += "<p><strong>Current Version:</strong> " + String(FIRMWARE_VERSION) + " (Build " + String(FIRMWARE_BUILD) + ")</p>";
    html += "<button class='button' onclick='checkFirmwareUpdate()'>Update Now</button>";
    html += "<button class='button' onclick='this.parentElement.style.display=\"none\"'>Update Later</button>";
    html += "</div>";
  }

  // Network status
  html += "<div class='network-status ";
  if (currentConnectionMode == CONNECTION_4G_FIRST) {
    html += "cellular-connected'>";
    html += "<strong>Hệ thống đã nhận SIM!</strong><br>";
    // Hiển thị thông tin trạng thái 4G
    html += cellularStatusSummary();
  } else if (currentConnectionMode == CONNECTION_WIFI_FIRST) {
    html += "wifi-connected'>";
    html += "<strong>WiFi-First Mode</strong><br>";
    if (WiFi.status() == WL_CONNECTED) {
      html += "SSID: " + WiFi.SSID() + "<br>";
      html += "IP: " + WiFi.localIP().toString() + "<br>";
      html += "WiFi Connected";
    } else {
      html += "WiFi Disconnected<br>";
      html += "Fallback to 4G";
    }
  } else {
    html += "ap-mode'>";
    html += "<strong>Không phát hiện SIM!</strong><br>";
    html += "Không thể kết nối tới máy chủ<br>";
    html += "Vui lòng kiểm tra lại kết nối SIM và thử lại!";
  }
  html += "</div>";

  html += "<div class='grid'>";
  html += "<div class='card'><h3>Nhiệt Độ</h3><div>" + String(temperature, 1) + " °C</div></div>";
  html += "<div class='card'><h3>Chất Lượng Không Khí (MQ-135)</h3><div>" + String(smokeValue) + "</div></div>";
  html += "<div class='card'><h3>Lửa (KY-026)</h3><div>" + String(fireValue10) + "</div></div>";
  html += String("<div class='card'><h3>Cảnh Báo</h3><div class='") + (alertActive ? "warn'>CẢNH BÁO" : "ok'>Bình thường") + "</div></div>";
  html += "</div>";

  // Admin actions (ẩn WiFi Setup khỏi trang chính)
  html += "<div style='margin:20px 0'>";
  html += "<h3>Admin Actions</h3>";
  html += "<button class='button' onclick='checkFirmwareUpdate()'>Kiểm tra cập nhật</button>";
  html += "<a href='/api/status' class='button'>Debug</a>";
  html += "</div>";

  html += "<p><small>Device: " DEVICE_ID " | Firmware: " + String(FIRMWARE_VERSION) + " (Build " + String(FIRMWARE_BUILD) + ")</small></p>";
  html += "<script>";
  html += "function showFlash(msg){var f=document.getElementById('flash');f.innerHTML=\"<div style='background:#e8f5e9;border-left:4px solid #4caf50;padding:10px;border-radius:4px;margin:10px 0'>\"+msg+\"</div>\";}";
  html += "function showModal(t, m){var r=document.getElementById('modalRoot');document.getElementById('modalTitle').textContent=t;document.getElementById('modalText').textContent=m;r.style.display='block';}";
  html += "function hideModal(){document.getElementById('modalRoot').style.display='none';}";
  // Dùng localStorage để truyền trạng thái thay vì query string
  html += "(function(){try{if(localStorage.getItem('updated')==='1'){showModal('Cập nhật thành công','Vui lòng chờ 2-3 phút để khởi động lại...');localStorage.removeItem('updated');}}catch(e){}})();";
  html += "(function(){try{if(localStorage.getItem('updating')==='1'){showModal('Đang cập nhật firmware...','Vui lòng chờ, không thao tác');localStorage.removeItem('updating');fetch('/api/firmware/update-wifi',{method:'POST'}).then(r=>r.json()).then(d=>{if(d.status==='success'){localStorage.setItem('updated','1');location.href='/';}else{hideModal();alert('Lỗi cập nhật: '+(d.error||'Unknown'));}}).catch(e=>{hideModal();alert('Lỗi kết nối: '+e);});}}catch(e){}})();";
  // Nếu được đưa về với updating=1: hiện overlay chờ và tự gọi update để ngăn spam
  html += "(function(){try{var p=new URLSearchParams(location.search);if(p.get('updating')==='1'){";
  html += "  const o=document.getElementById('overlay');const h=document.getElementById('overlayHint');h.textContent='Đang cập nhật firmware...';o.style.display='block';";
  html += "  fetch('/api/firmware/update-wifi',{method:'POST'})";
  html += "    .then(r=>r.json())";
  html += "    .then(d=>{ if(d.status==='success'){ location.href='/?updated=1'; } else { o.style.display='none'; alert('Lỗi cập nhật: '+(d.error||'Unknown')); } })";
  html += "    .catch(e=>{ o.style.display='none'; alert('Lỗi kết nối: '+e); });";
  html += "}}catch(e){}})();";
  html += "function showOverlay(msg){const o=document.getElementById('overlay');const h=document.getElementById('overlayHint');if(msg)h.textContent=msg;o.style.display='block';}";
  html += "function hideOverlay(){document.getElementById('overlay').style.display='none';}";
  html += "function checkFirmwareUpdate() {";
  html += "  showOverlay('Đang kiểm tra firmware...');";
  html += "  fetch('/api/firmware/check?sync=1')";
  html += "    .then(r => r.json())";
  html += "    .then(d => {";
  html += "      hideOverlay();";
  html += "      if (d.error) { alert('Không thể kiểm tra firmware: ' + d.error); return; }";
  html += "      if (d.update_available) {";
  html += "        if (confirm('Có firmware mới: ' + d.latest_version + '\\n\\nBạn có muốn cập nhật không?')) {";
  html += "          window.location.href = '/firmware-update';";
  html += "        }";
  html += "      } else {";
  html += "        alert('Đã là phiên bản mới nhất: ' + d.current_version + ' (Build ' + d.current_build + ')');";
  html += "      }";
  html += "    })";
  html += "    .catch(e => { hideOverlay(); alert('Kiểm tra firmware thất bại: ' + e); });";
  html += "}";
  // Tắt auto reload để tránh giật lag
  // html += "setInterval(()=>fetch('/api/status').then(r=>r.json()).then(d=>{location.reload()}),5000)";
  html += "</script>";
  html += "</body></html>";
  return html;
}

/**
 * @brief Endpoint `/` - trả về trang HTML tổng quan.
 */
void handleRoot() {
  server.send(200, "text/html", renderHtml());
}

/**
 * @brief Endpoint `/api/status` - trả về dữ liệu cảm biến dạng JSON phục vụ debug.
 */
void handleApiStatus() {
  String json;
  {
    JsonDocument doc;
    // Không gửi timestamp trong API status, chỉ gửi dữ liệu cảm biến
    // doc["timestamp"] = getCurrentTimestamp();
    doc["temperature"] = temperature;
    doc["smoke_value"] = smokeValue;
    // MQ-135: không có preheat gating, bỏ trường cũ
    doc["fire_value"] = fireValue10; // KY-026 10-bit
    // Debug states
    doc["temp_alert"] = tempAlertFlag;
    doc["smoke_alert"] = smokeAlertFlag;
    doc["fire_alert"] = fireAlertFlag;
    doc["buzzer_is_on"] = buzzerIsOn;
    doc["relay_pin"] = RELAY_PIN;
    doc["relay_active_low_runtime"] = relayActiveLowRuntime;
    doc["relay_level"] = (int)digitalRead(RELAY_PIN);
    doc["device_id"] = DEVICE_ID;
    serializeJson(doc, json);
  }
  server.send(200, "application/json", json);
}

/**
 * @brief Đặt lịch upload dữ liệu định kỳ (60 giây/lần) thông qua task upload.
 */
void tryBackendUpload() {
  // CẤM UPLOAD TRƯỚC KHI NETWORK TASK HOÀN TẤT
  if (!networkTaskCompleted) {
    return;  // Tất cả các request upload bị cấm cho đến khi network setup xong
  }

  static unsigned long lastUpload = 0;
  const unsigned long interval = 60000; // mỗi 60s
  unsigned long now = millis();
  if (now - lastUpload < interval) return;
  lastUpload = now;

  esp_task_wdt_reset(); // Reset watchdog before prepare
  JsonDocument doc;
  // Không gửi timestamp, để server tự tạo
  doc["temperature"] = temperature;
  doc["smoke_value"] = smokeValue;
  // MQ-135: không có preheat gating, bỏ trường cũ
  doc["fire_value"] = fireValue10; // KY-026 10-bit
  doc["temp_alert"] = tempAlertFlag;
  doc["smoke_alert"] = smokeAlertFlag;
  doc["fire_alert"] = fireAlertFlag;
  doc["device_id"] = DEVICE_ID;
  String body;
  serializeJson(doc, body);

  // USE ASYNC UPLOAD: Đặt flag để upload task xử lý
  // Cách này tránh block main loop
  if (xSemaphoreTake(uploadMutex, 100)) {
    uploadBody = body;
    xSemaphoreGive(uploadMutex);
  }
  uploadPending = true; // Upload task sẽ xử lý này
}

/**
 * @brief Đặt lịch upload ngay lập tức (không khẩn cấp) thông qua task upload.
 */
void uploadImmediate() {
  // USE ASYNC UPLOAD: Đặt flag để upload task xử lý ngay
  Serial.println("[UPLOAD] Bắt đầu upload immediate...");

  JsonDocument doc;
  // Không gửi timestamp, để server tự tạo
  doc["temperature"] = temperature;
  doc["smoke_value"] = smokeValue;
  // MQ-135: không có preheat gating, bỏ trường cũ
  doc["fire_value"] = fireValue10; // KY-026 10-bit
  doc["temp_alert"] = tempAlertFlag;
  doc["smoke_alert"] = smokeAlertFlag;
  doc["fire_alert"] = fireAlertFlag;
  doc["device_id"] = DEVICE_ID;
  String body;
  serializeJson(doc, body);

  // Protect uploadBody assignment with mutex
  if (xSemaphoreTake(uploadMutex, 100)) {
    uploadBody = body;
    xSemaphoreGive(uploadMutex);
  }
  uploadPending = true; // Bắt đầu upload (async via uploadTask)
}

/**
 * @brief Đặt lịch upload khẩn, yêu cầu task upload dùng đường truyền timeout ngắn.
 */
void uploadImmediateCritical() {
  // Gửi khẩn: dùng path timeout ngắn, không retry
  Serial.println("[UPLOAD][URGENT] Bắt đầu upload immediate (CRITICAL)...");
  JsonDocument doc;
  doc["temperature"] = temperature;
  doc["smoke_value"] = smokeValue;
  doc["fire_value"] = fireValue10; // KY-026 10-bit
  doc["temp_alert"] = tempAlertFlag;
  doc["smoke_alert"] = smokeAlertFlag;
  doc["fire_alert"] = fireAlertFlag;
  doc["device_id"] = DEVICE_ID;
  String body;
  serializeJson(doc, body);

  if (xSemaphoreTake(uploadMutex, 100)) {
    uploadBody = body;
    xSemaphoreGive(uploadMutex);
  }
  urgentUploadPending = true;
}

/**
 * @brief Quy trình kiểm tra nhanh các cảm biến và phần cứng cảnh báo (dành cho kỹ thuật viên).
 */
void testSensors() {
  Serial.println("Kiểm tra cảm biến nhiệt độ DS18B20...");
  tempSensor.begin();
  int deviceCount = tempSensor.getDeviceCount();
  Serial.print("Số lượng cảm biến DS18B20 tìm thấy: ");
  Serial.println(deviceCount);

  if (deviceCount == 0) {
    Serial.println("Không tìm thấy cảm biến DS18B20!");
    Serial.println("Kiểm tra:");
    Serial.println("- VCC kết nối với 3.3V hoặc 5V");
    Serial.println("- GND kết nối với GND");
    Serial.println("- Data kết nối với GPIO 4");
    Serial.println("- Có điện trở pull-up 4.7kΩ giữa Data và VCC");
  } else {
    tempSensor.requestTemperatures();
    float testTemp = tempSensor.getTempCByIndex(0);
    Serial.print("Nhiệt độ đọc được: ");
    Serial.print(testTemp);
    Serial.println(" °C");
  }

  Serial.println("Kiểm tra cảm biến lửa KY-026 (Analog)...");
  int fireTest = analogRead(FIRE_SENSOR_ANALOG_PIN);
  int fireTest10 = fireTest >> 2; // 12-bit → 10-bit
  Serial.print("Giá trị analog KY-026 (10-bit): ");
  Serial.println(fireTest10);
  Serial.println("(0-1023, so với FIRE_ANALOG_THRESHOLD)");

  Serial.println("Kiểm tra cảm biến khí MQ-135...");
  int smokeTest = analogRead(SMOKE_SENSOR_PIN);
  Serial.print("Giá trị analog MQ-135: ");
  Serial.println(smokeTest);
  Serial.println("(0-4095, giá trị cao = nồng độ MQ-135 cao)");

  Serial.println("Test LED và Buzzer...");
  if (LED_PIN >= 0) digitalWrite(LED_PIN, HIGH);
  buzzerOn();
  delay(500);
  if (LED_PIN >= 0) digitalWrite(LED_PIN, LOW);
  buzzerOff();
  Serial.println("LED và Buzzer hoạt động bình thường");

  Serial.println("=== KẾT THÚC TEST ===");
}

/**
 * @brief Bộ lọc median đơn giản để loại bỏ nhiễu trong chuỗi mẫu analog.
 */
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

/**
 * @brief Tính trung bình trượt cho MQ-135 để làm mượt giá trị đo cuối cùng.
 */
int movingAverage(int newValue) {
  smokeHistory[smokeHistoryIndex] = newValue;
  smokeHistoryIndex = (smokeHistoryIndex + 1) % MOVING_AVERAGE_SIZE;

  long sum = 0;
  for (int i = 0; i < MOVING_AVERAGE_SIZE; i++) {
    sum += smokeHistory[i];
  }
  return (int)(sum / MOVING_AVERAGE_SIZE);
}

/**
 * @brief (Không dùng) Giữ placeholder đồng bộ NTP – backend phụ trách timestamp.
 */
void syncNTP() {
  // NTP disabled - backend provides timestamp
  Serial.println("[NTP] Disabled - backend handles time");
}

/**
 * @brief Trả về timestamp hiện tại nếu dùng NTP (hiện trả 0 vì server tự chèn thời gian).
 */
unsigned long getCurrentTimestamp() {
  // Backend handles timestamps; return 0 to indicate unused
  return 0;
}

/**
 * @brief Buộc đồng bộ NTP (đã vô hiệu hóa vì backend quản lý thời gian).
 */
void forceSyncNTP() {
  // NTP disabled - backend provides timestamp
  Serial.println("[NTP] Disabled - backend handles time");
}

// WiFi Setup Functions
/**
 * @brief Endpoint `/wifi-scan` - quét mạng Wi-Fi xung quanh và hiển thị cho kỹ thuật viên.
 *
 * Có cơ chế debounce để tránh spam scan khi người dùng reload liên tục.
 */
void handleWiFiScan() {
  // Debounce and single-scan guard
  unsigned long now = millis();
  if (wifiScanInProgress && (now - wifiScanLastStartMs) < 5000) {
    // Return scanning overlay without starting another scan
    String html = "<!DOCTYPE html><html><head><meta charset='utf-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<meta http-equiv='refresh' content='2'>";
    html += "<title>WiFi Scan</title>";
    html += "<style>body{font-family:Arial;padding:0;margin:0} .overlay{position:fixed;inset:0;background:rgba(0,0,0,.6);color:#fff;display:flex;align-items:center;justify-content:center;text-align:center} .panel{max-width:90%}</style></head><body>";
    html += "<div class='overlay'><div class='panel'><h2>Đang quét WiFi...</h2><p>Vui lòng chờ, không bấm hay reload trang.</p></div></div></body></html>";
    server.send(200, "text/html", html);
    return;
  }
  wifiScanInProgress = true;
  wifiScanLastStartMs = now;
  Serial.println("Quét WiFi xung quanh...");

  WiFi.mode(WIFI_AP_STA);
  if (server.hasArg("rescan")) {
    WiFi.scanDelete();
    WiFi.scanNetworks(true); // async
  }
  int n = WiFi.scanComplete();
  if (n < 0) {
    // Chưa xong → bắt đầu hoặc chờ và hiển thị overlay chống spam
    WiFi.scanNetworks(true);
    String html = "<!DOCTYPE html><html><head><meta charset='utf-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<meta http-equiv='refresh' content='2'>"; // tự refresh để lấy kết quả
    html += "<title>WiFi Scan</title>";
    html += "<style>body{font-family:Arial;padding:0;margin:0} .overlay{position:fixed;inset:0;background:rgba(0,0,0,.6);color:#fff;display:flex;align-items:center;justify-content:center;text-align:center} .panel{max-width:90%}</style></head><body>";
    html += "<div class='overlay'><div class='panel'>";
    html += "<h2>Đang quét WiFi...</h2><p>Vui lòng chờ, không bấm hay reload trang.</p>";
    html += "<p><a href='/wifi-scan?rescan=1' style='color:#fff;text-decoration:underline'>Quét lại</a> · <a href='/' style='color:#fff;text-decoration:underline'>Trang chủ</a></p>";
    html += "</div></div></body></html>";
    server.send(200, "text/html", html);
    return;
  }

  String html = "<!DOCTYPE html><html><head><meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>WiFi Networks</title>";
  html += "<style>body{font-family:Arial;padding:20px;background:#f5f5f5}";
  html += ".container{max-width:600px;margin:0 auto;background:white;padding:30px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1)}";
  html += ".network{border:1px solid #ddd;border-radius:5px;padding:15px;margin:10px 0}";
  html += ".network strong{color:#2196f3}";
  html += ".signal{color:#4caf50}";
  html += ".button{background:#2196f3;color:white;padding:8px 16px;border:none;border-radius:3px;cursor:pointer;margin:5px;text-decoration:none;display:inline-block}";
  html += ".button:hover{background:#1976d2}</style></head><body>";

  html += "<div class='container'>";
  html += "<h1>WiFi Networks Found</h1>";
  html += "<p>Found " + String(n) + " networks:</p>";
  html += "<a href='/wifi-scan?rescan=1' class='button'>Rescan</a> ";
  html += "<a href='/firmware-update' class='button'>← Back to Firmware Update</a>";

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

  html += "</div></body></html>";

  server.send(200, "text/html", html);
  // Scan completed, clear guard
  wifiScanInProgress = false;
}

/**
 * @brief Endpoint `/wifi-connect` - nhận SSID/password và thử kết nối Wi-Fi STA.
 *
 * Kết quả thành công sẽ được lưu vào SPIFFS để các lần sau tự động dùng lại.
 */
void handleWiFiConnect() {
  if (server.hasArg("ssid") && server.hasArg("password")) {
    String ssid = server.arg("ssid");
    String password = server.arg("password");

    Serial.println("Đang kết nối WiFi: " + ssid);

    // Thử kết nối WiFi mới
    WiFi.mode(WIFI_AP_STA);
    // Không xóa cấu hình WiFi cũ để giữ các mạng đã lưu
    WiFi.persistent(true);
    WiFi.setAutoReconnect(true);
    WiFi.begin(ssid.c_str(), password.c_str());

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
      delay(500);
      Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("WiFi đã kết nối!");
      Serial.print("IP: "); Serial.println(WiFi.localIP());

      // Lưu cấu hình WiFi vào SPIFFS
      saveWiFiConfig(ssid, password);

      String html = "<!DOCTYPE html><html><head><meta charset='utf-8'>";
      html += "<title>Connected</title></head><body>";
      html += "<h2>WiFi Connected!</h2>";
      html += "<p>SSID: " + ssid + "</p>";
      html += "<p>IP: " + WiFi.localIP().toString() + "</p>";
      html += "<p>Quay lại trang Firmware Update...</p>";
      html += "<script>setTimeout(() => window.location.href='/firmware-update', 1000);</script>";
      html += "</body></html>";

      server.send(200, "text/html", html);
      // Không reset; giữ nguyên để user tiếp tục update
    } else {
      Serial.println("WiFi kết nối thất bại");

      String html = "<!DOCTYPE html><html><head><meta charset='utf-8'>";
      html += "<title>Connection Failed</title></head><body>";
      html += "<h2>WiFi Connection Failed</h2>";
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

/**
 * @brief Endpoint `/wifi-reset` - xóa cấu hình Wi-Fi đã lưu và khởi động lại thiết bị.
 */
void handleWiFiReset() {
  Serial.println("Reset WiFi config...");

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

/**
 * @brief Lưu thông tin Wi-Fi xuống SPIFFS dưới dạng JSON để khôi phục sau này.
 */
void saveWiFiConfig(String ssid, String password) {
  // Khởi tạo SPIFFS nếu chưa có
  if (!SPIFFS.begin(true)) {
    Serial.println("Lỗi khởi tạo SPIFFS");
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
    Serial.println("Đã lưu cấu hình WiFi: " + ssid);
  } else {
    Serial.println("Không thể lưu cấu hình WiFi");
  }
}

// WiFi saved-config helpers removed to enforce WiFi on-demand only

/**
 * @brief Endpoint `/api/firmware/update-wifi` - thực hiện OTA qua Wi-Fi khi firmware mới sẵn sàng.
 */
void handleFirmwareUpdateWiFi() {
  Serial.println("Bắt đầu cập nhật firmware qua WiFi...");

  if (!firmwareUpdateAvailable) {
    server.send(400, "application/json", "{\"error\":\"No firmware update available\"}");
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    server.send(400, "application/json", "{\"error\":\"WiFi not connected. Please use WiFi Setup first\"}");
    return;
  }

  // Tạo URL đầy đủ cho firmware
  String fullUrl = String("http://") + String(BACKEND_HOST) + ":" + String(BACKEND_PORT) + latestFirmwareUrl;

  Serial.println("Tải firmware từ: " + fullUrl);

  // Thực hiện OTA update
  bool success = performOTAUpdate(fullUrl, "WiFi");

  if (success) {
    server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Firmware update completed\"}");
  } else {
    server.send(500, "application/json", "{\"error\":\"Firmware update failed\"}");
  }
}

/**
 * @brief Endpoint `/api/firmware/update-4g` - đã vô hiệu hóa theo chính sách (trả về lỗi).
 */
void handleFirmwareUpdate4G() {
  // 4G OTA bị vô hiệu hóa theo yêu cầu: chỉ cho phép kiểm tra qua 4G, update qua WiFi
  server.send(400, "application/json", "{\"error\":\"4G OTA disabled. Please use WiFi.\"}");
}

/**
 * @brief Endpoint `/api/firmware/check` - kiểm tra trạng thái firmware (sync hoặc async).
 */
void handleFirmwareCheck() {
  Serial.println("Kiểm tra firmware update từ web interface...");

  // Hỗ trợ chế độ đồng bộ: trả về kết quả thực sau khi kiểm tra xong
  if (server.hasArg("sync") && server.arg("sync") == "1") {
    // Bỏ qua debounce và thực hiện kiểm tra trực tiếp
    checkFirmwareUpdate();
    extern volatile bool lastFirmwareCheckSuccess;
    extern String lastFirmwareCheckError;

    String json;
    {
      JsonDocument doc;
      doc["current_version"] = FIRMWARE_VERSION;
      doc["current_build"] = FIRMWARE_BUILD;
      doc["update_available"] = firmwareUpdateAvailable;
      doc["checking"] = false;
      doc["rate_limited"] = false;
      if (!lastFirmwareCheckSuccess && lastFirmwareCheckError.length()) {
        doc["error"] = lastFirmwareCheckError;
      }
      if (firmwareUpdateAvailable) {
        doc["latest_version"] = latestFirmwareDisplayVersion;
        doc["latest_url"] = latestFirmwareUrl;
      } else {
        doc["latest_version"] = FIRMWARE_VERSION;
        doc["latest_url"] = "";
      }
      doc["checked_via"] = (WiFi.status() == WL_CONNECTED) ? "wifi" : ((currentConnectionMode == CONNECTION_4G_FIRST) ? "4g" : "none");
      serializeJson(doc, json);
    }
    server.send(200, "application/json", json);
    return;
  }

  unsigned long nowMs = millis();
  bool rateLimited = (nowMs - lastFirmwareCheckRequestAt) < FIRMWARE_CHECK_MIN_INTERVAL_MS;

  if (firmwareCheckInProgress || rateLimited) {
    // Trả về trạng thái hiện tại, không tạo task mới
    String json;
    {
      JsonDocument doc;
      doc["current_version"] = FIRMWARE_VERSION;
      doc["current_build"] = FIRMWARE_BUILD;
      doc["update_available"] = firmwareUpdateAvailable;
      doc["checking"] = firmwareCheckInProgress || true;
      doc["rate_limited"] = rateLimited;
      if (rateLimited) {
        doc["message"] = "Đang kiểm tra hoặc chờ 30s trước khi kiểm tra lại";
      }
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
    return;
  }

  // Đánh dấu bắt đầu một lần kiểm tra mới (debounce)
  lastFirmwareCheckRequestAt = nowMs;
  firmwareCheckInProgress = true;

  // Tạo task riêng để kiểm tra firmware, không block web server
  xTaskCreatePinnedToCore([](void* param) {
    // Subscribe this task to watchdog
    esp_task_wdt_add(NULL);
    esp_task_wdt_reset();

    // Thực thi kiểm tra firmware
    checkFirmwareUpdate();
    Serial.println("[FIRMWARE_TASK] Kiểm tra firmware hoàn tất");

    // Hoàn tất: cập nhật cờ trạng thái
    firmwareCheckInProgress = false;
    lastFirmwareCheck = millis();

    // Unsubscribe and delete task
    esp_task_wdt_delete(NULL);
    vTaskDelete(NULL);
  }, "firmwareCheckAsync", 4096, NULL, 1, NULL, 1);

  // Phản hồi ngay lập tức để UI không bị treo
  String json;
  {
    JsonDocument doc;
    doc["current_version"] = FIRMWARE_VERSION;
    doc["current_build"] = FIRMWARE_BUILD;
    doc["update_available"] = firmwareUpdateAvailable;
    doc["checking"] = true;
    doc["rate_limited"] = false;
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