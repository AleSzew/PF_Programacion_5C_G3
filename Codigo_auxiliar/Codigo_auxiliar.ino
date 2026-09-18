//no borrar comentarios
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "Wire.h"
#include "I2Cdev.h"
#include "MPU6050.h"

MPU6050 sensor;
ESP8266WebServer server(80);

// ============================================================
// RED: esta placa se conecta como cliente al ESP32 principal,
// que actúa de Access Point ("ESP32_C3_Server").
// ============================================================
const char* ssid_principal = "ESP32_C3_Server";
const char* password_principal = "GRUPO3XX";

// IP FIJA de esta placa. Esto es lo único que cambia entre
// el auxiliar 1 y el auxiliar 2: acá pongo .2 para el auxiliar 1.
// Para el auxiliar 2, cambiar SOLO esta línea a 192.168.4.3
IPAddress miIP(192, 168, 4, 2);
IPAddress gateway(192, 168, 4, 1);  // el ESP32 principal siempre es .1 en modo AP
IPAddress subred(255, 255, 255, 0);

// ============================================================
// OFFSET PROPIO DE ESTA PLACA
// ============================================================
// Cada auxiliar tiene su propio offset porque está montado en
// un lugar distinto del cuerpo. Se calcula una vez al arrancar,
// igual que en el principal.
float offset_X, offset_Y, offset_Z;
float inclX, inclY, inclZ;
bool midiendo = false;  // true mientras el principal esté calibrando o midiendo

void handleIniciar() {
  midiendo = true;
  Serial.println("Iniciando medicion (orden del principal)");
  server.send(200, "text/plain", "OK");
}

void handleDetener() {
  midiendo = false;
  Serial.println("Deteniendo medicion (orden del principal)");
  server.send(200, "text/plain", "OK");
}

void handleDatos() {
  if (!midiendo) {
    // Devuelve algo identificable como "no estoy midiendo"
    // en vez de una lectura real, para que el principal lo distinga
    server.send(200, "text/plain", "NOMIDIENDO");
    return;
  }

  leerInclinacion();
  String respuesta = String(inclX, 2) + "," + String(inclY, 2) + "," + String(inclZ, 2);
  server.send(200, "text/plain", respuesta);
}

void calibrarOffset() {
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
}

// Lee el sensor y actualiza inclX/Y/Z con el offset ya aplicado
void leerInclinacion() {
  int16_t ax, ay, az;
  sensor.getAcceleration(&ax, &ay, &az);
  float x = (ax / 16384.0) * 9.81;
  float y = (ay / 16384.0) * 9.81;
  float z = (az / 16384.0) * 9.81;

  inclX = atan2(x, sqrt(y * y + z * z)) * 180.0 / PI - offset_X;
  inclY = atan2(y, sqrt(x * x + z * z)) * 180.0 / PI - offset_Y;
  inclZ = atan2(z, sqrt(x * x + y * y)) * 180.0 / PI - offset_Z;
}


void setup() {
  Serial.begin(115200);

  // I2C en los únicos 2 pines disponibles del ESP-01
  Wire.begin(0, 2);  // GPIO0 = SDA, GPIO2 = SCL
  sensor.initialize();

  if (!sensor.testConnection()) {
    Serial.println("Error: MPU6050 no responde. Revisar cableado I2C.");
  }
  Serial.println("Escaneando redes cercanas...");
  int redesEncontradas = WiFi.scanNetworks();
  for (int i = 0; i < redesEncontradas; i++) {
    Serial.print(WiFi.SSID(i));
    Serial.print(" (canal ");
    Serial.print(WiFi.channel(i));
    Serial.print(", RSSI ");
    Serial.print(WiFi.RSSI(i));
    Serial.println(")");
  }
  // Conexión WiFi con IP fija (evita el problema de DHCP que
  // vimos: acá SIEMPRE va a ser 192.168.4.2, sin importar el
  // orden en que se prendan las placas)
  WiFi.mode(WIFI_STA);
  WiFi.config(miIP, gateway, subred);
  WiFi.begin(ssid_principal, password_principal);

  Serial.println("Conectando al ESP32 principal...");
  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < 15000) {
    delay(300);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConectado. IP asignada:");
    Serial.println(WiFi.localIP());
    calibrarOffset();  // recién calibra si logró conectarse
  } else {
    Serial.println("\nNo se pudo conectar al AP. Reintentando en loop().");
  }

  server.on("/datos", handleDatos);
  server.on("/iniciar", handleIniciar);  
  server.on("/detener", handleDetener); 
  server.begin();
}

void loop() {
  // Si se cayó la conexión (el AP se reinició, por ejemplo),
  // intenta reconectar sin bloquear el resto del programa.
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(ssid_principal, password_principal);
    delay(1000);
    return;
  }

  server.handleClient();
}