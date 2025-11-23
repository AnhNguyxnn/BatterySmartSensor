# Battery Smart Sensor - Những Điểm Quan Trọng Cần Chú Ý

Tài liệu này tập trung vào các điểm kỹ thuật quan trọng cần lưu ý khi phát triển và bảo trì hệ thống.

## Mục Lục

- [Thread Safety và Concurrency](#thread-safety-và-concurrency)
- [Watchdog Timer](#watchdog-timer)
- [Memory Management](#memory-management)
- [Network Handling](#network-handling)
- [Error Handling](#error-handling)
- [Race Conditions](#race-conditions)
- [Timing và Delays](#timing-và-delays)
- [Backend Critical Points](#backend-critical-points)

---

## Thread Safety và Concurrency

### Upload Task Mutex

**Vị trí:** `src/main.cpp:95, 200-204, 256-272`

**Vấn đề:** Upload task chạy song song với main loop, cần bảo vệ shared data.

**Giải pháp:**
- Tạo mutex trong `setup()` TRƯỚC khi tạo tasks (dòng 316)
- Luôn dùng `xSemaphoreTake()` và `xSemaphoreGive()` khi truy cập `uploadBody`
- Copy dữ liệu ra biến local trước khi xử lý (dòng 198-204)

**Lưu ý:**
```cpp
// SAI - Không bảo vệ
uploadBody = newData;
uploadPending = true;

// ĐÚNG - Có mutex
if (xSemaphoreTake(uploadMutex, portMAX_DELAY)) {
  uploadBody = newData;
  xSemaphoreGive(uploadMutex);
}
uploadPending = true; // Flag ngoài mutex là OK
```

### Urgent vs Normal Upload

**Vị trí:** `src/main.cpp:180-272`

**Vấn đề:** Cần ưu tiên cảnh báo khẩn cấp hơn upload định kỳ.

**Giải pháp:**
- Kiểm tra `urgentUploadPending` TRƯỚC `uploadPending` (dòng 181-182)
- Chỉ reset flag tương ứng sau khi gửi thành công (dòng 256-271)
- Kiểm tra lại trạng thái trước khi reset để tránh mất dữ liệu mới (dòng 257-270)

**Lưu ý:** Nếu có urgent mới xuất hiện trong lúc upload normal, giữ nguyên `urgentUploadPending` để xử lý ngay ở lần lặp tiếp theo.

### Cellular HTTP Mutex

**Vị trí:** `src/cellular.cpp:22, 346, 459, 580`

**Vấn đề:** Nhiều task có thể gọi cellular HTTP cùng lúc.

**Giải pháp:**
- Dùng `cellularHttpMutex` để serialize mọi HTTP request qua 4G
- Luôn `xSemaphoreTake()` trước khi gọi HTTP, `xSemaphoreGive()` sau khi xong
- Đảm bảo release mutex trong mọi code path (kể cả lỗi)

---

## Watchdog Timer

### Main Loop Reset

**Vị trí:** `src/main.cpp:436, 452, 457, 475`

**Vấn đề:** Watchdog timeout 60s, nếu không reset sẽ reboot.

**Giải pháp:**
- Reset watchdog ở đầu mỗi `loop()` iteration (dòng 436)
- Reset sau mỗi operation có thể mất thời gian:
  - Sau đọc cảm biến (dòng 452)
  - Sau xử lý HTTP (dòng 457)
  - Sau upload attempt (dòng 475)

### Task Watchdog

**Vị trí:** `src/main.cpp:175, 376, 427`

**Vấn đề:** Một số task cần disable watchdog vì chạy HTTP operations dài.

**Giải pháp:**
- `uploadTask`: Disable watchdog vì HTTP có thể mất > 60s (dòng 175)
- `networkTask`: Disable watchdog, chỉ chạy 1 lần khi startup (dòng 376)
- `firmwareCheckTask`: Subscribe/unsubscribe watchdog đúng cách (dòng 1534, 1546)

**Lưu ý:** Task nào disable watchdog phải tự quản lý timeout, không để treo vô hạn.

### Cellular Operations

**Vị trí:** `src/cellular.cpp:281, 287, 293, 299, 364, 370, 385, 392, 395, 398, 401`

**Vấn đề:** Cellular operations (NETOPEN, DNS, HTTP) có thể mất thời gian dài.

**Giải pháp:**
- Reset watchdog sau mỗi AT command quan trọng
- Reset trong vòng lặp retry (dòng 425-427, 517-519, 648-650)
- Reset sau mỗi HTTP operation (dòng 364, 370, 385, 392, 395, 398, 401)

---

## Memory Management

### String Operations

**Vị trí:** Toàn bộ codebase

**Vấn đề:** ESP32 có RAM hạn chế, String operations dễ gây fragmentation.

**Giải pháp:**
- Tránh tạo String lớn trong loop
- Dùng `String.reserve()` nếu biết trước kích thước
- Ưu tiên dùng `const char*` cho string literals

**Lưu ý:** `renderHtml()` tạo String lớn, chỉ gọi khi cần thiết (khi có HTTP request).

### JSON Buffer

**Vị trí:** `src/main.cpp:1026-1037, 1055-1066, 1082-1091`

**Vấn đề:** ArduinoJson cần buffer đủ lớn.

**Giải pháp:**
- Dùng `JsonDocument` với size phù hợp
- Kiểm tra `deserializeJson()` return value
- Tránh serialize/deserialize trong loop nếu không cần

### Stack Size

**Vị trí:** `src/main.cpp:359, 362`

**Vấn đề:** Tasks cần stack đủ lớn cho HTTP operations.

**Giải pháp:**
- `networkTask`: 8192 bytes (dòng 359)
- `uploadTask`: 8192 bytes (dòng 362)
- Monitor free heap: `ESP.getFreeHeap()` trong setup()

---

## Network Handling

### Network Task Completion Flag

**Vị trí:** `src/main.cpp:85, 185-189, 417, 1014-1017`

**Vấn đề:** Không được upload trước khi network setup xong.

**Giải pháp:**
- Flag `networkTaskCompleted` chỉ set TRUE sau khi network task hoàn tất (dòng 417)
- Upload task kiểm tra flag trước khi gửi (dòng 185-189)
- `tryBackendUpload()` cũng kiểm tra flag (dòng 1014-1017)

**Lưu ý:** Đây là critical guard để tránh gửi dữ liệu khi chưa có kết nối.

### Connection Mode

**Vị trí:** `src/main.cpp:78-83, 210-252`

**Vấn đề:** Cần biết đang dùng WiFi hay 4G để gửi đúng đường.

**Giải pháp:**
- Enum `ConnectionMode` để track trạng thái (dòng 78-83)
- Upload task kiểm tra `currentConnectionMode` để chọn đường gửi (dòng 210-252)
- Không fallback tự động giữa WiFi và 4G trong upload task

### Cellular Connection State

**Vị trí:** `src/cellular.cpp:20-21, 316-332, 348-356`

**Vấn đề:** Cần kiểm tra và reconnect nếu mất kết nối.

**Giải pháp:**
- `ensureCellularConnection()` kiểm tra signal và data connection (dòng 316-332)
- Tự động reconnect nếu phát hiện mất kết nối (dòng 348-356)
- Retry với exponential backoff (dòng 373-443)

**Lưu ý:** Không gọi `cellularBegin()` trong interrupt handler hoặc critical section.

### WiFi Scan Debounce

**Vị trí:** `src/main.cpp:87-89, 1211-1224`

**Vấn đề:** WiFi scan tốn thời gian, không được spam.

**Giải pháp:**
- Flag `wifiScanInProgress` và timestamp `wifiScanLastStartMs` (dòng 87-89)
- Kiểm tra debounce trước khi scan (dòng 1214-1224)
- Return overlay nếu đang scan (dòng 1216-1223)

---

## Error Handling

### HTTP Retry Logic

**Vị trí:** `src/cellular.cpp:373-443, 607-664`

**Vấn đề:** HTTP có thể fail do mạng không ổn định.

**Giải pháp:**
- Retry tối đa 3 lần với exponential backoff (dòng 373-443)
- Reconnect nếu status code là -1, -2, -3, hoặc 400 (dòng 412-442)
- Đóng NETOPEN trước khi reconnect (dòng 417-418)

**Lưu ý:** Không retry mãi, sau 3 lần thì bỏ qua và đợi lần upload tiếp theo.

### Sensor Read Errors

**Vị trí:** `src/main.cpp:528-577`

**Vấn đề:** Cảm biến có thể disconnect hoặc trả về giá trị sai.

**Giải pháp:**
- DS18B20: Kiểm tra `DEVICE_DISCONNECTED_C` (đã comment, dòng 533-538)
- MQ-135: Dùng median filter + moving average để loại nhiễu (dòng 541-562)
- KY-026: Dùng median filter (dòng 568-576)

**Lưu ý:** Không crash nếu cảm biến lỗi, chỉ log và tiếp tục.

### Firmware Update Errors

**Vị trí:** `src/firmware_update.cpp:212-283`

**Vấn đề:** OTA update có thể fail do mạng, flash, hoặc file.

**Giải pháp:**
- Kiểm tra HTTP status code (dòng 222-225)
- Kiểm tra content length (dòng 227-231)
- Kiểm tra `Update.begin()` và `Update.write()` (dòng 233-252)
- Reset watchdog trong quá trình transfer (dòng 257-268)

**Lưu ý:** Nếu update fail, thiết bị vẫn chạy firmware cũ, không bị brick.

---

## Race Conditions

### Upload Flag Reset

**Vị trí:** `src/main.cpp:256-272`

**Vấn đề:** Có thể có urgent mới xuất hiện trong lúc upload normal.

**Giải pháp:**
- Kiểm tra lại `urgentUploadPending` trước khi reset `uploadPending` (dòng 262)
- Chỉ reset flag tương ứng với request vừa gửi (dòng 257-271)
- Giữ nguyên `urgentUploadPending` nếu có urgent mới (dòng 268-270)

### Firmware Check Concurrency

**Vị trí:** `src/main.cpp:72-75, 1498-1524`

**Vấn đề:** Nhiều request có thể trigger firmware check cùng lúc.

**Giải pháp:**
- Flag `firmwareCheckInProgress` để block concurrent checks (dòng 73)
- Rate limiting: 30 giây giữa các lần check (dòng 75, 1499)
- Tạo task riêng để check, không block web server (dòng 1532-1548)

### Network Task và Upload Task

**Vị trí:** `src/main.cpp:85, 185-189`

**Vấn đề:** Upload task có thể chạy trước khi network task xong.

**Giải pháp:**
- Flag `networkTaskCompleted` được set bởi network task (dòng 417)
- Upload task kiểm tra flag trước khi upload (dòng 185-189)
- Delay 500ms nếu chưa sẵn sàng (dòng 187-188)

---

## Timing và Delays

### Sensor Read Interval

**Vị trí:** `src/main.cpp:448-453, config.h:93`

**Vấn đề:** Đọc cảm biến quá nhanh gây lãng phí CPU, quá chậm mất dữ liệu.

**Giải pháp:**
- Interval 1 giây (1000ms) - đủ để phát hiện thay đổi nhanh
- Dùng `millis()` để track, không dùng `delay()` trong loop

### Upload Interval

**Vị trí:** `src/main.cpp:1019-1023, config.h:94`

**Vấn đề:** Upload quá thường xuyên tốn băng thông và pin.

**Giải pháp:**
- Interval 60 giây cho upload định kỳ
- Upload ngay lập tức khi có cảnh báo (urgent)

### Cellular Stabilization Delay

**Vị trí:** `src/main.cpp:384-388, cellular.cpp:296-300`

**Vấn đề:** Modem cần thời gian ổn định sau NETOPEN.

**Giải pháp:**
- Delay 3 giây sau NETOPEN trong network task (dòng 384-388)
- Delay 1.5 giây sau DNS resolution (dòng 296-300)

**Lưu ý:** Không giảm delay này, modem cần thời gian để sẵn sàng.

### Watchdog Reset Frequency

**Vị trí:** Toàn bộ codebase

**Vấn đề:** Reset quá ít → timeout, reset quá nhiều → lãng phí CPU.

**Giải pháp:**
- Reset ở đầu loop() (dòng 436)
- Reset sau mỗi operation có thể mất thời gian
- Reset trong vòng lặp retry/delay

---

## Backend Critical Points

### Timestamp Generation

**Vị trí:** `battery_backend/backend/app/main.py:368-374`

**Vấn đề:** ESP32 có thể có clock sai, cần dùng server timestamp.

**Giải pháp:**
- Server tự tạo timestamp khi nhận request (dòng 369)
- Không dùng timestamp từ ESP32 (dòng 374)
- Đảm bảo server có NTP sync

**Lưu ý:** ESP32 không sync NTP (đã disable, dòng 459), hoàn toàn dựa vào server.

### Database Migration

**Vị trí:** `battery_backend/backend/app/main.py:32-64`

**Vấn đề:** SQLite không hỗ trợ ALTER TABLE tốt như PostgreSQL.

**Giải pháp:**
- Tự động migration khi startup (dòng 64)
- Kiểm tra schema và thêm cột mới nếu thiếu (dòng 46-59)
- Chỉ chạy với SQLite (dòng 43)

**Lưu ý:** Với PostgreSQL/MySQL, dùng Alembic thay vì migration tự động.

### Telegram Background Task

**Vị trí:** `battery_backend/backend/app/main.py:386-392`

**Vấn đề:** Gửi Telegram có thể chậm, không được block request.

**Giải pháp:**
- Dùng FastAPI `BackgroundTasks` (dòng 390)
- Gửi async, không chờ response
- Bỏ qua nếu không cấu hình Telegram (dòng 207-209)

### API Key Authentication

**Vị trí:** `battery_backend/backend/app/main.py:158-176`

**Vấn đề:** API key phải match giữa ESP32 và backend.

**Giải pháp:**
- Dependency injection `verify_api_key()` (dòng 158-176)
- So sánh với `API_KEY` từ environment variable
- Trả về 401 nếu không match

**Lưu ý:** Đổi API key mặc định trong production (dòng 153).

### CORS Configuration

**Vị trí:** `battery_backend/backend/app/main.py:72-78`

**Vấn đề:** CORS quá mở có thể gây lỗ hổng bảo mật.

**Giải pháp:**
- Hiện tại cho phép mọi origin (`allow_origins=["*"]`)
- Production nên giới hạn origin cụ thể

---

## Các Điểm Quan Trọng Khác

### GPIO Initialization Order

**Vị trí:** `src/main.cpp:337-341`

**Vấn đề:** Set OUTPUT trước khi set mức có thể gây glitch.

**Giải pháp:**
- Set mức an toàn TRƯỚC (dòng 339)
- Sau đó mới `pinMode(OUTPUT)` (dòng 340)

### Relay Active Low/High

**Vị trí:** `src/main.cpp:49, 151-162, config.h:75`

**Vấn đề:** Relay có thể active-low hoặc active-high tùy module.

**Giải pháp:**
- Config `RELAY_ACTIVE_LOW` trong `config.h`
- Runtime variable `relayActiveLowRuntime` để đổi mode (dòng 49)
- Logic đảo ngược trong `buzzerOn()` và `buzzerOff()` (dòng 151-162)

### Alert Repeat Interval

**Vị trí:** `src/main.cpp:587-588, 636-640`

**Vấn đề:** Cảnh báo cần gửi lại định kỳ để đảm bảo backend nhận được.

**Giải pháp:**
- Gửi lại mỗi 30 giây khi cảnh báo vẫn active (dòng 636-640)
- Reset timer khi hết cảnh báo (dòng 654)

### Firmware Check Rate Limiting

**Vị trí:** `src/main.cpp:74-75, 1498-1524`

**Vấn đề:** Firmware check tốn băng thông, không được spam.

**Giải pháp:**
- Minimum interval 30 giây (dòng 75)
- Flag `firmwareCheckInProgress` để block concurrent (dòng 73)
- Return status hiện tại nếu đang check hoặc rate limited (dòng 1501-1524)

### 4G OTA Disabled

**Vị trí:** `src/main.cpp:1454-1458, firmware_update.cpp:215`

**Vấn đề:** 4G OTA bị vô hiệu hóa theo yêu cầu.

**Giải pháp:**
- Endpoint `/api/firmware/update-4g` trả về lỗi (dòng 1457)
- `performOTAUpdate()` chỉ dùng WiFi (dòng 215)

**Lưu ý:** Chỉ cho phép kiểm tra firmware qua 4G, không cho phép update.

---

## Checklist Khi Sửa Code

- [ ] Kiểm tra thread safety: Có dùng mutex khi cần?
- [ ] Kiểm tra watchdog: Có reset đủ thường xuyên?
- [ ] Kiểm tra memory: Có leak hoặc fragmentation?
- [ ] Kiểm tra error handling: Có handle mọi error path?
- [ ] Kiểm tra race condition: Có flag/guard đúng?
- [ ] Kiểm tra timing: Delay có đủ không?
- [ ] Test với mạng chậm/không ổn định
- [ ] Test với cảm biến disconnect
- [ ] Test với power cycle đột ngột

---

## Tài Liệu Tham Khảo

- ESP32 FreeRTOS: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/freertos.html
- ArduinoJson: https://arduinojson.org/
- TinyGSM: https://github.com/vshymanskyy/TinyGSM
- FastAPI: https://fastapi.tiangolo.com/
- SQLAlchemy: https://docs.sqlalchemy.org/
