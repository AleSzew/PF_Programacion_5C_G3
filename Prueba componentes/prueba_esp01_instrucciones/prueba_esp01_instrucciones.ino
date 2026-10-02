// ============================================================
// AUXILIAR -- MODO CALIBRACION
// Se conecta como ESTACION al AP del principal y expone /datos.
// Toda la calibracion (offset + rango) la hace el PRINCIPAL,
// pidiendole datos a este auxiliar por HTTP.
// ============================================================
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "I2Cdev.h"
#include "MPU6050.h"
#include "Wire.h"

const char* ssid_principal = "CALIBRACION_PRINCIPAL";
const char* password_principal = "12345678";

IPAddress miIP(192, 168, 4, 2);
IPAddress gateway(192, 168, 4, 1);
IPAddress subred(255, 255, 255, 0);

ESP8266WebServer server(80);
MPU6050 sensor;

void handleDatos() {
  int16_t ax, ay, az;
  sensor.getAcceleration(&ax, &ay, &az);
  float x = (ax / 16384.0) * 9.81;
  float y = (ay / 16384.0) * 9.81;
  float z = (az / 16384.0) * 9.81;

  float ix = atan2(x, sqrt(y * y + z * z)) * 180.0 / PI;
  float iy = atan2(y, sqrt(x * x + z * z)) * 180.0 / PI;
  float iz = atan2(z, sqrt(x * x + y * y)) * 180.0 / PI;

  // Manda los angulos CRUDOS, sin ningun offset -- la calibracion
  // la hace el principal con estos datos crudos.
  server.send(200, "text/plain", String(ix, 3) + "," + String(iy, 3) + "," + String(iz, 3));
}

void setup() {

    Serial.begin(115200);
    Serial.println("Arrancando... conecta el MPU6050 AHORA si todavia no lo hiciste.");
    delay(5000);  // tiempo para conectar el sensor con el ESP-01 ya arrancado

    Wire.begin(2, 0);
    sensor.initialize();
     if (!sensor.testConnection()) {
      Serial.println("Error: MPU6050 no responde.");
    }

    WiFi.mode(WIFI_STA);
    WiFi.config(miIP, gateway, subred);
    WiFi.begin(ssid_principal, password_principal);
    WiFi.setSleepMode(WIFI_NONE_SLEEP);

    Serial.println("Conectando al principal...");
    unsigned long inicio = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - inicio < 15000) {
      delay(300);
      Serial.print(".");
    }
    Serial.println(WiFi.status() == WL_CONNECTED ? "\nConectado." : "\nNo se pudo conectar.");

    server.on("/datos", handleDatos);
    server.begin();
  }

  void loop() {
    if (WiFi.status() != WL_CONNECTED) {
      static unsigned long ultimoIntento = 0;
      if (millis() - ultimoIntento > 10000) {
        ultimoIntento = millis();
        WiFi.begin(ssid_principal, password_principal);
      }
      return;
    }
    server.handleClient();
  }