# HƯỚNG DẪN UPLOAD CODE LÊN ESP32

## Vấn đề hiện tại:
- Code đã biên dịch thành công ✅
- Không thể upload do vấn đề quyền truy cập cổng USB

## Giải pháp:

### Cách 1: Thêm user vào group dialout
```bash
sudo usermod -a -G dialout $USER
```
Sau đó đăng nhập lại hoặc chạy:
```bash
newgrp dialout
```

### Cách 2: Upload với quyền sudo
```bash
sudo /home/anhnguyen/.platformio/penv/bin/pio run -t upload -e esp32dev
```

### Cách 3: Thay đổi quyền cổng USB
```bash
sudo chmod 666 /dev/ttyUSB0
```

## Kiểm tra cổng USB:
```bash
ls -la /dev/ttyUSB*
```

## Upload thành công:
Sau khi upload thành công, mở Serial Monitor để xem dữ liệu:
```bash
/home/anhnguyen/.platformio/penv/bin/pio device monitor -e esp32dev
```

## Cấu hình WiFi:
Trước khi upload, nhớ sửa thông tin WiFi trong file `src/config.h`:
```cpp
#define WIFI_SSID "Tên_WiFi_của_bạn"
#define WIFI_PASSWORD "Mật_khẩu_WiFi"
```

## Kiểm tra hoạt động:
1. Mở Serial Monitor (115200 baud)
2. Quan sát quá trình kết nối WiFi
3. Kiểm tra dữ liệu từ các cảm biến
4. Test hệ thống cảnh báo
