/*
 * HƯỚNG DẪN SỬ DỤNG HỆ THỐNG GIÁM SÁT PIN
 * 
 * CẢM BIẾN SỬ DỤNG:
 * 1. DS18B20 - Cảm biến nhiệt độ
 * 2. MQ2 - Cảm biến khói và khí gas
 * 3. IR Fire Sensor - Cảm biến cháy hồng ngoại
 * 
 * KẾT NỐI PHẦN CỨNG:
 * 
 * DS18B20 (Nhiệt độ):
 * - VCC: 3.3V hoặc 5V
 * - GND: GND
 * - Data: GPIO 4 (có thể thay đổi)
 * 
 * MQ2 (Khói):
 * - VCC: 5V
 * - GND: GND  
 * - A0: GPIO A0 (ADC)
 * 
 * IR Fire Sensor (Cháy):
 * - VCC: 5V
 * - GND: GND
 * - OUT: GPIO 2 (có thể thay đổi)
 * 
 * BUZZER (Còi cảnh báo):
 * - Positive: GPIO 18
 * - Negative: GND
 * 
 * LED (Đèn cảnh báo):
 * - Anode: GPIO 19
 * - Cathode: GND (qua điện trở 220Ω)
 * 
 * CẤU HÌNH:
 * 1. Thay đổi WiFi credentials trong main.cpp:
 *    - ssid: Tên WiFi của bạn
 *    - password: Mật khẩu WiFi
 * 
 * 2. Tuỳ chọn SoftAP (khi không có WiFi):
 *    - AP_SSID: Tên mạng phát ra từ ESP32 (mặc định BatteryMonitor-AP)
 *    - AP_PASSWORD: Mật khẩu AP (mặc định 12345678)
 * 
 * 3. Điều chỉnh ngưỡng cảnh báo:
 *    - TEMP_THRESHOLD: Nhiệt độ nguy hiểm (mặc định 60°C)
 *    - SMOKE_THRESHOLD: Ngưỡng khói (mặc định 300)
 * 
 * CHỨC NĂNG:
 * - Đọc dữ liệu từ cảm biến mỗi 1 giây
 * - Tự chạy HTTP server nội bộ trên ESP32 (port 80)
 * - SoftAP fallback nếu không kết nối được WiFi STA
 * - Cảnh báo bằng LED và còi khi phát hiện nguy hiểm
 * - Hiển thị dữ liệu trên Serial Monitor
 * 
 * DỮ LIỆU GỬI LÊN SERVER:
 * {
 *   "timestamp": 1234567890,
 *   "temperature": 25.5,
 *   "smoke_value": 150,
 *   "fire_detected": false,
 *   "alert_active": false,
 *   "device_id": "battery_monitor_001"
 * }
 */
