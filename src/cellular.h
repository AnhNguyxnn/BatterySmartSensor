#ifndef CELLULAR_H
#define CELLULAR_H

/**
 * @file cellular.h
 * @brief Khai báo mọi hàm/phần dùng để điều khiển modem 4G SIMCOM từ firmware ESP32.
 *
 * Mỗi hàm đều có chú thích tiếng Việt để đội kỹ thuật dễ tra cứu khi cần bảo trì hiện trường.
 */

#include <Arduino.h>

// Bảo đảm TinyGSM build với hỗ trợ SSL (một số request cần HTTPS)
#ifndef TINY_GSM_USE_SSL
#define TINY_GSM_USE_SSL
#endif

#include <TinyGsmClient.h>
#include <ArduinoHttpClient.h>
#include "config.h"

// --------------------------------------------------------------------
// Biến toàn cục được khởi tạo trong cellular.cpp
// --------------------------------------------------------------------
extern TinyGsm modem;
extern TinyGsmClient gsmClient;

// --------------------------------------------------------------------
// Nguyên mẫu hàm - mô tả chi tiết ngay tại đây
// --------------------------------------------------------------------

/**
 * @brief Đánh thức modem bằng tín hiệu PWRKEY theo wiring tại config.h.
 * @return true nếu đã gửi xong chuỗi điều khiển (không kiểm tra modem phản hồi).
 */
bool cellularPowerOn();

/**
 * @brief Chu trình khởi tạo modem (AT, SIM, APN) và mở data context.
 * @return true nếu modem sẵn sàng để thực hiện HTTP, false nếu có lỗi.
 */
bool cellularBegin();

/**
 * @brief Gửi HTTP POST qua 4G với timeout mặc định, retry nhẹ.
 * @param host Tên miền backend (đã ghi trong config).
 * @param port Cổng dịch vụ.
 * @param path Đường dẫn API.
 * @param body Payload JSON.
 * @param response Chuỗi phản hồi để trả lại cho caller.
 * @return true nếu status 2xx, false nếu thất bại.
 */
bool cellularHttpPost(const char* host, uint16_t port, const char* path, const String& body, String& response);

/**
 * @brief Gửi HTTP GET qua 4G, dùng cho kiểm tra firmware hoặc tải metadata.
 */
bool cellularHttpGet(const char* host, uint16_t port, const char* path, String& response);

/**
 * @brief POST với khả năng tự cấu hình timeout/số lần thử/backoff.
 *
 * Dùng trong các tình huống đặc biệt (ví dụ OTA hoặc cảnh báo quan trọng).
 */
bool cellularHttpPostWithOptions(const char* host, uint16_t port, const char* path,
                                 const String& body, String& response,
                                 uint16_t timeoutMs, int attempts, uint16_t backoffMs);

/**
 * @brief POST cực nhanh, không retry (dành cho cảnh báo khẩn, tiết kiệm thời gian).
 */
bool cellularHttpPostCritical(const char* host, uint16_t port, const char* path, const String& body, String& response);

/**
 * @brief POST HTTPS thông qua tập lệnh AT +CHTTPS* (không dùng TinyGSM stack).
 *
 * Phù hợp khi backend yêu cầu TLS mà modem không hỗ trợ trong TinyGSM.
 */
bool cellularHttpPostAT(const char* host, uint16_t port, const char* path, const String& body, String& response);

/**
 * @brief Reset hoàn toàn modem (tắt/bật lại) khi gặp lỗi không hồi phục.
 */
void cellularReset();

/**
 * @brief Lấy chuỗi mô tả trạng thái 4G (IMEI, chất lượng sóng, IP...) để hiển thị trên UI.
 */
String cellularStatusSummary();

/**
 * @brief Tải firmware nhị phân qua 4G và ghi trực tiếp vào flash (Update class).
 */
bool cellularOtaDownload(const char* host, uint16_t port, const char* path);

#endif


