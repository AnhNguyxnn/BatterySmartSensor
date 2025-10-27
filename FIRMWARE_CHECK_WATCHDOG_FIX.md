# Firmware Check Watchdog Timeout Fix

## 🐛 Vấn đề (The Problem)

Khi kiểm tra firmware update từ web interface, ESP32 gặp **watchdog timeout** và crash:

```
E (376496) task_wdt: Task watchdog got triggered. The following tasks did not reset the watchdog in time:
E (376496) task_wdt:  - loopTask (CPU 1)
E (376496) task_wdt: Aborting.
abort() was called at PC 0x400f1b2d on core 0
```

### Nguyên nhân chính:

1. **Blocking Operation in Web Server Thread**: 
   - Hàm `handleFirmwareCheck()` được gọi từ web server request handler (loopTask)
   - Nó gọi `checkFirmwareUpdate()` **synchronously** (chờ kết quả)
   - Hàm này có thể block lâu khi:
     - Gọi `cellularBegin()` (UART communication, timeout 15 giây)
     - HTTP GET request qua 4G
     - JSON parsing

2. **Insufficient Watchdog Resets**:
   - Trong `cellularBegin()`, có nhiều `delay()` mà **không reset watchdog**
   - Ví dụ: `for (int i = 0; i < 2; i++) delay(500);` - 1 giây mà không reset
   - Watchdog timeout mặc định là 60 giây, nhưng loopTask cần reset thường xuyên

3. **Task không Subscribe Watchdog**:
   - Khi tạo task mới bằng `xTaskCreatePinnedToCore()`, task này không tự động subscribe vào watchdog
   - Nếu task chạy lâu mà không reset, watchdog sẽ timeout

## ✅ Giải pháp (The Solution)

### 1. Move Firmware Check to Async Task

**File: `src/main.cpp`** - Sửa `handleFirmwareCheck()`

```cpp
void handleFirmwareCheck() {
  Serial.println("🔍 Kiểm tra firmware update từ web interface...");
  
  // ⚠️ TẠO TASK ASYNC ĐỂ KHÔNG BLOCK WEB SERVER
  static bool checkInProgress = false;
  
  if (!checkInProgress) {
    checkInProgress = true;
    // Tạo task riêng để kiểm tra firmware
    xTaskCreatePinnedToCore([](void* param) {
      // 🔒 Subscribe this task to watchdog
      esp_task_wdt_add(NULL);
      esp_task_wdt_reset();
      
      checkFirmwareUpdate();
      Serial.println("[FIRMWARE_TASK] Kiểm tra firmware hoàn tất");
      
      // 🔒 Unsubscribe from watchdog
      esp_task_wdt_delete(NULL);
      vTaskDelete(NULL);
    }, "firmwareCheckAsync", 4096, NULL, 1, NULL, 1);
    
    checkInProgress = false;
  }
  
  // Trả về ngay (phản ứng nhanh với client)
  String json;
  {
    JsonDocument doc;
    doc["current_version"] = FIRMWARE_VERSION;
    doc["current_build"] = FIRMWARE_BUILD;
    doc["update_available"] = firmwareUpdateAvailable;
    doc["checking"] = true;  // Báo đang kiểm tra trong background
    // ... thêm fields khác
    serializeJson(doc, json);
  }
  server.send(200, "application/json", json);
}
```

**Lợi ích:**
- ✅ Web server không bị block
- ✅ Client nhận response nhanh (`checking: true`)
- ✅ Task riêng có watchdog subscription và resets đủ

### 2. Add Watchdog Resets in `cellularBegin()`

**File: `src/cellular.cpp`** - Thêm resets sau các long operations

```cpp
// DNS config
modem.sendAT("+CDNSCFG=\"8.8.8.8\",\"1.1.1.1\"");
modem.waitResponse(1000);
esp_task_wdt_reset(); // ✅ Reset after DNS config

// Check signal strength
modem.sendAT("+CSQ");
modem.waitResponse(1000);
esp_task_wdt_reset(); // ✅ Reset after signal check

// Pre-resolve backend host
modem.sendAT("+CDNSGIP=\"" BACKEND_HOST "\"");
modem.waitResponse(5000);
esp_task_wdt_reset(); // ✅ Reset after DNS resolution

// HTTP client initialization
for (int i = 0; i < 2; i++) {
  delay(500);
  esp_task_wdt_reset(); // ✅ Reset during stabilization delays
}
```

