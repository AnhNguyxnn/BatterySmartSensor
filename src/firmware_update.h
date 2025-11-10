#ifndef FIRMWARE_UPDATE_H
#define FIRMWARE_UPDATE_H

/**
 * @file firmware_update.h
 * @brief Khai báo các hàm phục vụ kiểm tra và cập nhật firmware OTA cho thiết bị.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "config.h"

// Biến toàn cục được định nghĩa trong main.cpp và sử dụng lại tại đây
extern WebServer server;
extern bool firmwareUpdateAvailable;
extern String latestFirmwareVersion;
extern String latestFirmwareUrl;
extern bool firmwareNotificationAPActive;

/**
 * @brief Kiểm tra với backend xem có firmware mới không (ưu tiên Wi-Fi, fallback 4G).
 */
void checkFirmwareUpdate();

/**
 * @brief Bật cơ chế thông báo (AP và console log) khi phát hiện firmware mới.
 */
void startFirmwareNotificationAP();

/**
 * @brief Tắt thông báo firmware để trở lại trạng thái bình thường.
 */
void stopFirmwareNotificationAP();

/**
 * @brief Thực hiện OTA qua HTTP bằng Wi-Fi (đọc file nhị phân và ghi vào flash).
 * @param url Link trực tiếp đến firmware do server trả về.
 * @param method Dự phòng cho future (giữ tham số để đồng nhất prototype).
 */
bool performOTAUpdate(String url, String method);

/**
 * @brief Render trang HTML cho giao diện cập nhật firmware trong web server nội bộ.
 */
void handleFirmwareWebInterface();

#endif
