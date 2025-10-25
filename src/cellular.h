#ifndef CELLULAR_H
#define CELLULAR_H

#include <Arduino.h>
// Bảo đảm bật SSL trước khi include TinyGSM
#ifndef TINY_GSM_USE_SSL
#define TINY_GSM_USE_SSL
#endif
#include <TinyGsmClient.h>
#include <ArduinoHttpClient.h>
#include "config.h"

extern TinyGsm modem;
extern TinyGsmClient gsmClient;

bool cellularPowerOn();
bool cellularBegin();
bool cellularHttpPost(const char* host, uint16_t port, const char* path, const String& body, String& response);
bool cellularHttpGet(const char* host, uint16_t port, const char* path, String& response);
// HTTPS qua AT (SIMCOM CHTTPS*)
bool cellularHttpPostAT(const char* host, uint16_t port, const char* path, const String& body, String& response);
// Reset modem hoàn toàn khi gặp lỗi
void cellularReset();
// Tóm tắt trạng thái 4G cho UI
String cellularStatusSummary();

// OTA download qua 4G: tải binary và ghi vào flash qua Update
bool cellularOtaDownload(const char* host, uint16_t port, const char* path);

#endif


