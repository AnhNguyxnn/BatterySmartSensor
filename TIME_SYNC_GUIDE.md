# Hướng dẫn Đồng bộ Thời gian ESP32

## Vấn đề đã sửa

### ❌ **Trước đây**
- ESP32 sử dụng `millis()` làm timestamp
- `millis()` = thời gian từ khi khởi động (không phải thời gian thực)
- Dữ liệu có timestamp không chính xác

### ✅ **Bây giờ**
- ESP32 sync thời gian qua NTP (Network Time Protocol)
- Sử dụng `getCurrentTimestamp()` trả về Unix timestamp thực
- Timezone: UTC+7 (Vietnam)

## Cấu hình NTP

### ESP32 (config.h)
```cpp
#define NTP_SERVER "pool.ntp.org"
#define TIMEZONE_OFFSET 7        // UTC+7 (Vietnam)
#define NTP_SYNC_INTERVAL 3600000 // Sync mỗi 1 giờ
```

### Chức năng
- **Auto sync**: Khi kết nối WiFi thành công
- **Periodic sync**: Mỗi 1 giờ để đảm bảo độ chính xác
- **Fallback**: Nếu không sync được, dùng `millis()/1000`

## Kiểm tra Thời gian

### 1. Log ESP32
```
[NTP] Đang sync thời gian...
[NTP] Thời gian đã sync: 2024-01-15 14:30:25
```

### 2. Backend Dashboard
- **Time**: Thời gian server nhận dữ liệu
- **ESP32 Time**: Unix timestamp từ ESP32

### 3. API Response
```json
{
  "id": 1,
  "device_id": "battery_monitor_001",
  "timestamp": 1705312225,
  "temperature": 25.5,
  "smoke_value": 150,
  "created_at": "2024-01-15T14:30:25.123456"
}
```

## Troubleshooting

### ESP32 không sync được thời gian
1. **Kiểm tra WiFi**: Phải kết nối internet
2. **Kiểm tra NTP server**: `pool.ntp.org` có thể bị chặn
3. **Thay đổi NTP server**:
   ```cpp
   #define NTP_SERVER "time.google.com"  // hoặc "time.cloudflare.com"
   ```

### Thời gian vẫn sai
1. **Kiểm tra timezone**:
   ```cpp
   #define TIMEZONE_OFFSET 7  // UTC+7 cho Vietnam
   ```
2. **Kiểm tra log**: `[NTP] Thời gian đã sync: 2024-01-15 14:30:25`
3. **Restart ESP32** để sync lại

### Backend hiển thị sai thời gian
1. **Kiểm tra server timezone**:
   ```bash
   timedatectl status
   ```
2. **Set timezone server**:
   ```bash
   sudo timedatectl set-timezone Asia/Ho_Chi_Minh
   ```

## Test Thời gian

### 1. Kiểm tra ESP32
```bash
# Xem log ESP32
pio device monitor
# Tìm dòng: [NTP] Thời gian đã sync: ...
```

### 2. Kiểm tra Backend
```bash
curl http://cloud.anhnguyxn.io.vn:8000/api/readings/latest
# So sánh timestamp và created_at
```

### 3. So sánh với thời gian thực
```bash
date +%s  # Unix timestamp hiện tại
# So sánh với ESP32 timestamp
```

## Cấu hình Nâng cao

### Thay đổi NTP Server
```cpp
// Google NTP
#define NTP_SERVER "time.google.com"

// Cloudflare NTP  
#define NTP_SERVER "time.cloudflare.com"

// Local NTP (nếu có)
#define NTP_SERVER "192.168.1.1"
```

### Thay đổi Timezone
```cpp
// UTC+8 (Singapore, Malaysia)
#define TIMEZONE_OFFSET 8

// UTC+9 (Japan, Korea)
#define TIMEZONE_OFFSET 9

// UTC+0 (London)
#define TIMEZONE_OFFSET 0
```

### Thay đổi Sync Interval
```cpp
// Sync mỗi 30 phút
#define NTP_SYNC_INTERVAL 1800000

// Sync mỗi 6 giờ
#define NTP_SYNC_INTERVAL 21600000
```

## Lưu ý

1. **NTP cần internet**: Không sync được khi offline
2. **Fallback mechanism**: Vẫn hoạt động với `millis()` nếu không sync được
3. **Battery impact**: Sync NTP không tốn nhiều pin
4. **Accuracy**: NTP chính xác đến giây, đủ cho monitoring
5. **DST**: Vietnam không có Daylight Saving Time
