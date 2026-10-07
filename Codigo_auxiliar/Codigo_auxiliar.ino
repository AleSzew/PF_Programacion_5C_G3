// ESP-01 (Auxiliar)
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include "Wire.h"
#include "I2Cdev.h"
#include "MPU6050.h"

MPU6050 sensor;
WiFiUDP udp;
const uint16_t PuertoUDP = 8888;

const char* ssid_principal = "ESP32_C3_Server";
const char* password_principal = "GRUPO3XX";

// CAMBIAR A 192.168.4.3 EN EL AUXILIAR 2
IPAddress miIP(192, 168, 4, 2);
IPAddress gateway(192, 168, 4, 1);
IPAddress subred(255, 255, 255, 0);

float offset_X, offset_Y, offset_Z;
float inclX, inclY, inclZ;
bool midiendo = false;
unsigned long ultimoEnvioUDP = 0;
const unsigned long INTERVALO_ENVIO_UDP = 50;  // Envío a 20Hz

void calibrarOffset() {
  Serial.println("Calibrando offset auxiliar...");
  float sumaX = 0, sumaY = 0, sumaZ = 0;
  const int MUESTRAS = 100;
  for (int i = 0; i < MUESTRAS; i++) {
    int16_t ax, ay, az;
    sensor.getAcceleration(&ax, &ay, &az);
    float x = (ax / 16384.0) * 9.81;
    float y = (ay / 16384.0) * 9.81;
    float z = (az / 16384.0) * 9.81;
    sumaX += atan2(x, sqrt(y * y + z * z)) * 180.0 / PI;
    sumaY += atan2(y, sqrt(x * x + z * z)) * 180.0 / PI;
    sumaZ += atan2(z, sqrt(x * x + y * y)) * 180.0 / PI;
    delay(10);
  }
  offset_X = sumaX / MUESTRAS;
  offset_Y = sumaY / MUESTRAS;
  offset_Z = sumaZ / MUESTRAS;
  Serial.println("Calibracion auxiliar OK.");
}

bool leerInclinacion() {
  Wire.beginTransmission(0x68);
  if (Wire.endTransmission() != 0) {
    Wire.begin(2, 0);  // Reintento de reinicio I2C
    return false;
  }

  int16_t ax, ay, az;
  sensor.getAcceleration(&ax, &ay, &az);
  float x = (ax / 16384.0) * 9.81;
  float y = (ay / 16384.0) * 9.81;
  float z = (az / 16384.0) * 9.81;

  inclX = atan2(x, sqrt(y * y + z * z)) * 180.0 / PI - offset_X;
  inclY = atan2(y, sqrt(x * x + z * z)) * 180.0 / PI - offset_Y;
  inclZ = atan2(z, sqrt(x * x + y * y)) * 180.0 / PI - offset_Z;
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n--- ESP-01 AUXILIAR INICIANDO ---");

  Wire.begin(2, 0);  // GPIO0 = SDA, GPIO2 = SCL
  sensor.initialize();

  WiFi.mode(WIFI_STA);
  WiFi.config(miIP, gateway, subred);
  WiFi.begin(ssid_principal, password_principal);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);

  Serial.print("Conectando a AP ESP32...");
  unsigned long inicioWifi = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicioWifi < 10000) {
    delay(300);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConectado a WiFi AP. IP:");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nSin conexion inicial con AP. Reintentando en loop().");
  }

  calibrarOffset();
  udp.begin(PuertoUDP);
  Serial.println("UDP Listo en puerto 8888");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    static unsigned long ultimoReintento = 0;
    if (millis() - ultimoReintento > 2000) {
      ultimoReintento = millis();
      WiFi.begin(ssid_principal, password_principal);
    }
    return;
  }

  int packetSize = udp.parsePacket();
  if (packetSize > 0) {
    char packetBuffer[32] = { 0 };
    udp.read(packetBuffer, sizeof(packetBuffer) - 1);
    String comando = String(packetBuffer);
    comando.trim();

    if (comando == "INICIAR") {
      midiendo = true;
      Serial.println("Comando UDP: INICIAR medicion");
    } else if (comando == "DETENER") {
      midiendo = false;
      Serial.println("Comando UDP: DETENER medicion");
    }
  }

  if (midiendo && (millis() - ultimoEnvioUDP >= INTERVALO_ENVIO_UDP)) {
    ultimoEnvioUDP = millis();

    udp.beginPacket(gateway, PuertoUDP);
    if (!leerInclinacion()) {
      udp.print("ERROR_I2C");
    } else {
      String msg = String(inclX, 2) + "," + String(inclY, 2) + "," + String(inclZ, 2);
      udp.print(msg);
      Serial.println("Enviando: " + msg);
    }
    udp.endPacket();
  }
}