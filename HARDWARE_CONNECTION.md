# SƠ ĐỒ KẾT NỐI PHẦN CỨNG (ESP32 + DS18B20 + MQ2 + IR Fire + 4G A7680C)

Các chân dưới đây bám theo cấu hình hiện tại trong `src/config.h`. Nếu bạn đổi pin trong `config.h` thì cắm theo pin mới.

## Tóm tắt mapping theo config hiện tại

- DS18B20 (1‑Wire): Data → GPIO 23 (khuyến nghị) hoặc GPIO 4/5/12/13/14/15/16/17. Tránh GPIO34–39 vì chỉ input. Trong `config.h` của bạn đang đặt `TEMP_SENSOR_PIN 34` là không phù hợp — hãy chuyển sang GPIO 23 hoặc 4 để hoạt động đúng, kèm điện trở kéo lên 4.7kΩ.
- MQ2 (Analog): A0 của module → GPIO 36 (ADC1_CH0, ký hiệu VP)
- IR Fire (Digital): OUT → GPIO 2 (đã bật INPUT_PULLUP trong code)
- Buzzer: (+) → GPIO 18, (–) → GND
- LED cảnh báo: Anode(+) qua R≈220Ω → GPIO 19, Cathode(–) → GND
- Nguồn cảm biến: MQ2 và IR Fire dùng 5V; DS18B20 dùng 3.3V hoặc 5V. Tất cả phải chung GND với ESP32.
- Module 4G SIMCOM A7680C:
  - Nguồn riêng 4.0V–4.2V, tối thiểu 2A đỉnh (không dùng 5V trực tiếp).
  - UART (theo `config.h`): TXD module → GPIO 26 (CELL_RX_PIN), RXD module → GPIO 27 (CELL_TX_PIN).
  - PWRKEY của module → GPIO 4 (CELL_PWRKEY_PIN) qua transistor/level phù hợp nếu cần; code giữ mức theo thời gian `CELL_PWRKEY_ACTIVE_MS` để bật.

## Sơ đồ chữ (ASCII)

```
                ESP32 DevKit (3.3V logic)
                ┌──────────────────────────┐
3.3V ──────────┤ DS18B20 VCC              │
GND  ──────────┤ DS18B20 GND              │
GPIO23 ────────┤ DS18B20 DATA             │  ← kéo lên 4.7kΩ lên 3.3V
                │                          │
5V   ──────────┤ MQ2 VCC                  │
GND  ──────────┤ MQ2 GND                  │
GPIO36 (ADC1) ─┤ MQ2 A0                   │
                │                          │
5V   ──────────┤ IR VCC                   │
GND  ──────────┤ IR GND                   │
GPIO2 ─────────┤ IR OUT                   │ (đã bật INPUT_PULLUP)
                │                          │
GPIO18 ────────┤ Buzzer (+)               │  Buzzer (–) → GND
GPIO19 ──R220Ω─┤ LED (+)                  │  LED (–) → GND
                │                          │
4.0–4.2V/≥2A ──┤ A7680C VBAT              │
GND  ──────────┤ A7680C GND               │
GPIO26 ◀───────┤ A7680C TXD (về ESP32 RX) │
GPIO27 ───────▶┤ A7680C RXD (từ ESP32 TX) │
GPIO4  ────────┤ A7680C PWRKEY            │
                └──────────────────────────┘
```

## Lưu ý quan trọng

1) DS18B20 bắt buộc có điện trở kéo lên 4.7kΩ giữa DATA và VCC. Chân phải hỗ trợ output. Không dùng GPIO34–39 (input‑only). Đề xuất GPIO23 hoặc GPIO4.

2) MQ2 analog là tín hiệu 0–5V ở nhiều module; ESP32 ADC chỉ 0–3.3V. Hãy dùng biến trở trên board MQ2 để hạ mức ra còn ≤3.3V, hoặc thêm cầu chia áp nếu cần. Ta đã dùng ADC1 (GPIO36) để không xung đột Wi‑Fi.

3) IR Fire: nhiều module xuất active‑LOW; code đã bật `INPUT_PULLUP`. Nếu LED trên module luôn sáng, chỉnh biến trở độ nhạy hoặc đổi `FIRE_THRESHOLD` (LOW/HIGH) trong `config.h`.

4) 4G A7680C: cần nguồn riêng ổn định (đỉnh phát >2A). Dây nguồn ngắn, to; GND chung với ESP32. Kiểm tra UART đúng chiều (TX↔RX). Nếu khó ổn định với GPIO26/27, bạn có thể chuyển sang UART2 GPIO16/17 và cập nhật `config.h`.

5) GND chung: tất cả thiết bị (ESP32, MQ2, IR, DS18B20, 4G) phải chung GND.

6) HTTPS: nếu backend dùng HTTPS, hãy cấu hình `ROOT_CA` đúng định dạng PEM (không thụt dòng) và `HTTPS_INSECURE=0`. Khi thử nhanh có thể đặt `HTTPS_INSECURE=1` (không khuyến nghị production).

## Checklist trước khi cấp nguồn

- [ ] Xác nhận DS18B20 nối pin hỗ trợ output + có 4.7kΩ pull‑up.
- [ ] MQ2 A0 đưa về GPIO36 và mức điện áp ≤3.3V.
- [ ] IR OUT về GPIO2, điều chỉnh biến trở độ nhạy.
- [ ] Buzzer/LED đúng cực; có điện trở hạn dòng cho LED.
- [ ] A7680C nguồn riêng 4.0–4.2V, GND chung; TX/RX đúng chiều; PWRKEY nối đúng.
