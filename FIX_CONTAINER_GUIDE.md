# Hướng dẫn Fix Container Crash

## 🎯 Vấn đề
Container bị crash vì thiếu dependency `python-multipart`.

## ✅ Giải pháp

### **Bước 1: Fix requirements.txt**
```bash
# Thêm python-multipart vào requirements.txt
echo "python-multipart==0.0.12" >> backend/requirements.txt
```

### **Bước 2: Stop và Remove Container cũ**
```bash
docker stop battery-backend
docker rm battery-backend
```

### **Bước 3: Build Image mới**
```bash
docker build -t battery-backend .
```

### **Bước 4: Start Container mới**
```bash
docker run -d \
    --name battery-backend \
    -p 8000:8000 \
    -e BATTERY_API_KEY="battery_monitor_2024_secure_key" \
    battery-backend
```

### **Bước 5: Test Container**
```bash
# Kiểm tra container status
docker ps | grep battery-backend

# Kiểm tra logs
docker logs battery-backend

# Test firmware endpoints
curl http://localhost:8000/api/firmware/info
```

## 🔧 Manual Commands

### **Fix Container**
```bash
# Stop container
docker stop battery-backend

# Remove container
docker rm battery-backend

# Build new image
docker build -t battery-backend .

# Start new container
docker run -d \
    --name battery-backend \
    -p 8000:8000 \
    -e BATTERY_API_KEY="battery_monitor_2024_secure_key" \
    battery-backend
```

### **Test Container**
```bash
# Check status
docker ps | grep battery-backend

# Check logs
docker logs battery-backend

# Test endpoints
curl http://localhost:8000/api/firmware/info
curl "http://localhost:8000/api/firmware/check?current_version=1.0.0-build1&device_id=battery_monitor_001"
```

## 📊 Expected Results

### **Container Status**
```
CONTAINER ID   IMAGE            COMMAND                  CREATED         STATUS         PORTS                    NAMES
d23512c14dde   battery-backend  "python -m uvicorn..."   2 minutes ago   Up 2 minutes   0.0.0.0:8000->8000/tcp   battery-backend
```

### **Firmware Info Response**
```json
{
  "version": "1.0.1",
  "build": 1,
  "download_url": "/api/firmware/download/1.0.1",
  "file_size": 1048576,
  "release_notes": "Bug fixes and improvements",
  "checksum": "abc123def456...",
  "uploaded_at": "2025-01-21T...",
  "filename": "battery_monitor_v1.0.1.bin"
}
```

### **Firmware Check Response**
```json
{
  "update_available": true,
  "latest_version": "1.0.1",
  "latest_build": 1,
  "current_version": "1.0.0-build1",
  "download_url": "/api/firmware/download/1.0.1",
  "file_size": 1048576,
  "release_notes": "Bug fixes and improvements",
  "checksum": "abc123def456..."
}
```

## 🚨 Troubleshooting

### **1. Container vẫn crash**
- **Cause**: Vẫn thiếu dependencies
- **Solution**: Kiểm tra logs và thêm dependencies

### **2. Port không accessible**
- **Cause**: Port binding sai
- **Solution**: Kiểm tra port mapping

### **3. Application không start**
- **Cause**: Lỗi trong code
- **Solution**: Kiểm tra logs và fix code

## ✅ Success Checklist

- [ ] Container đang chạy (Status: Up)
- [ ] Port 8000 accessible từ localhost
- [ ] Port 8000 accessible từ external
- [ ] Firmware endpoints có trong OpenAPI spec
- [ ] API key đúng
- [ ] Upload response có status "success"
- [ ] ESP32 có thể check firmware update

## 🎯 Next Steps

1. **Fix container** với python-multipart
2. **Test container** status và logs
3. **Test firmware endpoints**
4. **Upload firmware** và test ESP32
5. **Monitor OTA** update process
