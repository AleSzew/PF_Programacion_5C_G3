// ============================================================
// HERRAMIENTA DE CALIBRACION -- PRINCIPAL (ESP32-C3)
// Hace de Access Point (como en el sistema real), le pide datos
// al auxiliar por HTTP, y calibra AMBOS sensores (local + aux)
// a la vez: offset, ruido, y rango de movimiento del ejercicio.
// Resultado se ve por Serial (cable USB conectado todo el tiempo).
// ============================================================
#include <MPU6050.h>
#include "Wire.h"
#include "WiFi.h"
#include <HTTPClient.h>
#include "I2Cdev.h"

MPU6050 sensor;

const char* ssid = "CALIBRACION_PRINCIPAL";
const char* password = "12345678";
const char* urlAux = "http://192.168.4.2/datos";

void leerLocal(float &x, float &y, float &z) {
  int16_t ax, ay, az;
  sensor.getAcceleration(&ax, &ay, &az);
  float xa = (ax / 16384.0) * 9.81;
  float ya = (ay / 16384.0) * 9.81;
  float za = (az / 16384.0) * 9.81;
  x = atan2(xa, sqrt(ya * ya + za * za)) * 180.0 / PI;
  y = atan2(ya, sqrt(xa * xa + za * za)) * 180.0 / PI;
  z = atan2(za, sqrt(xa * xa + ya * ya)) * 180.0 / PI;
}

// Devuelve true si pudo leer, y llena x/y/z con los angulos crudos del auxiliar
bool leerAux(float &x, float &y, float &z) {
  HTTPClient http;
  http.setTimeout(500);
  http.begin(urlAux);
  int codigo = http.GET();
  if (codigo != 200) { http.end(); return false; }

  String payload = http.getString();
  http.end();

  int c1 = payload.indexOf(',');
  int c2 = payload.indexOf(',', c1 + 1);
  if (c1 < 0 || c2 < 0) return false;

  x = payload.substring(0, c1).toFloat();
  y = payload.substring(c1 + 1, c2).toFloat();
  z = payload.substring(c2 + 1).toFloat();
  return true;
}

// ----------------------------------------------------------
// Calibra un sensor (local o aux) en 2 pasos: offset+ruido, y rango.
// "nombreSensor" es solo para que el Serial diga de cual se trata.
// "esLocal" decide si lee del sensor propio o le pide al auxiliar.
// ----------------------------------------------------------
void calibrarSensor(const char* nombreSensor, bool esLocal) {
  Serial.print("\n========== Calibrando: ");
  Serial.print(nombreSensor);
  Serial.println(" ==========");

  // --- PASO 1: offset + ruido en reposo ---
  Serial.println("Paso 1: dejar el sensor quieto. Midiendo en 3 segundos...");
  delay(3000);

  const int MUESTRAS = 300;
  float sumaX = 0, sumaY = 0, sumaZ = 0;
  float valX[MUESTRAS], valY[MUESTRAS], valZ[MUESTRAS];
  int ok = 0;

  for (int i = 0; i < MUESTRAS; i++) {
    float ix, iy, iz;
    bool leido = esLocal ? (leerLocal(ix, iy, iz), true) : leerAux(ix, iy, iz);
    if (!leido) { i--; continue; } // reintenta esta muestra si el auxiliar no respondio
    valX[ok] = ix; valY[ok] = iy; valZ[ok] = iz;
    sumaX += ix; sumaY += iy; sumaZ += iz;
    ok++;
    delay(5);
  }

  float offsetX = sumaX / ok, offsetY = sumaY / ok, offsetZ = sumaZ / ok;

  float cuadX = 0, cuadY = 0, cuadZ = 0;
  for (int i = 0; i < ok; i++) {
    cuadX += pow(valX[i] - offsetX, 2);
    cuadY += pow(valY[i] - offsetY, 2);
    cuadZ += pow(valZ[i] - offsetZ, 2);
  }
  float ruidoX = sqrt(cuadX / ok), ruidoY = sqrt(cuadY / ok), ruidoZ = sqrt(cuadZ / ok);

  Serial.println("--- OFFSET (reposo) ---");
  Serial.print("offset X: "); Serial.println(offsetX, 3);
  Serial.print("offset Y: "); Serial.println(offsetY, 3);
  Serial.print("offset Z: "); Serial.println(offsetZ, 3);
  Serial.print("ruido  X: "); Serial.println(ruidoX, 3);
  Serial.print("ruido  Y: "); Serial.println(ruidoY, 3);
  Serial.print("ruido  Z: "); Serial.println(ruidoZ, 3);

  // --- PASO 2: rango de movimiento ---
  Serial.println("\nPaso 2: hace varias repeticiones del ejercicio (15 seg)...");
  delay(1000);

  float minX = 999, maxX = -999, minY = 999, maxY = -999, minZ = 999, maxZ = -999;
  unsigned long inicio = millis();
  while (millis() - inicio < 15000) {
    float ix, iy, iz;
    bool leido = esLocal ? (leerLocal(ix, iy, iz), true) : leerAux(ix, iy, iz);
    if (!leido) { delay(20); continue; }

    ix -= offsetX; iy -= offsetY; iz -= offsetZ;
    if (ix < minX) minX = ix; if (ix > maxX) maxX = ix;
    if (iy < minY) minY = iy; if (iy > maxY) maxY = iy;
    if (iz < minZ) minZ = iz; if (iz > maxZ) maxZ = iz;

    delay(20);
  }

  Serial.println("--- RANGO DE MOVIMIENTO (offset ya restado) ---");
  Serial.print("X: min="); Serial.print(minX, 2); Serial.print(" max="); Serial.println(maxX, 2);
  Serial.print("Y: min="); Serial.print(minY, 2); Serial.print(" max="); Serial.println(maxY, 2);
  Serial.print("Z: min="); Serial.print(minZ, 2); Serial.print(" max="); Serial.println(maxZ, 2);
}

void setup() {
  Serial.begin(115200);
  unsigned long esperaInicio = millis();
  while (!Serial && millis() - esperaInicio < 10000) delay(100);
  delay(500);

  Wire.begin(6, 7);
  sensor.initialize();
  if (!sensor.testConnection()) {
    Serial.println("Error: MPU6050 local no responde.");
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  Serial.print("AP creado. IP: ");
  Serial.println(WiFi.softAPIP());

  Serial.println("Escribi 'calibrar' y Enter cuando el auxiliar este conectado y quieras arrancar.");
}

void loop() {
  if (Serial.available() > 0) {
    String linea = Serial.readStringUntil('\n');
    linea.trim();
    if (linea == "calibrar") {
      Serial.print("Estaciones conectadas: ");
      Serial.println(WiFi.softAPgetStationNum());

      calibrarSensor("SENSOR LOCAL", true);

      if (WiFi.softAPgetStationNum() > 0) {
        calibrarSensor("SENSOR AUXILIAR", false);
      } else {
        Serial.println("\nNo hay auxiliar conectado, se omite su calibracion.");
      }

      Serial.println("\n\n===== CALIBRACION COMPLETA. Copiar los numeros de arriba. =====");
      Serial.println("Escribi 'calibrar' de nuevo para repetir.");
    }
  }
}