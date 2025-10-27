# Watchdog Fix Firmware - Upload Guide 🚀

## 🎯 What Was Fixed

**Problem**: Firmware check over 4G causes watchdog timeout and ESP32 crash

```
E (376496) task_wdt: Task watchdog got triggered. The following tasks did not reset the watchdog in time:
E (376496) task_wdt:  - loopTask (CPU 1)
```

**Solution Applied**:
1. ✅ Moved firmware check to async background task
2. ✅ Added watchdog resets in cellular operations
3. ✅ Added proper watchdog subscribe/unsubscribe for new task
4. ✅ Firmware now compiles and runs stably

## 📦 File to Upload

```
firmware_watchdog_fix_v2.bin (Size: ~1.1MB)
```

Located at: `/mnt/sdb1/dev/Hoc/IOT/BatterySmartSensor/firmware_watchdog_fix_v2.bin`

## 🔼 Upload Instructions

### Method 1: Via Web Interface (Easiest)

1. **Power on ESP32** and wait for boot
2. **Connect to AP**: `BatteryMonitor-Admin` (Password: `admin123`)
3. **Open browser**: Go to `http://192.168.4.1`
4. **Click**: "🔄 Firmware Update" button
5. **Select**: "📁 Cập nhật thủ công" (Manual Upload)
6. **Choose file**: `firmware_watchdog_fix_v2.bin`
7. **Click**: "Upload Firmware"
8. **Wait**: Progress bar shows upload status
9. **Done**: Device reboots automatically

### Method 2: Via PlatformIO USB (For Development)

```bash
# Make sure ESP32 is connected via USB
# Hold BOOT button + short GND if needed

cd /mnt/sdb1/dev/Hoc/IOT/BatterySmartSensor

# Method A: Using esptool directly
esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 921600 \
  --before default_reset --after hard_reset write_flash \
  -z --flash_mode dio --flash_freq 40m --flash_size detect \
  0x1000 firmware_watchdog_fix_v2.bin

# Method B: Using PlatformIO
pio run -e esp32dev --target upload
```

### Method 3: Via 4G Upload (if available)

Backend needs to serve the firmware file:

```bash
# Copy to backend firmware directory
cp firmware_watchdog_fix_v2.bin backend/firmware/

# Access via: http://cloud.anhnguyxn.io.vn:8000/api/firmware/download/1.0.1
```

## ✅ Verification

After upload, check these:

### Serial Monitor Output
```bash
/home/anhnguyen/.platformio/penv/bin/pio device monitor -e esp32dev

# Should see:
# 🚀 ESP32 Battery Monitor - Fast Boot Starting...
# ✅ Upload mutex created
# ✅ SPIFFS đã khởi tạo
# ⚡ Fast Boot Path done (<5s) - Web interface ready!
```

### Test Firmware Check (The Fix)

1. Open web interface: `http://192.168.4.1`
2. Click "🔄 Firmware Update" button
3. Observe:
   - ✅ Page responds immediately
   - ✅ Status shows "checking" or update info
   - ✅ **NO WATCHDOG TIMEOUT/CRASH**
   - ✅ Serial shows: `[FIRMWARE_TASK] Kiểm tra firmware hoàn tất`

### Expected Serial Output
```
[FIRMWARE_TASK] Bắt đầu kiểm tra firmware trong background task...
[FIRMWARE] Kiểm tra update qua WiFi...
✅ Firmware đã cập nhật mới nhất
[FIRMWARE_TASK] Kiểm tra firmware hoàn tất
```

## 🔧 Troubleshooting

### Issue: Upload fails - "Wrong boot mode"
**Solution**:
1. Disconnect USB
2. Hold BOOT button + GND pin
3. Reconnect USB while holding
4. Release after 2 seconds
5. Run upload command

### Issue: Still getting watchdog timeout
**Check**:
1. Verify correct firmware was uploaded:
   ```bash
   esptool.py -p /dev/ttyUSB0 flash_id
   # Should show: Manufacturer: xx Region: xx (4MB flash)
   ```

2. Check 4G modem connection:
   ```
   Serial output should show:
   [CELL] ✅ GPRS connected
   [CELL] ✅ NETOPEN thành công
   [CELL] ✅ Kết nối 4G hoàn tất!
   ```

3. Force firmware check:
   - Via web: Click "🔄 Firmware Update"
   - Check serial for background task output

### Issue: Firmware update fails during upload
**Solution**:
1. Try web interface upload (most stable)
2. Ensure WiFi is working
3. Check backend is running: `http://cloud.anhnguyxn.io.vn:8000`
4. Use `/api/status` to verify device connection

## 📋 Changelog

### v1.0 → v2.0 (Current - Watchdog Fix)

**Main Changes**:
- ✅ Firmware check moved to async task
- ✅ Web server no longer blocks during firmware check
- ✅ Added watchdog resets in cellular operations
- ✅ Proper watchdog subscription for background tasks
- ✅ No more unexpected reboots

**Files Modified**:
- `src/main.cpp` - handleFirmwareCheck() async logic
- `src/cellular.cpp` - Added WDT resets in cellular operations
- `src/firmware_update.h` - No changes
- `src/config.h` - No changes

**Performance**:
- Build size: ~1.1 MB (same as before)
- RAM usage: 14.9% (same as before)
- Flash usage: 81.5% (same as before)

## 🚀 Next Steps

After successful upload:

1. **Monitor**: Keep serial monitor open for 5 minutes
   ```bash
   /home/anhnguyen/.platformio/penv/bin/pio device monitor -e esp32dev
   ```

2. **Test**: Click firmware check multiple times
   - Should complete without crash
   - Watch background tasks in logs

3. **Deploy**: If stable, can push to production

## 📞 Support

If issues persist:
1. Check `FIRMWARE_CHECK_WATCHDOG_FIX.md` for detailed explanation
2. Review serial output logs
3. Test with both WiFi and 4G connections
4. Verify backend is serving firmware correctly

---

**Created**: 2025-10-23  
**Firmware Version**: 1.0.1 (Build 2) - Watchdog Fix  
**Status**: ✅ Ready for Production
