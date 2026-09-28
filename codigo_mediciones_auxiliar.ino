#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <MPU6050.h>

const char* ssid = "ESP32_C3_Server";
const char* password = "GRUPO3XX";

IPAddress ip(192,168,4,2), gateway(192,168,4,1), subnet(255,255,255,0);
#define PIN_SDA 0
#define PIN_SCL 2

ESP8266WebServer server(80);
MPU6050 sensor;

void handleIniciar() { server.send(200,"text/plain","OK"); }
void handleDetener() { server.send(200,"text/plain","OK"); }

void handleDatos() {
  int16_t ax, ay, az;
  sensor.getAcceleration(&ax, &ay, &az);
  float x=(ax/16384.0)*9.81, y=(ay/16384.0)*9.81, z=(az/16384.0)*9.81;
  float inclY = atan2(y, sqrt(x*x+z*z)) * 180.0 / PI;
  server.send(200,"text/plain", String(x,2)+","+String(y,2)+","+String(z,2)+","+String(inclY,2));
}

void setup() {
  Serial.begin(115200);
  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClockStretchLimit(200);
  sensor.initialize();

  WiFi.mode(WIFI_STA);
  WiFi.config(ip, gateway, subnet);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(200);

  server.on("/iniciar", handleIniciar);
  server.on("/detener", handleDetener);
  server.on("/datos", handleDatos);
  server.begin();
}

void loop() {
  server.handleClient();
}