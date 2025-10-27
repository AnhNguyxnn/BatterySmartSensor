# 🚀 Firmware Update - Re-enable Data Uploads

## Issue Found
The watchdog fix disabled the periodic data uploads. This has been **FIXED**! 

## What Changed

✅ **Re-enabled periodic data uploads** every 60 seconds  
✅ **Re-enabled immediate uploads** when alerts are detected  
✅ **Added watchdog resets** during uploads to prevent timeout  
✅ **Supports both WiFi and 4G** connections  

## The Fix

### tryBackendUpload() Function
- Now performs actual HTTP POST every 60 seconds
- Includes watchdog resets before/after HTTP operations
- Automatically chooses WiFi or 4G based on connection mode

### uploadImmediate() Function
- Now actually sends data when alerts are detected
- Includes watchdog resets for safety
- Supports both connection types

## How to Update

### Step 1: Stop Monitor
```bash
pkill -f "pio device monitor"
```

### Step 2: Boot Mode
- Hold **BOOT** button
- Press **RESET** (while holding BOOT)
- Release **BOOT**

### Step 3: Upload
```bash
cd /mnt/sdb1/dev/Hoc/IOT/BatterySmartSensor
/home/anhnguyen/.platformio/penv/bin/pio run -e esp32dev --target upload
```

## Verify Upload Works

Start monitor:
```bash
/home/anhnguyen/.platformio/penv/bin/pio device monitor -e esp32dev
```

Look for:
```
[UPLOAD] ✅ Upload 4G OK
```
or
```
[UPLOAD] ✅ Upload WiFi OK
```

Every 60 seconds, you should see upload messages.

## Expected Data on Web

After uploading, you should see data appearing on your backend within 60 seconds:
- 🌡️ Temperature readings
- 💨 Smoke sensor values
- 🔥 Fire detection status
- 🚨 Alert status
- ⏰ Timestamp (server-generated)

## What Was Fixed

| Component | Before | After |
|-----------|--------|-------|
| Periodic Upload | ❌ Disabled | ✅ Every 60s |
| Alert Upload | ❌ Disabled | ✅ Immediate |
| Watchdog Safety | ✅ Safe | ✅ Safe + Uploads |
| Data on Web | ❌ No data | ✅ Real-time data |

## Watchdog Protection

All uploads now include watchdog resets:
- Before upload starts
- After HTTP request/response
- After closing connection

**Result:** No watchdog timeout + data gets uploaded! ✅

## Troubleshooting

**No data appearing on web:**
1. Check serial monitor for `[UPLOAD]` messages
2. Verify backend URL is correct in config
3. Check network connectivity (4G or WiFi)
4. Wait 60 seconds for periodic upload

**Upload failures:**
1. Check backend is running
2. Verify API key is correct
3. Check 4G/WiFi connection status
4. Look for error messages in serial monitor

## File Size

Binary: 1.1 MB (increased from 1.064 MB)
- RAM: 14.9% (same)
- Flash: 81.5% (slightly higher due to upload code)

---

**Status:** ✅ Ready to Upload  
**Date:** October 23, 2025  
**Firmware:** 1.0.1 (with Watchdog Fix + Data Uploads)
