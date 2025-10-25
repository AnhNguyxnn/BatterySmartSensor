#ifndef FIRMWARE_UPDATE_H
#define FIRMWARE_UPDATE_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "config.h"

// External variables from main.cpp
extern WebServer server;
extern bool firmwareUpdateAvailable;
extern String latestFirmwareVersion;
extern String latestFirmwareUrl;
extern bool firmwareNotificationAPActive;

// Function declarations
void checkFirmwareUpdate();
void startFirmwareNotificationAP();
void stopFirmwareNotificationAP();
bool performOTAUpdate(String url, String method);
void handleFirmwareWebInterface();

#endif