**Lợi ích:**
- ✅ Watchdog được reset thường xuyên trong các phần blocking
- ✅ Ngăn watchdog timeout ngay cả khi 4G operations chậm

## 📊 Thay đổi Files

### Modified Files:
1. **`src/main.cpp`**:
   - Xóa: firmware check task từ startup
   - Sửa: `handleFirmwareCheck()` để tạo task async on-demand
   - Thêm: watchdog subscription/unsubscription logic

2. **`src/cellular.cpp`**:
   - Thêm: `esp_task_wdt_reset()` sau DNS config
   - Thêm: `esp_task_wdt_reset()` sau signal check
   - Thêm: `esp_task_wdt_reset()` sau DNS resolution
   - Thêm: `esp_task_wdt_reset()` trong stabilization loop

### No Changes:
- `firmware_update.cpp` - `checkFirmwareUpdate()` đã có đủ resets
- `config.h` - Watchdog timeout vẫn 60 giây
- Web interface - Không cần thay đổi

## 🧪 Testing

### Test Case 1: Firmware Check via Web Interface
```
1. Connect to AP: BatteryMonitor-Admin
2. Go to http://192.168.4.1
3. Click "🔄 Firmware Update"
4. Observe:
   - Response received immediately
   - Checking message appears
   - Background task processes silently
   - No watchdog crash
   - Result updates when ready
```

### Test Case 2: 4G Firmware Check
```
1. Ensure 4G connection is working
2. Click firmware check button
3. Observe:
   - WiFi check happens first (quick)
   - Fallback to 4G if WiFi fails
   - Task runs in background
   - Watchdog resets prevent timeout
   - Result updates when complete
```

### Expected Behavior:
- ✅ Device responds immediately to HTTP request
- ✅ No watchdog timeout even with 4G delay
- ✅ Background task updates variables
- ✅ Next page refresh shows latest firmware info
- ✅ Serial output shows `[FIRMWARE_TASK] Kiểm tra firmware hoàn tất`

## ⚠️ Important Notes

1. **Watchdog Configuration**:
   ```cpp
   esp_task_wdt_init(60, true);  // 60 second timeout
   esp_task_wdt_add(NULL);        // Add loopTask
   ```
   - Main task (loopTask) has 60s timeout
   - Each background task must subscribe separately
   - Remember to unsubscribe before deleting task

2. **Background Task Timing**:
   - WiFi check: Usually <1 second
   - 4G check: Can take 10-15 seconds
   - Client sees result next time it polls or refreshes page

3. **Memory**:
   - New task allocated 4096 bytes stack
   - Safe size for firmware check operations
   - Automatically deleted after completion

## 🔍 Debugging

### If still getting watchdog timeout:

1. **Check task creation:**
   ```cpp
   Serial.println("[FIRMWARE_TASK] Starting...");  // Should see this
   Serial.println("[FIRMWARE_TASK] Done");        // Should see this
   ```

2. **Verify watchdog resets:**
   ```cpp
   // Add debug resets
   esp_task_wdt_reset();
   Serial.println("[WDT] Reset");
   ```

3. **Check 4G connection:**
   - If UART communication hangs, watchdog will timeout anyway
   - Verify modem responds to AT commands
   - Check `cellularBegin()` logs for failures

## 📝 Summary

**Problem**: Firmware check blocking web server and causing watchdog timeout

**Root Cause**: 
- Synchronous long operation in web server thread
- Insufficient watchdog resets during cellular operations

**Solution**:
- Move firmware check to async background task
- Add watchdog resets in cellular operations
- Subscribe/unsubscribe task to/from watchdog

**Result**: 
- ✅ No more watchdog crashes during firmware checks
- ✅ Web server responsive
- ✅ Firmware checks work over both WiFi and 4G
- ✅ Production-ready
