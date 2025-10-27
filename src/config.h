#ifndef CONFIG_H
#define CONFIG_H

// Cấu hình WiFi
#define WIFI_SSID "AnhNguyen"
#define WIFI_PASSWORD "AnhNguyennn"

// Cấu hình SoftAP chính (hoạt động liên tục để quản trị)
#define AP_SSID "BatteryMonitor-Admin"
#define AP_PASSWORD "admin123"

// Timeout kết nối WiFi STA (ms)
#define WIFI_CONNECT_TIMEOUT_MS 15000

// Cấu hình HTTP Server nội bộ
#define HTTP_SERVER_PORT 80

// Cấu hình Cellular (SIMCOM A7680C / SIM7600 tương thích TinyGSM)
#define CELL_APN "m3-world"           // thay theo SIM
#define CELL_USER "mms"                  // nếu cần
#define CELL_PASS "mms"                  // nếu cần

// Buộc LTE-only (không 2G/3G). 1: chỉ LTE, 0: tự động
#define CELL_FORCE_LTE_ONLY 0

// UART kết nối module (chọn Serial1 hoặc Serial2 phụ thuộc wiring)
#define CELL_UART Serial2
#define CELL_BAUD 115200
#define CELL_TX_PIN 26
#define CELL_RX_PIN 27

// PWRKEY chân điều khiển bật module nếu dùng
#define CELL_PWRKEY_PIN 4
#define CELL_PWRKEY_ACTIVE_MS 1500
// SIMCOM (SIM7600/A7680C) thường dùng PWRKEY active-LOW
#define CELL_PWRKEY_ACTIVE_LOW 1

// Backend server (dùng cho upload qua 4G)
#define ENABLE_CELLULAR_UPLOAD 1
#define BACKEND_HOST "cloud.anhnguyxn.io.vn"   // DNS only → HTTP
#define BACKEND_PORT 8000
#define BACKEND_PATH "/api/ingest"
#define APPLICATION_KEY "battery_monitor_2025_secure_key"  // API key để xác thực

// Cấu hình GPIO Pins
#define TEMP_SENSOR_PIN 23        // DS18B20
#define SMOKE_SENSOR_PIN 35      // MQ-135 (Analog) → ESP32 ADC1 GPIO36 (VP). Tránh ADC2 khi dùng WiFi
#define FIRE_SENSOR_PIN 2        // IR Fire Sensor (Digital)
// Một số module MH flame/IR có ngõ ra active-LOW/open-collector, cần kéo lên
#define FIRE_INPUT_PULLUP 1      // 1: dùng INPUT_PULLUP cho chân IR, 0: dùng INPUT
#define BUZZER_PIN 18            // Còi cảnh báo
#define LED_PIN 19               // LED cảnh báo

// Còi điều khiển qua relay (nếu đang dùng module RELAY đóng/ngắt nguồn cho buzzer)
#define BUZZER_DRIVEN_BY_RELAY 1   // 1: Buzzer điều khiển qua Relay, 0: điều khiển trực tiếp chân BUZZER_PIN
#define RELAY_PIN BUZZER_PIN       // Có thể đổi sang GPIO khác nếu relay nối chân khác
#define RELAY_ACTIVE_LOW 0         // 1: Relay active-LOW (phổ biến), 0: active-HIGH
// Chọn tiếp điểm relay đang dùng để cấp nguồn cho buzzer
// 0: dùng tiếp điểm NO (thường mở) → Buzzer OFF khi relay nhả; ON khi relay hút
// 1: dùng tiếp điểm NC (thường đóng) → Buzzer ON khi relay nhả; OFF khi relay hút
#define RELAY_CONTACT_NC 1

// Âm báo khi thiết bị hoàn tất setup mạng
#define STARTUP_CHIME_ENABLED 1     // 1: Bật âm báo khởi động sau khi setup mạng xong


// Ngưỡng cảnh báo (theo thứ tự ưu tiên)
#define TEMP_THRESHOLD 60.0      // Nhiệt độ nguy hiểm (°C) - ƯU TIÊN CAO NHẤT
#define SMOKE_THRESHOLD 300      // Ngưỡng khí MQ-135 (0-4095, 12-bit ADC) - ƯU TIÊN TRUNG BÌNH
#define FIRE_THRESHOLD LOW       // Ngưỡng cảm biến cháy IR - ƯU TIÊN THẤP NHẤT

// Ngưỡng phụ cho logic phức tạp
#define TEMP_SMOKE_THRESHOLD_MULTIPLIER 0.8  // 80% của TEMP_THRESHOLD cho MQ-135 + Temp

// Cấu hình độ nhạy cảnh báo (ALERT)
// 1 = cao (chỉ cần 1 module cảnh báo)
// 2 = trung (cần 2 module cảnh báo)
// 3 = nhẹ (cần cả 3 module cảnh báo)
#define ALERT_SENSITIVITY 2

// Cấu hình thời gian
#define SENSOR_INTERVAL 1000     // Đọc cảm biến mỗi 1 giây
#define DATA_SEND_INTERVAL 3000 // Gửi dữ liệu mỗi 30 giây

// Cấu hình thiết bị
#define DEVICE_ID "battery_monitor_001"
#define SERIAL_BAUD_RATE 115200

// Firmware version info
#define FIRMWARE_VERSION "1.0.0"
#define FIRMWARE_BUILD 3

// Firmware update settings
#define FIRMWARE_CHECK_INTERVAL 43200000  // 12 giờ (43200000ms)
#define FIRMWARE_UPDATE_TIMEOUT 300000    // 5 phút timeout
#define FIRMWARE_NOTIFICATION_AP_SSID "FirmwareUpdate-v" FIRMWARE_VERSION
#define FIRMWARE_NOTIFICATION_AP_PASSWORD "update123"

// Cấu hình ADC cho MQ-135
#define ADC_SAMPLES 16           // số mẫu đọc để lấy trung bình
#define SMOKE_FLOAT_RANGE 800    // nếu (max-min) > ngưỡng này coi như chân đang trôi (sensor chưa cắm)
#define MEDIAN_FILTER_SIZE 5     // kích thước median filter
#define MOVING_AVERAGE_SIZE 10   // kích thước moving average filter


// Cấu hình Fast Setup
#define FAST_SETUP_MODE 1           // 1: Bật fast setup, 0: Tắt (debug mode)
// #define DEBUG_SENSORS              // Uncomment để bật debug sensors

#endif
