# Hướng dẫn Restart Backend trên Server

## 🎯 Vấn đề
Backend đang chạy với code cũ, chưa có firmware endpoints `/api/firmware/upload`.

## ✅ Giải pháp

### **Phương pháp 1: Restart Backend trên Server**

#### **Bước 1: SSH vào Server**
```bash
ssh root@cloud.anhnguyxn.io.vn
# hoặc
ssh your_user@cloud.anhnguyxn.io.vn
```

#### **Bước 2: Stop Container cũ**
```bash
docker stop battery-backend
docker rm battery-backend
```

#### **Bước 3: Build Image mới**
```bash
cd /path/to/BatterySmartSensor
docker build -t battery-backend ./backend
```

#### **Bước 4: Start Container mới**
```bash
docker run -d \
    --name battery-backend \
    -p 8000:8000 \
    -e BATTERY_API_KEY="battery_monitor_2024_secure_key" \
    battery-backend
```

#### **Bước 5: Verify Endpoints**
```bash
curl http://localhost:8000/api/firmware/info
curl http://localhost:8000/api/firmware/check?current_version=1.0.0-build1
```

### **Phương pháp 2: Upload Firmware trực tiếp**

#### **Bước 1: Tạo thư mục firmware**
```bash
mkdir -p /root/backend/firmware
```

#### **Bước 2: Upload firmware file**
```bash
# Từ máy local
scp firmware.bin root@cloud.anhnguyxn.io.vn:/root/backend/firmware/battery_monitor_v1.0.1.bin
```

#### **Bước 3: Tạo firmware info**
```bash
cat > /root/backend/firmware/latest.json << EOF
{
  "version": "1.0.1",
  "build": 1,
  "download_url": "/api/firmware/download/1.0.1",
  "file_size": $(stat -c%s /root/backend/firmware/battery_monitor_v1.0.1.bin),
  "release_notes": "Bug fixes and improvements",
  "checksum": "$(md5sum /root/backend/firmware/battery_monitor_v1.0.1.bin | cut -d' ' -f1)",
  "uploaded_at": "$(date -Iseconds)",
  "filename": "battery_monitor_v1.0.1.bin"
}
EOF
```

### **Phương pháp 3: Sử dụng Script**

#### **Upload qua HTTP (Fallback)**
```bash
./upload_firmware_http.sh firmware.bin 1.0.1
```

#### **Upload trực tiếp lên Server**
```bash
./upload_firmware_direct.sh firmware.bin 1.0.1
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

### **Check Available Endpoints**
```bash
curl -s http://localhost:8000/openapi.json | python3 -c "import json, sys; data=json.load(sys.stdin); [print(f'{method.upper()} {path}') for path, methods in data['paths'].items() for method in methods.keys()]"
```

### **Test Firmware Endpoints**
```bash
# Test firmware info
curl http://localhost:8000/api/firmware/info

# Test firmware check
curl "http://localhost:8000/api/firmware/check?current_version=1.0.0-build1&device_id=battery_monitor_001"

# Test firmware upload
curl -X POST http://localhost:8000/api/firmware/upload \
  -H "X-API-Key: battery_monitor_2024_secure_key" \
  -F "file=@firmware.bin" \
  -F "version=1.0.1" \
  -F "build=1" \
  -F "release_notes=Bug fixes"
```

## 📊 Expected Results

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

### **Upload Success Response**
```json
{
  "status": "success",
  "message": "Firmware v1.0.1 uploaded successfully",
  "firmware_info": {
    "version": "1.0.1",
    "build": 1,
    "download_url": "/api/firmware/download/1.0.1",
    "file_size": 1048576,
    "release_notes": "Bug fixes and improvements",
    "checksum": "abc123def456...",
    "uploaded_at": "2025-01-21T...",
    "filename": "battery_monitor_v1.0.1.bin"
  }
}
```

## 🚨 Troubleshooting

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

## ✅ Success Checklist

- [ ] Backend container đang chạy
- [ ] Port 8000 accessible
- [ ] Firmware endpoints có trong OpenAPI spec
- [ ] API key đúng
- [ ] File firmware có extension .bin
- [ ] Upload response có status "success"
- [ ] ESP32 có thể check firmware update

## 🎯 Next Steps

1. **Restart backend** với firmware endpoints
2. **Test upload** firmware
3. **Test ESP32** firmware check
4. **Deploy production** firmware
5. **Monitor OTA** update process
