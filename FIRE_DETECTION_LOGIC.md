# Hướng dẫn Logic Phát hiện Cháy

## Thứ tự Ưu tiên Phát hiện Cháy

### 🔥 **Ưu tiên 1: NHIỆT ĐỘ CAO (Cao nhất)**
- **Điều kiện**: `temperature > TEMP_THRESHOLD` (60°C)
- **Hành động**: Báo cháy ngay lập tức
- **Lý do**: Pin quá nóng có thể gây cháy nổ
- **Thông báo**: `🔥 NHIỆT ĐỘ CAO (60.5°C)`

### ⚠️ **Ưu tiên 2: MQ2 + NHIỆT (Trung bình)**
- **Điều kiện**: `smokeValue > SMOKE_THRESHOLD` (300) + `temperature > 48°C` (80% của 60°C)
- **Hành động**: Báo pin xì khí nguy hiểm
- **Lý do**: Pin xì khí H₂, CH₄, ethylene + nhiệt = nguy hiểm
- **Thông báo**: `⚠️ PIN XÌ KHÍ: Khí độc (350) + Nhiệt (50.2°C)`

### 🔍 **Ưu tiên 3: IR FIRE (Thấp nhất)**
- **Điều kiện**: `fireDetected = true` + không có nhiệt/khí
- **Hành động**: Báo cháy cần xác minh
- **Lý do**: IR Fire hay báo sai, chỉ là điều kiện phụ
- **Thông báo**: `🔍 CẢM BIẾN IR PHÁT HIỆN CHÁY (cần xác minh)`

## Logic Chi tiết

### Kịch bản 1: Nhiệt độ cao
```
Nhiệt độ: 65°C (> 60°C) → BÁO CHÁY NGAY
Khí: 200 (< 300) → Không quan trọng
IR: false → Không quan trọng
Kết quả: 🚨 CẢNH BÁO: 🔥 NHIỆT ĐỘ CAO (65.0°C)
```

### Kịch bản 2: Nhiệt cao + Khí độc
```
Nhiệt độ: 65°C (> 60°C) → BÁO CHÁY NGAY
Khí: 350 (> 300) → Thêm vào cảnh báo
IR: false → Không quan trọng
Kết quả: 🚨 CẢNH BÁO: 🔥 NHIỆT ĐỘ CAO (65.0°C) + KHÍ ĐỘC HẠI (350)
```

### Kịch bản 3: Pin xì khí
```
Nhiệt độ: 50°C (> 48°C, < 60°C) → Đủ để báo xì khí
Khí: 350 (> 300) → Báo xì khí
IR: false → Không quan trọng
Kết quả: 🚨 CẢNH BÁO: ⚠️ PIN XÌ KHÍ: Khí độc (350) + Nhiệt (50.0°C)
```

### Kịch bản 4: IR Fire false positive
```
Nhiệt độ: 25°C (< 48°C) → Không báo
Khí: 150 (< 300) → Không báo
IR: true → Chỉ báo IR (ưu tiên thấp)
Kết quả: 🚨 CẢNH BÁO: 🔍 CẢM BIẾN IR PHÁT HIỆN CHÁY (cần xác minh)
```

## Cấu hình Ngưỡng

### config.h
```cpp
// Ngưỡng cảnh báo (theo thứ tự ưu tiên)
#define TEMP_THRESHOLD 60.0      // Nhiệt độ nguy hiểm (°C) - ƯU TIÊN CAO NHẤT
#define SMOKE_THRESHOLD 300      // Ngưỡng khí độc MQ2 (0-1023) - ƯU TIÊN TRUNG BÌNH
#define FIRE_THRESHOLD LOW       // Ngưỡng cảm biến cháy IR - ƯU TIÊN THẤP NHẤT

// Ngưỡng phụ cho logic phức tạp
#define TEMP_SMOKE_THRESHOLD_MULTIPLIER 0.8  // 80% của TEMP_THRESHOLD cho MQ2+Temp logic
```

### Điều chỉnh Ngưỡng

#### Nhiệt độ (TEMP_THRESHOLD)
- **Pin Li-ion**: 60°C (an toàn)
- **Pin LiFePO4**: 70°C (chịu nhiệt tốt hơn)
- **Pin Lead-acid**: 50°C (nhạy cảm hơn)

#### Khí độc (SMOKE_THRESHOLD)
- **Môi trường sạch**: 200-300
- **Môi trường có khói**: 400-500
- **Pin xì khí nhẹ**: 300-400
- **Pin xì khí nặng**: 500+

#### IR Fire (FIRE_THRESHOLD)
- **LOW**: Báo khi có lửa (default)
- **HIGH**: Báo khi không có lửa (inverted)

## Test Logic

### Test Nhiệt độ
```cpp
// Giả lập nhiệt độ cao
temperature = 65.0;  // > 60°C
smokeValue = 200;    // < 300
fireDetected = false;
// Kết quả: Báo cháy ngay
```

### Test Pin xì khí
```cpp
// Giả lập pin xì khí
temperature = 50.0;  // > 48°C, < 60°C
smokeValue = 350;    // > 300
fireDetected = false;
// Kết quả: Báo pin xì khí
```

### Test IR false positive
```cpp
// Giả lập IR báo sai
temperature = 25.0;  // < 48°C
smokeValue = 150;    // < 300
fireDetected = true;
// Kết quả: Báo IR cần xác minh
```

## Lợi ích

1. **Giảm false positive**: IR Fire không báo sai khi có nhiệt/khí thật
2. **Phát hiện sớm**: Nhiệt độ cao báo ngay, không chờ IR
3. **Phân biệt nguyên nhân**: Pin xì khí vs cháy thật
4. **Ưu tiên đúng**: Nhiệt độ > Khí độc > IR Fire
5. **Thông báo rõ ràng**: Người dùng biết chính xác vấn đề gì

## Troubleshooting

### IR Fire báo sai liên tục
- Kiểm tra ánh sáng mặt trời chiếu vào sensor
- Kiểm tra bụi bẩn trên sensor
- Điều chỉnh ngưỡng hoặc tắt IR Fire

### Không báo khi pin thật sự cháy
- Kiểm tra kết nối cảm biến nhiệt độ
- Kiểm tra ngưỡng TEMP_THRESHOLD
- Kiểm tra cảm biến MQ2 có hoạt động không

### Báo quá nhạy
- Tăng TEMP_THRESHOLD lên 65-70°C
- Tăng SMOKE_THRESHOLD lên 400-500
- Giảm TEMP_SMOKE_THRESHOLD_MULTIPLIER xuống 0.7
