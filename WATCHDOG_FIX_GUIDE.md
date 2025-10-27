# Watchdog Timeout Fix Guide

## Problem Summary

Your ESP32 device was experiencing a **Task Watchdog Timer (WDT) crash** approximately 125 seconds after boot. The error message was:

```
E (125183) task_wdt: Task watchdog got triggered. The following tasks did not reset the watchdog in time:
E (125183) task_wdt:  - networkTask (CPU 1)
```

## Root Cause

The `networkTask` was performing long blocking operations without frequently resetting the watchdog timer:

1. **Long cellular operations**: `cellularBegin()` could take 15+ seconds during modem initialization
2. **HTTP requests**: Cellular HTTP operations and AT commands could take 5-30+ seconds
3. **NTP sync**: Time synchronization could block for extended periods
4. **Insufficient watchdog resets**: The main `networkTask` only reset the watchdog at the beginning and end, not during long sub-operations

## Solution Applied

Added **frequent `esp_task_wdt_reset()` calls** throughout all long-running operations to prevent the watchdog from timing out:

### 1. In `networkTask()` (main.cpp):
```cpp
if (connectionEstablished) {
    Serial.println("🌐 Bắt đầu đồng bộ NTP & gửi dữ liệu khởi động...");
    esp_task_wdt_reset();      // Before NTP sync
    forceSyncNTP();
    esp_task_wdt_reset();      // After NTP sync
    readSensors();
    esp_task_wdt_reset();      // After sensor read
    uploadImmediate();
    esp_task_wdt_reset();      // After upload
    Serial.println("🔍 Kiểm tra firmware update...");
    checkFirmwareUpdate();
    esp_task_wdt_reset();      // After firmware check
}
```

### 2. In `cellularBegin()` - Modem Initialization (cellular.cpp):
- Added watchdog reset after LTE mode setup
- Added watchdog reset after checking RAT (Radio Access Technology)

### 3. In `cellularBegin()` - GPRS Connection (cellular.cpp):
- Reset after successful GPRS connection
- Reset after diagnostic AT commands (+CPIN, +CREG, +CGREG, +CEREG)
- Reset after CEER log retrieval

### 4. In `cellularBegin()` - Network Operations (cellular.cpp):
- Reset after NETCLOSE command
- Reset during NETOPEN wait loop
- Reset after successful NETOPEN
- Reset before returning failure
- Reset after DNS configuration
- Reset after NTP attempt

### 5. In `cellularHttpPost()` (cellular.cpp):
- Reset before HTTP request setup
- Reset after DNS resolution
- Reset before sending body
- Reset after request sent
- Reset during error handling
- Reset after successful response

## Build and Flash Instructions

### Step 1: Compile the Fixed Firmware

```bash
cd /mnt/sdb1/dev/Hoc/IOT/BatterySmartSensor
/home/anhnguyen/.platformio/penv/bin/pio run -e esp32dev
```

Expected output:
```
========================= [SUCCESS] Took ~30 seconds =========================
```

### Step 2: Stop Serial Monitor (if running)

First, close the serial monitor if it's running:
```bash
# In the terminal running the serial monitor, press Ctrl+C
```

### Step 3: Upload Firmware to ESP32

```bash
/home/anhnguyen/.platformio/penv/bin/pio run -e esp32dev --target upload
```

The upload will start and show progress. When complete, you should see:
```
========================= [SUCCESS] Took ~X seconds =========================
```

### Step 4: Verify the Fix

Once upload is complete, start the serial monitor to verify:

```bash
/home/anhnguyen/.platformio/penv/bin/pio device monitor -e esp32dev
```

Watch for the device to boot and stabilize. The watchdog timeout should **NOT occur** after ~125 seconds anymore.

## Testing the Fix

1. **First boot (Fast Boot Path)**: Device should complete in <5 seconds
2. **Network initialization**: Should complete without watchdog timeout
3. **Stable operation**: Device should run indefinitely without crashes
4. **Monitor output**: You should NOT see the watchdog error anymore

Expected output after boot:
```
🚀 ESP32 Battery Monitor - Fast Boot Starting...
⚡ Fast Boot Path - khởi tạo tối thiểu...
✅ SPIFFS đã khởi tạo
📡 AP Quản trị đã khởi động
HTTP server lắng nghe tại port 80
⚡ Fast Boot Path done (<5s) - Web interface ready!
🌐 Network initialization running in background...
🌐 Bắt đầu khởi tạo mạng nền...
[CELL] Khởi tạo modem...
[CELL] Modem restart thành công
✅ 4G connected (background)
🌐 Bắt đầu đồng bộ NTP & gửi dữ liệu khởi động...
[NTP] 🔄 Force sync NTP...
... (Normal operation without watchdog crash)
```

## Key Changes Summary

| Component | Changes | Purpose |
|-----------|---------|---------|
| `networkTask()` | 5 new watchdog resets | Prevent timeout during initialization sequence |
| `cellularBegin()` | 8 new watchdog resets | Prevent timeout during modem setup and network operations |
| `cellularHttpPost()` | 5 new watchdog resets | Prevent timeout during HTTP communication |
| `firmware_update.cpp` | Already had resets | No changes needed |
| `loop()` | No changes | Already resets watchdog frequently (every 100ms) |

## Why This Fix Works

- **Watchdog timeout threshold**: 60 seconds (configured in setup())
- **Previous behavior**: Long operations in networkTask could exceed 60 seconds without reset → crash
- **New behavior**: Watchdog is reset frequently (every few seconds) during operations → no timeout

The watchdog can now monitor the main loop effectively while background tasks reset it appropriately during their long operations.

## If You Still Experience Issues

If the watchdog timeout persists after flashing:

1. **Check device connectivity**:
   - Ensure 4G module has good signal
   - Verify WiFi credentials in config
   - Check internet connectivity

2. **Increase watchdog timeout** (if needed):
   ```cpp
   // In main.cpp, setup() function, line 114:
   esp_task_wdt_init(60, true);  // Change 60 to 120 for 120 seconds
   ```

3. **Enable debug output**:
   ```cpp
   // In config.h:
   #define DEBUG_SENSORS  // Uncomment to enable detailed debug logging
   ```

4. **Monitor device memory**:
   ```
   Free Heap: 295912 bytes  // Should be > 200KB minimum
   ```

## Related Files Modified

- `/mnt/sdb1/dev/Hoc/IOT/BatterySmartSensor/src/main.cpp` - Lines 161-209 (networkTask)
- `/mnt/sdb1/dev/Hoc/IOT/BatterySmartSensor/src/cellular.cpp` - Lines 177-284 (cellularBegin and cellularHttpPost)
