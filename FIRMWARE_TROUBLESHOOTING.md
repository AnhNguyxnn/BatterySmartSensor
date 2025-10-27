# Troubleshooting Firmware Upload

## ❌ Vấn đề: "Not Found" khi upload firmware

### 🔍 **Nguyên nhân**
Backend đang chạy với code cũ, chưa có firmware endpoints.

### ✅ **Giải pháp**

#### **Bước 1: Restart Backend**
```bash
# Chạy script restart backend
./restart_backend.sh
```

#### **Bước 2: Test Firmware Endpoints**
```bash
# Test tất cả firmware endpoints
./test_firmware_endpoints.sh
```

#### **Bước 3: Upload Firmware**
```bash
# Tạo file firmware test
echo "fake firmware content for testing" > firmware.bin

# Upload firmware
curl -X POST http://cloud.anhnguyxn.io.vn:82/api/firmware/upload \
  -H "X-API-Key: battery_monitor_2024_secure_key" \
  -F "file=@firmware.bin" \
  -F "version=1.0.1" \
  -F "build=1" \
  -F "release_notes=Bug fixes"
```

## 🔧 **Manual Steps**

### **1. Stop Container**
```bash
docker stop battery-backend
docker rm battery-backend
```

### **2. Build New Image**
```bash
docker build -t battery-backend .
```

### **3. Start Container**
```bash
docker run -d \
    --name battery-backend \
    -p 82:8000 \
    -e BATTERY_API_KEY="battery_monitor_2024_secure_key" \
    battery-backend
```

### **4. Verify Endpoints**
```bash
# Check firmware info
curl http://cloud.anhnguyxn.io.vn:82/api/firmware/info

# Check firmware check
curl "http://cloud.anhnguyxn.io.vn:82/api/firmware/check?current_version=1.0.0-build1&device_id=battery_monitor_001"
```

## 🧪 **Test Commands**

### **Test Upload**
```bash
curl -X POST http://cloud.anhnguyxn.io.vn:82/api/firmware/upload \
  -H "X-API-Key: battery_monitor_2024_secure_key" \
  -F "file=@firmware.bin" \
  -F "version=1.0.1" \
  -F "build=1" \
  -F "release_notes=Bug fixes"
```

### **Test Download**
```bash
curl -O http://cloud.anhnguyxn.io.vn:82/api/firmware/download/1.0.1
```

### **Test Check**
```bash
curl "http://cloud.anhnguyxn.io.vn:82/api/firmware/check?current_version=1.0.0-build1&device_id=battery_monitor_001"
```

## 📊 **Expected Responses**

### **Firmware Info**
```json
{
  "version": "1.0.1",
  "build": 1,
  "download_url": "/api/firmware/download/1.0.1",
  "file_size": 32,
  "release_notes": "Bug fixes",
  "checksum": "abc123...",
  "uploaded_at": "2025-01-21T...",
  "filename": "battery_monitor_v1.0.1.bin"
}
```

### **Firmware Check**
```json
{
  "update_available": true,
  "latest_version": "1.0.1",
  "latest_build": 1,
  "current_version": "1.0.0-build1",
  "download_url": "/api/firmware/download/1.0.1",
  "file_size": 32,
  "release_notes": "Bug fixes",
  "checksum": "abc123..."
}
```

### **Upload Success**
```json
{
  "status": "success",
  "message": "Firmware v1.0.1 uploaded successfully",
  "firmware_info": {
    "version": "1.0.1",
    "build": 1,
    "download_url": "/api/firmware/download/1.0.1",
    "file_size": 32,
    "release_notes": "Bug fixes",
    "checksum": "abc123...",
    "uploaded_at": "2025-01-21T...",
    "filename": "battery_monitor_v1.0.1.bin"
  }
}
```

## 🚨 **Common Issues**

### **1. "Not Found" Error**
- **Cause**: Backend chưa có firmware endpoints
- **Solution**: Restart backend với code mới

### **2. "Invalid API Key" Error**
- **Cause**: API key không đúng
- **Solution**: Kiểm tra `X-API-Key` header

### **3. "Only .bin files allowed" Error**
- **Cause**: File không có extension .bin
- **Solution**: Đổi tên file thành .bin

### **4. Connection Refused**
- **Cause**: Backend không chạy
- **Solution**: Start backend container

## 🔍 **Debug Commands**

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
netstat -tlnp | grep :82
```

### **Check API Endpoints**
```bash
curl http://cloud.anhnguyxn.io.vn:82/docs
```

## ✅ **Success Checklist**

- [ ] Backend container đang chạy
- [ ] Port 82 accessible
- [ ] Firmware endpoints có trong OpenAPI spec
- [ ] API key đúng
- [ ] File firmware có extension .bin
- [ ] Upload response có status "success"
- [ ] ESP32 có thể check firmware update

## 🎯 **Next Steps**

1. **Restart backend** với firmware endpoints
2. **Test upload** firmware
3. **Test ESP32** firmware check
4. **Deploy production** firmware
5. **Monitor OTA** update process
