#include "I2Cdev.h"
#include "MPU6050.h"
#include "Wire.h"
#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include "BluetoothSerial.h"
#include <HTTPClient.h>  
#include <Ticker.h>
#include <NimBLEDevice.h>

MPU6050 sensor;
AsyncWebServer server(80);
NimBLEServer* pServer;
NimBLECharacteristic* pCharacteristic;
NimBLECharacteristic* pCharacteristicWrite;
Ticker timerMediciones;
Ticker timerBoton;
bool clientBLEConectado = false;


// --- Máquina de estados ---
typedef enum {
               INICIALIZACION,
               MEDICIONES,
               C_WIFI,
               ANALISIS_REP,
               ANALISIS_SERIE,
               C_APLICACION } estadoMaq_General_t;

estadoMaq_General_t estadoMaq_General = INICIALIZACION;
// Sensor local
float ax_local, ay_local, az_local;
float gx_local, gy_local, gz_local;
float ax_ms2_local, ay_ms2_local, az_ms2_local;
float inclX_local, inclY_local, inclZ_local;

// Sensor auxiliar (valores recibidos por WiFi)
float ax_aux, ay_aux, az_aux;
float gx_aux, gy_aux, gz_aux;
float ax_ms2_aux, ay_ms2_aux, az_ms2_aux;
float inclX_aux, inclY_aux, inclZ_aux;

// Valores estándar (calibración del local)
float std_ax, std_ay, std_az;
float std_inclX, std_inclY, std_inclZ;
bool estandarCalibrado = false;
String resultadoValidacion = "";

#define PIN_BOTON 0
#define PIN_LED_R 2
#define PIN_LED_G 4
#define PIN_LED_B 5

const char* serverNameAx = "http://192.168.4.2/Ax";
const char* serverNameAy = "http://192.168.4.2/Ay";
const char* serverNameAz = "http://192.168.4.2/Az";
const char* serverNameGx = "http://192.168.4.2/Gx";
const char* serverNameGy = "http://192.168.4.2/Gy";
const char* serverNameGz = "http://192.168.4.2/Gz";
const char* ssid = "ESP32_C3_Server";
const char* password = "GRUPO3";


void setup() {
 Serial.begin(115200);

  // I2C y sensor
  Wire.begin(8,9); // SDA, SCL
  sensor.initialize();
  
  if (sensor.testConnection()) {
    Serial.println(" Sensor local ok");
  } else {
    Serial.println(" Error sensor local");
  }
  
  // Pines
  pinMode(PIN_BOTON, INPUT_PULLUP);
  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_B, OUTPUT);
  
  // WiFi
  WiFi.softAP(ssid, password);
  server.begin();
  Serial.print(" AP WiFi: ");
  Serial.println(WiFi.softAPIP());
  
  // BLE Nimble
  inicializarBLE();
  
  timerBoton.attach(1, segundosBoton);
}
void loop() {
  Maq_General();
}

