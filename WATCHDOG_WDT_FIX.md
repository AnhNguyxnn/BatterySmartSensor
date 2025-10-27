# Watchdog Timer (WDT) Fix - Firmware Update Check Blocking Issue

## Problem Description

The ESP32 was experiencing **Task Watchdog Timer (WDT) crashes** approximately every 265 seconds after startup:

```
E (265209) task_wdt: Task watchdog got triggered. The following tasks did not reset the watchdog in time:
E (265209) task_wdt:  - loopTask (CPU 1)
```

The watchdog was triggered right after the firmware update check, indicating that the HTTP operations in `checkFirmwareUpdate()` were blocking the main loop without yielding control to reset the watchdog.

## Root Cause

The issue occurred in the firmware update checking logic:

1. **Main loop** calls `checkFirmwareUpdate()` every 12 hours (line 309 in main.cpp)
2. `checkFirmwareUpdate()` makes **blocking HTTP requests** to the backend server
3. During these requests, the code doesn't yield control frequently enough
4. The watchdog timer (configured for 60 seconds) times out waiting for a reset
5. Watchdog triggers, causing the ESP32 to crash and reboot

The HTTP operations are inherently blocking:
- WiFi HTTP GET requests
- 4G cellular HTTP GET requests  
- JSON parsing of large responses
- Response handling

## Solution

### 1. Disable Periodic Firmware Check in Main Loop

**File:** `src/main.cpp` (lines 303-315)

The periodic firmware check has been **disabled** in the main loop because:
- Firmware check is already performed at startup in `networkTask()`
- The web interface (`handleFirmwareCheck()`) provides on-demand firmware checking
- Periodic checking in the main loop causes unnecessary blocking

```cpp
// ❌ DISABLED: Firmware check in main loop causes watchdog timeout
// HTTP operations block for too long without yielding control
// Firmware check only happens at startup via networkTask and via web interface
// Periodic checks are disabled to prevent WDT crashes
/*
if (currentTime - lastFirmwareCheck >= FIRMWARE_CHECK_INTERVAL) {
  // ... periodic check code disabled ...
}
*/
```

### 2. Add Watchdog Resets During HTTP Operations

**File:** `src/firmware_update.cpp` - `checkFirmwareUpdate()` function

Added frequent `esp_task_wdt_reset()` calls during:
- Before/after HTTP GET requests
- After receiving response data
- After JSON parsing
- After data extraction
- After connection closing

This ensures the watchdog is reset regularly even if HTTP operations take time.

### 3. Add Watchdog Resets During Firmware Download

**File:** `src/firmware_update.cpp` - `performOTAUpdate()` function

Added progressive watchdog resets during the firmware download loop:
- Reset before starting the transfer
- Reset every 10 chunks (~5KB) during download
- Reset during delay periods
- Reset before finishing the update

This prevents timeout during long firmware file downloads over slow 4G connections.

## Testing

To verify the fix works:

1. **Build and upload** the modified firmware
2. **Monitor the serial output** - it should NOT crash after ~265 seconds
3. **Connect via the web interface** and test manual firmware checks
4. **Verify 4G firmware checks** work without crashes

## Configuration

The watchdog timeout is configured as:
```cpp
esp_task_wdt_init(60, true);  // 60 second timeout
esp_task_wdt_add(NULL);        // Apply to current task (loopTask)
```

With the fix, the watchdog will:
- Be reset regularly in the main loop (line 287, 296, 301, 319)
- Be reset during firmware checks (if called from web interface)
- Be reset during firmware downloads
- Never timeout on the loopTask

## Firmware Version

This fix applies to **version 1.0.1** and later.

## Related Issues

- Previous watchdog timeout: ~265 seconds into normal operation
- Affected: All 4G-first connection modes
- Triggered: During automatic firmware update checks after network initialization

## Future Improvements

Consider implementing:
- Separate firmware check task that doesn't block the main loop
- Implement firmware check with timeout and retry logic
- Add configurable watchdog timeout values
- Monitor watchdog reset frequency in production
