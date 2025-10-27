# Hướng dẫn Authentication API

## Cấu hình API Key

### Backend (FastAPI)
- **Environment Variable**: `BATTERY_API_KEY` (khuyến nghị)
- **Default fallback**: `battery_monitor_2024_secure_key` (chỉ dùng development)

### ESP32 (config.h)
```cpp
#define APPLICATION_KEY "battery_monitor_2024_secure_key"
```

## Test Authentication

### 1. Test không có API key (sẽ fail)
```bash
curl -X POST http://cloud.anhnguyxn.io.vn:8000/api/ingest \
  -H "Content-Type: application/json" \
  -d '{"timestamp":123,"temperature":25.5,"smoke_value":150,"smoke_connected":true,"mq2_preheated":true,"fire_detected":false,"alert_active":false,"device_id":"battery_monitor_001"}'
```
**Kết quả mong đợi:** `401 Unauthorized`

### 2. Test với API key sai (sẽ fail)
```bash
curl -X POST http://cloud.anhnguyxn.io.vn:8000/api/ingest \
  -H "Content-Type: application/json" \
  -H "X-API-Key: wrong_key" \
  -d '{"timestamp":123,"temperature":25.5,"smoke_value":150,"smoke_connected":true,"mq2_preheated":true,"fire_detected":false,"alert_active":false,"device_id":"battery_monitor_001"}'
```
**Kết quả mong đợi:** `401 Unauthorized`

### 3. Test với API key đúng (sẽ thành công)
```bash
curl -X POST http://cloud.anhnguyxn.io.vn:8000/api/ingest \
  -H "Content-Type: application/json" \
  -H "X-API-Key: battery_monitor_2024_secure_key" \
  -d '{"timestamp":123,"temperature":25.5,"smoke_value":150,"smoke_connected":true,"mq2_preheated":true,"fire_detected":false,"alert_active":false,"device_id":"battery_monitor_001"}'
```
**Kết quả mong đợi:** `{"id":1,"status":"ok"}`

## Cập nhật Backend với API Key mới

### Cách 1: Environment Variable (Khuyến nghị)
```bash
# Tạo API key mới
openssl rand -hex 32

# Set environment variable
export BATTERY_API_KEY="your_new_secure_key_here"

# Chạy Docker với ENV
sudo docker run -d --name battery-backend -p 82:8000 -e BATTERY_API_KEY="your_new_secure_key_here" battery-backend
```

### Cách 2: Docker Compose (Dễ nhất)
```bash
# Tạo file .env
echo "BATTERY_API_KEY=your_new_secure_key_here" > .env

# Chạy với docker-compose
sudo docker-compose up -d
```

### Cách 3: File .env
```bash
# Copy template
cp env.example .env

# Edit .env file
nano .env
# Set: BATTERY_API_KEY=your_new_secure_key_here

# Chạy backend
python -m uvicorn backend.app.main:app --host 0.0.0.0 --port 8000
```

## Bảo mật

1. **Thay đổi API key mặc định** trước khi deploy production
2. **Sử dụng HTTPS** trong production (hiện tại dùng HTTP để tránh lỗi 4G)
3. **Giới hạn IP** nếu cần thiết
4. **Rate limiting** để tránh spam

## Troubleshooting

### ESP32 không gửi được data
- Kiểm tra `APPLICATION_KEY` trong `config.h` khớp với backend
- Kiểm tra log `[WIFI] Upload` và `[CELL] HTTP` status code
- Nếu 401: API key sai
- Nếu 400: JSON format sai
- Nếu -1: Network error

### Backend không nhận được request
- Kiểm tra header `X-API-Key` có được gửi không
- Kiểm tra log backend để xem request có đến không
- Test với curl để verify API key
