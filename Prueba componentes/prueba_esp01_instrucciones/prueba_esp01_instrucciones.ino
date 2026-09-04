//no borrar comenrarios
//usar conector para subir esp01 de marco(ruben), tocar el boton siempre hasta antes de enchufarlo
//y hasta que se suba 
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "I2Cdev.h"
#include "MPU6050.h"
#include "Wire.h"

// Configuración de la red Wi-Fi
const char* ssid = "ESP01_MPU6050";
const char* password = "12345678";

ESP8266WebServer server(80);
MPU6050 sensor;

// Variables para el MPU6050
int16_t ax, ay, az;
float ax_ms2, ay_ms2, az_ms2;
float inclX = 0, inclY = 0, inclZ = 0;

// Temporizador no bloqueante
unsigned long tiempoPrevio = 0;
const long intervalo = 200; // Actualizar datos cada 200ms

void setup() {
  Serial.begin(115200);

  // 1. Inicialización de I2C para ESP-01 (GPIO2 = SDA, GPIO0 = SCL)
  Wire.begin(2, 0);
  sensor.initialize();

  if (sensor.testConnection()) {
    Serial.println("MPU6050 listo.");
  } else {
    Serial.println("Error al iniciar MPU6050.");
  }

  // 2. Crear Punto de Acceso (Access Point)
  WiFi.softAP(ssid, password);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  // 3. Ruta principal de respuesta Wi-Fi
  server.on("/mensaje", []() {
    String respuesta = "--- Lecturas MPU6050 ---\n";
    respuesta += "Inclinacion X: " + String(inclX, 2) + " deg\n";
    respuesta += "Inclinacion Y: " + String(inclY, 2) + " deg\n";
    respuesta += "Inclinacion Z: " + String(inclZ, 2) + " deg\n";

    server.send(200, "text/plain", respuesta);
  });

  server.begin();
  Serial.println("Servidor Web iniciado.");
}

void loop() {
  // Atender peticiones del servidor Web
  server.handleClient();

  // Lectura periódica del sensor
  unsigned long tiempoActual = millis();
  if (tiempoActual - tiempoPrevio >= intervalo) {
    tiempoPrevio = tiempoActual;

    sensor.getAcceleration(&ax, &ay, &az);

    ax_ms2 = (ax / 16384.0) * 9.81;
    ay_ms2 = (ay / 16384.0) * 9.81;
    az_ms2 = (az / 16384.0) * 9.81;

    inclX = atan2(ax_ms2, sqrt(ay_ms2 * ay_ms2 + az_ms2 * az_ms2)) * 180.0 / PI;
    inclY = atan2(ay_ms2, sqrt(ax_ms2 * ax_ms2 + az_ms2 * az_ms2)) * 180.0 / PI;
    inclZ = atan2(az_ms2, sqrt(ax_ms2 * ax_ms2 + ay_ms2 * ay_ms2)) * 180.0 / PI;
  }
}