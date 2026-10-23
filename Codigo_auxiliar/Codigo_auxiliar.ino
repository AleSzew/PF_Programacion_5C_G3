// ESP-01 (ESP8266) - Módulo Auxiliar UDP con MPU6050
#include <Wire.h>
#include <MPU6050.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

// ============================================================
// CONFIGURACIÓN DE IDENTIFICACIÓN Y RED
// ============================================================
// Selecciona cuál auxiliar es este módulo:
// Cambiar a '1' para Auxiliar 1 (IP Estática: 192.168.4.2)
// Cambiar a '2' para Auxiliar 2 (IP Estática: 192.168.4.3)
#define NUMERO_AUXILIAR 1

const char* ssid = "ESP32_C3_Server";
const char* password = "GRUPO3XX";

// Direcciones IP de la red local
IPAddress ipPrincipal(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

#if NUMERO_AUXILIAR == 1
  IPAddress ipLocal(192, 168, 4, 2);
#else
  IPAddress ipLocal(192, 168, 4, 3);
#endif

const uint16_t puertoUDP = 8888;
WiFiUDP udp;

// ============================================================
// HARDWARE Y SENSOR (Pines I2C para ESP-01)
// ============================================================
MPU6050 sensor;

#define PIN_SDA 2  // GPIO2 (IO2)
#define PIN_SCL 0  // GPIO0 (IO0)

float offset_X = 0, offset_Y = 0, offset_Z = 0;
bool mpuOK = false;
bool midiendo = false;

unsigned long tUltimoEnvio = 0;
const unsigned long INTERVALO_ENVIO_MS = 50; // Transmisión a 20 Hz (cada 50 ms)

// ============================================================
// FUNCIONES DEL MPU6050
// ============================================================
void calibrarOffsetGlobal() {
  Serial.println("Calibrando MPU6050 Auxiliar...");
  float sumaX = 0, sumaY = 0, sumaZ = 0;
  const int MUESTRAS = 100;

  for (int i = 0; i < MUESTRAS; i++) {
    int16_t ax = 0, ay = 0, az = 0;
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
  Serial.println("Calibración completada.");
}

void leerEInclinacion(float &inclX, float &inclY, float &inclZ) {
  int16_t ax = 0, ay = 0, az = 0;
  sensor.getAcceleration(&ax, &ay, &az);

  float ax_ms2 = (ax / 16384.0) * 9.81;
  float ay_ms2 = (ay / 16384.0) * 9.81;
  float az_ms2 = (az / 16384.0) * 9.81;

  inclX = atan2(ax_ms2, sqrt(ay_ms2 * ay_ms2 + az_ms2 * az_ms2)) * 180.0 / PI - offset_X;
  inclY = atan2(ay_ms2, sqrt(ax_ms2 * ax_ms2 + az_ms2 * az_ms2)) * 180.0 / PI - offset_Y;
  inclZ = atan2(az_ms2, sqrt(ax_ms2 * ax_ms2 + ay_ms2 * ay_ms2)) * 180.0 / PI - offset_Z;
}

// ============================================================
// RECEPCIÓN DE COMANDOS UDP Y ENVÍO DE DATOS
// ============================================================
void procesarComandosUDP() {
  int packetSize = udp.parsePacket();
  if (packetSize > 0) {
    char buffer[32] = { 0 };
    udp.read(buffer, sizeof(buffer) - 1);
    String comando = String(buffer);
    comando.trim();

    Serial.print("Comando UDP recibido de Principal: ");
    Serial.println(comando);

    if (comando == "INICIAR") {
      midiendo = true;
      Serial.println("-> Botón presionado en Principal: MEDICIóN ACTIVADA");
    } else if (comando == "DETENER") {
      midiendo = false;
      Serial.println("-> Serie finalizada: MEDICIóN DETENIDA");
    }
  }
}

void enviarDatosUDP() {
  if (!mpuOK) {
    udp.beginPacket(ipPrincipal, puertoUDP);
    udp.print("ERROR_I2C");
    udp.endPacket();
    return;
  }

  if (!midiendo) {
    udp.beginPacket(ipPrincipal, puertoUDP);
    udp.print("NOMIDIENDO");
    udp.endPacket();
    return;
  }

  float x, y, z;
  leerEInclinacion(x, y, z);

  // Formato: "X,Y,Z" compatible con la función recibirPaquetesUDP() del principal
  String payload = String(x, 2) + "," + String(y, 2) + "," + String(z, 2);

  udp.beginPacket(ipPrincipal, puertoUDP);
  udp.print(payload);
  udp.endPacket();

  Serial.print("Enviando mediciones UDP: ");
  Serial.println(payload);
}

// ============================================================
// INICIALIZACIÓN
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n==================================");
  Serial.print("  ESP-01 AUXILIAR ");
  Serial.println(NUMERO_AUXILIAR);
  Serial.println("==================================");

  // Inicialización I2C (SDA = IO2, SCL = IO0)
  Wire.begin(PIN_SDA, PIN_SCL);
  sensor.initialize();

  if (sensor.testConnection()) {
    Serial.println("✅ MPU6050 detectado correctamente.");
    calibrarOffsetGlobal();
    mpuOK = true;
  } else {
    Serial.println("❌ ERROR: No se pudo comunicar con el MPU6050.");
    mpuOK = false;
  }

  // Configuración Wi-Fi robusta para ESP8266
  WiFi.persistent(false);
  WiFi.disconnect(true);
  delay(100);

  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP); // Desactiva la suspensión de radio Wi-Fi

  if (!WiFi.config(ipLocal, gateway, subnet, gateway)) {
    Serial.println("Error al aplicar IP estática.");
  }

  WiFi.begin(ssid, password);
  Serial.print("Conectando al AP Principal");

  int intentos = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    intentos++;
    if (intentos > 20) {
      Serial.println("\nReintentando asociación con la red...");
      WiFi.begin(ssid, password);
      intentos = 0;
    }
  }

  Serial.println("\n✅ WiFi Conectado!");
  Serial.print("IP del Auxiliar: ");
  Serial.println(WiFi.localIP());

  udp.begin(puertoUDP);
  Serial.println("Puerto UDP en escucha y listo.");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
    delay(500);
    return;
  }

  // Lee si llegó la orden INICIAR o DETENER desde la placa principal
  procesarComandosUDP();

  // Envía paquetes UDP periódicos
  if (millis() - tUltimoEnvio >= INTERVALO_ENVIO_MS) {
    tUltimoEnvio = millis();
    enviarDatosUDP();
  }
}