void Maq_General() {
  switch (estadoMaq_General) {
    case INICIALIZACION:
      digitalWrite(PIN_LED_G, HIGH); // LED verde = calibración
      if (digitalRead(PIN_BOTON) == LOW) {
        mediciones();          // tomar una lectura
        recibirValores();   
        calibrarEstandar();    // guardar como estándar
        estandarCalibrado = true;
        estadoMaq_General = MEDICIONES;
      }
      break;

    case MEDICIONES:
      digitalWrite(PIN_LED_G, LOW);
  recibirValores();
  mediciones();
  if (estandarCalibrado) {
    compararConEstandar();
  }
  break;

    case ANALISIS_REP:
      if (digitalRead(PIN_BOTON) == LOW) {
        Serial.println("Se tocó el botón, terminó la serie.");
        estadoMaq_General = ANALISIS_SERIE;
      }
      break;

    case ANALISIS_SERIE:
      segundosBoton = 0; // reset contador
      if (digitalRead(PIN_BOTON) == LOW && segundosBoton >= 5000) {
        Serial.println("Botón presionado >5s, apagar sistema.");
        resultadoValidacion = "SERIE TERMINADA: " + String(contadorErrores) + " errores";
        Serial.println(resultadoValidacion);
        estadoMaq_General = C_APLICACION;
      }
      
      break;

    case C_APLICACION:
    enviarFeedbackBLE(resultadoValidacion);
  if (digitalRead(PIN_BOTON) == LOW) {
    estadoMaq_General = INICIALIZACION; 
  }
  break;


}
void mediciones() {
  sensor.getAcceleration(&ax_local, &ay_local, &az_local);
  sensor.getRotation(&gx_local, &gy_local, &gz_local);

  ax_ms2_local = (ax_local / 16384.0) * 9.81;
  ay_ms2_local = (ay_local / 16384.0) * 9.81;
  az_ms2_local = (az_local / 16384.0) * 9.81;

  inclX_local = atan2(ax_ms2_local, sqrt(ay_ms2_local*ay_ms2_local + az_ms2_local*az_ms2_local)) * 180.0 / PI;
  inclY_local = atan2(ay_ms2_local, sqrt(ax_ms2_local*ax_ms2_local + az_ms2_local*az_ms2_local)) * 180.0 / PI;
  inclZ_local = atan2(az_ms2_local, sqrt(ax_ms2_local*ax_ms2_local + ay_ms2_local*ay_ms2_local)) * 180.0 / PI;

  Serial.println("Lectura LOCAL:");
  Serial.println(String(ax_ms2_local) + "," + String(ay_ms2_local) + "," + String(az_ms2_local) + "," +
                 String(inclX_local) + "," + String(inclY_local) + "," + String(inclZ_local));
}


void calibrarEstandar() {
  std_ax = ax_ms2;
  std_ay = ay_ms2;
  std_az = az_ms2;
  std_inclX = inclX;
  std_inclY = inclY;
  std_inclZ = inclZ;
  Serial.println("Valores estándar guardados ✅");
}
void compararConEstandar() {
  float tol_acc = 2.0;
  float tol_incl = 5.0;
  bool correcto_local = true;
  bool correcto_aux = true;

  // Comparación LOCAL
  if (abs(ax_ms2_local - std_ax) > tol_acc) { Serial.println("Error LOCAL eje X"); correcto_local = false; }
  if (abs(ay_ms2_local - std_ay) > tol_acc) { Serial.println("Error LOCAL eje Y"); correcto_local = false; }
  if (abs(az_ms2_local - std_az) > tol_acc) { Serial.println("Error LOCAL eje Z"); correcto_local = false; }

  if (abs(inclX_local - std_inclX) > tol_incl) { Serial.println("Error LOCAL inclinación X"); correcto_local = false; }
  if (abs(inclY_local - std_inclY) > tol_incl) { Serial.println("Error LOCAL inclinación Y"); correcto_local = false; }
  if (abs(inclZ_local - std_inclZ) > tol_incl) { Serial.println("Error LOCAL inclinación Z"); correcto_local = false; }

  // Comparación AUXILIAR
  if (abs(ax_ms2_aux - std_ax) > tol_acc) { Serial.println("Error AUX eje X"); correcto_aux = false; }
  if (abs(ay_ms2_aux - std_ay) > tol_acc) { Serial.println("Error AUX eje Y"); correcto_aux = false; }
  if (abs(az_ms2_aux - std_az) > tol_acc) { Serial.println("Error AUX eje Z"); correcto_aux = false; }

  if (abs(inclX_aux - std_inclX) > tol_incl) { Serial.println("Error AUX inclinación X"); correcto_aux = false; }
  if (abs(inclY_aux - std_inclY) > tol_incl) { Serial.println("Error AUX inclinación Y"); correcto_aux = false; }
  if (abs(inclZ_aux - std_inclZ) > tol_incl) { Serial.println("Error AUX inclinación Z"); correcto_aux = false; }

  if (correcto_local && correcto_aux) {
    resultadoValidacion = "CORRECTO";
    Serial.println("Movimiento CORRECTO");
  } else {
    resultadoValidacion = "INCORRECTO";
    Serial.println("Movimiento INCORRECTO");
  }
}

//  función para pedir valores al AUXILIAR
void recibirValores() {
  String ax = httpGETRequest(serverNameAx);
  String ay = httpGETRequest(serverNameAy);
  String az = httpGETRequest(serverNameAz);

  ax_ms2_aux = ax.toFloat();
  ay_ms2_aux = ay.toFloat();
  az_ms2_aux = az.toFloat();

  inclX_aux = atan2(ax_ms2_aux, sqrt(ay_ms2_aux*ay_ms2_aux + az_ms2_aux*az_ms2_aux)) * 180.0 / PI;
  inclY_aux = atan2(ay_ms2_aux, sqrt(ax_ms2_aux*ax_ms2_aux + az_ms2_aux*az_ms2_aux)) * 180.0 / PI;
  inclZ_aux = atan2(az_ms2_aux, sqrt(ax_ms2_aux*ax_ms2_aux + ay_ms2_aux*ay_ms2_aux)) * 180.0 / PI;

  Serial.println("Valores AUXILIAR recibidos:");
  Serial.println(String(ax_ms2_aux) + "," + String(ay_ms2_aux) + "," + String(az_ms2_aux));
}
void inicializarBLE() {
  NimBLEDevice::init("Techeck_ESP32");
  NimBLEDevice::setMTU(517);

  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new MiServerCallback());

  NimBLEService* pService = pServer->createService("11111111-1111-1111-1111-111111111111");

  pCharacteristic = pService->createCharacteristic(
    "22222222-2222-2222-2222-222222222222",
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
  );

  pCharacteristic->setValue("ESP32 listo");
  pService->start();

  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(pService->getUUID());
  pAdvertising->setScanResponse(true);
  NimBLEDevice::startAdvertising();

  Serial.println("BLE Nimble listo");
}
void segundosBoton() {
  segundosBoton++;
}
void enviarFeedbackBLE(const String& mensaje) {
  if (clientBLEConectado && pCharacteristic != nullptr) {
    pCharacteristic->setValue(mensaje.c_str());
    pCharacteristic->notify();
    Serial.println("BLE feedback: " + mensaje);
  }
}

String httpGETRequest(const char* serverName) {
  HTTPClient http;
  http.begin(serverName);
  int httpResponseCode = http.GET();
  String payload = "--";
  if (httpResponseCode > 0) {
    payload = http.getString();
  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  http.end();
  return payload;
}
