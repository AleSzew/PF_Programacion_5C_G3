#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

const char* ssid = "ESP32_AP";
const char* password = "12345678";

ESP8266WebServer server(80);

void setup() {
  Serial.begin(115200);

  // Crear Access Point
  WiFi.softAP(ssid, password);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  // Endpoint simple
  server.on("/mensaje", []() {
    server.send(200, "text/plain", "Hola desde el servidor ESP-01");
  });

  server.begin();
}

void loop() {
  server.handleClient();
}