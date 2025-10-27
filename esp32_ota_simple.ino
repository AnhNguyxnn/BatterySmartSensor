#include <WebServer.h>
#include <Update.h>
#include <WiFi.h>

WebServer server(80);

void handleRoot() {
  server.send(200, "text/html", 
    "<html><body>"
    "<h1>ESP32 Firmware Update</h1>"
    "<form method='POST' action='/update' enctype='multipart/form-data'>"
    "<input type='file' name='firmware'><br><br>"
    "<input type='submit' value='Update Firmware'>"
    "</form>"
    "</body></html>"
  );
}

void handleUpdate() {
  server.sendHeader("Connection", "close");
  server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
  ESP.restart();
}

void handleUpload() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    Update.begin(UPDATE_SIZE_UNKNOWN);
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    Update.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    Update.end(true);
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.softAP("ESP32-OTA", "12345678");
  server.on("/", handleRoot);
  server.on("/update", HTTP_POST, handleUpdate, handleUpload);
  server.begin();
  Serial.println("ESP32 OTA Server started");
  Serial.println("AP: ESP32-OTA");
  Serial.println("Password: 12345678");
  Serial.println("IP: 192.168.4.1");
}

void loop() {
  server.handleClient();
}