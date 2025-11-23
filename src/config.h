#ifndef CONFIG_H
#define CONFIG_H

/**
 * @file config.h
 * @brief Tập trung mọi hằng số cấu hình cho firmware ESP32 (mạng, cảm biến, cảnh báo, OTA).
 *
 * Việc gom nhóm cấu hình vào một nơi giúp:
 * - Dễ kiểm soát thay đổi khi triển khai tại hiện trường.
 * - Tránh sửa trực tiếp trong mã nguồn nghiệp vụ.
 * - Ghi chú rõ ràng bằng tiếng Việt để đội vận hành hiểu nhanh.
 */

// --------------------------------------------------------------------
// CẤU HÌNH WI-FI VÀ SOFT AP QUẢN TRỊ
// --------------------------------------------------------------------
#define WIFI_SSID "AnhNguyen"
#define WIFI_PASSWORD "AnhNguyennn"

// AP chạy thường trực để kỹ thuật viên truy cập cấu hình khi ra hiện trường
#define AP_SSID "BatteryMonitor-Admin"
#define AP_PASSWORD "admin123"

// Thời gian chờ ESP32 kết nối vào STA (ms) trước khi từ bỏ
#define WIFI_CONNECT_TIMEOUT_MS 15000

// Cổng HTTP cho web UI quản trị nội bộ
#define HTTP_SERVER_PORT 80

// --------------------------------------------------------------------
// CẤU HÌNH KẾT NỐI CELLULAR (SIMCOM A7680C / SIM7600)
// --------------------------------------------------------------------
#define CELL_APN "m3-world"   // Tùy SIM - cần đổi tương ứng nhà mạng
#define CELL_USER "mms"       // Bỏ trống nếu nhà mạng không yêu cầu
#define CELL_PASS "mms"

// 1 = ép chạy LTE-only để ổn định; 0 = để modem tự chọn (khi sóng LTE yếu)
#define CELL_FORCE_LTE_ONLY 1

// UART phần cứng kết nối module 4G
#define CELL_UART Serial2
#define CELL_BAUD 115200
#define CELL_TX_PIN 26
#define CELL_RX_PIN 27

// PWRKEY điều khiển nguồn cho modem; cần giữ mức kích đủ lâu
#define CELL_PWRKEY_PIN 4
#define CELL_PWRKEY_ACTIVE_MS 1500
#define CELL_PWRKEY_ACTIVE_LOW 1  // SIMCOM thường là active-LOW

// --------------------------------------------------------------------
// CẤU HÌNH BACKEND (Upload qua Wi-Fi hoặc 4G)
// --------------------------------------------------------------------
#define ENABLE_CELLULAR_UPLOAD 1
#define BACKEND_HOST "cloud.anhnguyxn.io.vn"   // Backend FastAPI
#define BACKEND_PORT 8000
#define BACKEND_PATH "/api/ingest"
#define APPLICATION_KEY "battery_monitor_2025_secure_key"  // API key xác thực

// --------------------------------------------------------------------
// CHÂN CẢM BIẾN VÀ NGOẠI VI
// --------------------------------------------------------------------
#define TEMP_SENSOR_PIN 23         // DS18B20 (1-Wire)
#define SMOKE_SENSOR_PIN 35        // MQ-135 → ESP32 ADC1 (tránh ADC2 khi bật Wi-Fi)
#define FIRE_SENSOR_ANALOG_PIN 34  // KY-026 analog AO → ESP32 ADC1

// Nếu có dùng chân digital của KY-026 thì bật pull-up tương ứng (mặc định không dùng)
#define FIRE_INPUT_PULLUP 0

// LED cảnh báo: để -1 nếu không sử dụng đèn báo
#define LED_PIN -1

// Điều khiển còi thông qua relay trung gian để bảo vệ ESP32
#define RELAY_PIN 0
#define RELAY_ACTIVE_LOW 1  // 1 = relay kích ở mức LOW (thường gặp), 0 = kích HIGH

// Âm báo sau khi thiết bị hoàn tất khởi tạo mạng
#define STARTUP_CHIME_ENABLED 1

// --------------------------------------------------------------------
// NGƯỠNG CẢNH BÁO AN TOÀN PIN
// --------------------------------------------------------------------
#define TEMP_THRESHOLD 90.0          // °C: Quá nhiệt → ưu tiên cao nhất
#define SMOKE_THRESHOLD 1500         // Giá trị ADC MQ-135 (0-4095)
#define FIRE_ANALOG_THRESHOLD 375    // KY-026 ADC (0-1023): nhỏ hơn ngưỡng = có lửa

// Ngưỡng phụ: nếu nhiệt độ cao, hạ ngưỡng MQ-135 xuống 80% để phát hiện sớm
#define TEMP_SMOKE_THRESHOLD_MULTIPLIER 0.8

// --------------------------------------------------------------------
// TẦN SUẤT ĐỌC/GỬI DỮ LIỆU
// --------------------------------------------------------------------
#define SENSOR_INTERVAL 1000      // ms: đọc cảm biến mỗi 1 giây
#define DATA_SEND_INTERVAL 3000   // ms: tối thiểu 3 giây mới gửi server một lần

// --------------------------------------------------------------------
#define DEVICE_ID "battery_monitor_001"
#define SERIAL_BAUD_RATE 115200

// --------------------------------------------------------------------
// THÔNG SỐ PHIÊN BẢN FIRMWARE
// --------------------------------------------------------------------
#define FIRMWARE_VERSION "1.0.2"
#define FIRMWARE_BUILD 1

// Cấu hình OTA
#define FIRMWARE_CHECK_INTERVAL 43200000  // 12 giờ (ms)
#define FIRMWARE_UPDATE_TIMEOUT 300000    // 5 phút
#define FIRMWARE_NOTIFICATION_AP_SSID "FirmwareUpdate-v" FIRMWARE_VERSION
#define FIRMWARE_NOTIFICATION_AP_PASSWORD "update123"

// --------------------------------------------------------------------
// XỬ LÝ NHIỄU ADC (MQ-135)
// --------------------------------------------------------------------
#define ADC_SAMPLES 16           // Lấy trung bình 16 mẫu cho mỗi lần đọc
#define SMOKE_FLOAT_RANGE 800    // Nếu biên độ dao động lớn → cảnh báo cảm biến lỏng
#define MEDIAN_FILTER_SIZE 5     // Bộ lọc trung vị để bỏ outlier
#define MOVING_AVERAGE_SIZE 10   // Trung bình trượt để làm mượt giá trị cuối

// --------------------------------------------------------------------
// CHẾ ĐỘ KHỞI ĐỘNG NHANH
// --------------------------------------------------------------------
#define FAST_SETUP_MODE 1        // 1 = boot tối giản, 0 = bật thêm debug
// #define DEBUG_SENSORS         // Bật để in log cảm biến chi tiết

#endif
