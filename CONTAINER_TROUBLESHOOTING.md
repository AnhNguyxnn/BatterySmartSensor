# Hướng dẫn Troubleshooting Container

## 🎯 Vấn đề
Container đã build thành công nhưng không thể kết nối từ bên ngoài.

## ✅ Giải pháp

### **Bước 1: Kiểm tra tình trạng Container**
```bash
# Kiểm tra container có đang chạy không
docker ps | grep battery-backend

# Kiểm tra logs của container
docker logs battery-backend

# Kiểm tra port binding
docker port battery-backend
```

### **Bước 2: Kiểm tra Container Details**
```bash
# Kiểm tra chi tiết container
docker inspect battery-backend | grep -E "(Status|State|IPAddress|Ports)"

# Kiểm tra container có đang chạy không
docker ps -a | grep battery-backend
```

### **Bước 3: Test Connection**
```bash
# Test local connection
curl http://localhost:8000/api/firmware/info

# Test external connection
curl http://cloud.anhnguyxn.io.vn:8000/api/firmware/info
```

### **Bước 4: Restart Container nếu cần**
```bash
# Stop container
docker stop battery-backend

# Remove container
docker rm battery-backend

# Start container mới
docker run -d \
    --name battery-backend \
    -p 8000:8000 \
    -e BATTERY_API_KEY="battery_monitor_2024_secure_key" \
    battery-backend
```

## 🔧 Manual Commands

### **Check Container Status**
```bash
docker ps | grep battery-backend
```

### **Check Container Logs**
```bash
docker logs battery-backend
```

### **Check Port Binding**
```bash
docker port battery-backend
```

### **Check Container Details**
```bash
docker inspect battery-backend | grep -E "(Status|State|IPAddress|Ports)"
```

### **Test Local Connection**
```bash
curl http://localhost:8000/api/firmware/info
```

### **Test External Connection**
```bash
curl http://cloud.anhnguyxn.io.vn:8000/api/firmware/info
```

## 📊 Expected Results

### **Container Status**
```
CONTAINER ID   IMAGE            COMMAND                  CREATED         STATUS         PORTS                    NAMES
d23512c14dde   battery-backend  "python -m uvicorn..."   2 minutes ago   Up 2 minutes   0.0.0.0:8000->8000/tcp   battery-backend
```

### **Port Binding**
```
8000/tcp -> 0.0.0.0:8000
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

## 🚨 Troubleshooting

### **1. Container không chạy**
- **Cause**: Container bị crash hoặc chưa khởi động
- **Solution**: Kiểm tra logs và restart container

### **2. Port không accessible**
- **Cause**: Port binding sai hoặc firewall block
- **Solution**: Kiểm tra port binding và firewall

### **3. Application không start**
- **Cause**: Lỗi trong code hoặc dependencies
- **Solution**: Kiểm tra logs và fix code

### **4. External connection failed**
- **Cause**: Firewall hoặc network configuration
- **Solution**: Kiểm tra firewall và network

## ✅ Success Checklist

- [ ] Container đang chạy (Status: Up)
- [ ] Port 8000 accessible từ localhost
- [ ] Port 8000 accessible từ external
- [ ] Firmware endpoints có trong OpenAPI spec
- [ ] API key đúng
- [ ] Upload response có status "success"
- [ ] ESP32 có thể check firmware update

## 🎯 Next Steps

1. **Kiểm tra container status** và logs
2. **Test connection** từ localhost và external
3. **Fix issues** nếu có
4. **Test firmware endpoints**
5. **Upload firmware** và test ESP32
6. **Monitor OTA** update process
