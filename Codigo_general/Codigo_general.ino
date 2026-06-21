#include <MPU6050.h>
#include "Wire.h"
#include "WiFi.h"
#include <HTTPClient.h>
#include <WebServer.h>
#include <Ticker.h>
#include <NimBLEDevice.h>

MPU6050 sensor;
NimBLEServer* pServer;
NimBLECharacteristic* pCharacteristic;
Ticker timerBoton;
bool clientBLEConectado = false;
int contadorErrores = 0;

// Cronómetro para no saturar el WiFi
unsigned long tiempoUltimaMedicion = 0;
const unsigned long INTERVALO_MEDICION = 500; // Medio segundo

// --- Máquina de estados ---
typedef enum {
  INICIALIZACION,
  MEDICIONES,
  C_WIFI,
  ANALISIS_REP,
  ANALISIS_SERIE,
  C_APLICACION
} estadoMaq_General_t;

estadoMaq_General_t estadoMaq_General = INICIALIZACION;

// Sensor local 
int16_t ax_local, ay_local, az_local;
int16_t gx_local, gy_local, gz_local;

// Los valores calculados
float ax_ms2_local, ay_ms2_local, az_ms2_local;
float inclX_local, inclY_local, inclZ_local;

// Sensor auxiliar 
float ax_aux, ay_aux, az_aux;
float gx_aux, gy_aux, gz_aux;
float ax_ms2_aux, ay_ms2_aux, az_ms2_aux;
float inclX_aux, inclY_aux, inclZ_aux;

// Valores estándar
float std_ax, std_ay, std_az;
float std_inclX, std_inclY, std_inclZ;
bool estandarCalibrado = false;
String resultadoValidacion = "";
int segundosBoton = 0;

#define PIN_BOTON 0
#define PIN_LED_R 2
#define PIN_LED_G 4
#define PIN_LED_B 5

const char* serverNameAx = "http://192.168.4.2/Ax";
const char* serverNameAy = "http://192.168.4.2/Ay";
const char* serverNameAz = "http://192.168.4.2/Az";

const char* ssid = "ESP32_C3_Server";
const char* password = "GRUPO3";

class MiServerCallback: public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) {
      clientBLEConectado = true;
    }
    void onDisconnect(NimBLEServer* pServer) {
      clientBLEConectado = false;
    }
};

void setup() {
  Serial.begin(115200);
  delay(1000); 

  // MODO SIMULACIÓN
  // Wire.begin(8, 9);
  // sensor.initialize();
  Serial.println("\n--- INICIANDO SISTEMA ---");
  Serial.println("MODO PRUEBA: Sensor MPU6050 desactivado/simulado");

  pinMode(PIN_BOTON, INPUT_PULLUP);
  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_B, OUTPUT);

  // --- CONFIGURACIÓN SEGURA DE WIFI (Sin AsyncWebServer) ---
  Serial.println("Iniciando AP WiFi...");
  WiFi.mode(WIFI_AP); 
  WiFi.softAP(ssid, password);
  delay(500); 
  
  Serial.print("AP WiFi iniciado correctamente. IP: ");
  Serial.println(WiFi.softAPIP());

  inicializarBLE();
  
  timerBoton.attach(1, funcionTimerBoton); 
}

void loop() {
  Maq_General();

  if (Serial.available() > 0) {
    String mensajeTerminal = Serial.readStringUntil('\n');
    mensajeTerminal.trim();
    if (mensajeTerminal.length() > 0) {
      enviarFeedbackBLE(mensajeTerminal);
      Serial.println("-> Enviado por BLE: " + mensajeTerminal);
    }
  }

  delay(10); 
}

void Maq_General() {
  switch (estadoMaq_General) {
    case INICIALIZACION:
      digitalWrite(PIN_LED_G, HIGH);  
      if (digitalRead(PIN_BOTON) == LOW) {
        mediciones();  
        recibirValores();
        calibrarEstandar();  
        estandarCalibrado = true;
        
        delay(300); 
        estadoMaq_General = MEDICIONES;
      }
      break;

    case MEDICIONES:
      digitalWrite(PIN_LED_G, LOW);
      
      if (millis() - tiempoUltimaMedicion >= INTERVALO_MEDICION) {
        tiempoUltimaMedicion = millis(); 
        
        recibirValores();
        mediciones();
        if (estandarCalibrado) {
          compararConEstandar();
        }
      }

      if (digitalRead(PIN_BOTON) == LOW) {
        delay(300); 
        estadoMaq_General = ANALISIS_REP;
      }
      break;

    case ANALISIS_REP:
      if (digitalRead(PIN_BOTON) == LOW) {
        Serial.println("Se tocó el botón, terminó la serie.");
        delay(300);
        estadoMaq_General = ANALISIS_SERIE;
      }
      break;

    case ANALISIS_SERIE:
      segundosBoton = 0;  
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
        delay(300);
        estadoMaq_General = INICIALIZACION;
      }
      break;
  }
}

void mediciones() {
  ax_local = 0;
  ay_local = 0;
  az_local = 0; 
  gx_local = 0;
  gy_local = 0;
  gz_local = 0;

  ax_ms2_local = (ax_local / 16384.0) * 9.81;
  ay_ms2_local = (ay_local / 16384.0) * 9.81;
  az_ms2_local = (az_local / 16384.0) * 9.81;

  inclX_local = atan2(ax_ms2_local, sqrt(ay_ms2_local * ay_ms2_local + az_ms2_local * az_ms2_local)) * 180.0 / PI;
  inclY_local = atan2(ay_ms2_local, sqrt(ax_ms2_local * ax_ms2_local + az_ms2_local * az_ms2_local)) * 180.0 / PI;
  inclZ_local = atan2(az_ms2_local, sqrt(ax_ms2_local * ax_ms2_local + ay_ms2_local * ay_ms2_local)) * 180.0 / PI;

  Serial.println("Lectura LOCAL SIMULADA:");
  Serial.println(String(ax_ms2_local) + "," + String(ay_ms2_local) + "," + String(az_ms2_local) + "," + String(inclX_local) + "," + String(inclY_local) + "," + String(inclZ_local));
}

void calibrarEstandar() {
  std_ax = ax_ms2_local;
  std_ay = ay_ms2_local;
  std_az = az_ms2_local;
  std_inclX = inclX_local;
  std_inclY = inclY_local;
  std_inclZ = inclZ_local;
  Serial.println("Valores estándar guardados");
}

void compararConEstandar() {
  float tol_acc = 2.0;
  float tol_incl = 2.0;
  bool correcto_local = true;
  bool correcto_aux = true;

  if (abs(ax_ms2_local - std_ax) > tol_acc) correcto_local = false;
  if (abs(ay_ms2_local - std_ay) > tol_acc) correcto_local = false;
  if (abs(az_ms2_local - std_az) > tol_acc) correcto_local = false;
  if (abs(inclX_local - std_inclX) > tol_incl) correcto_local = false;
  if (abs(inclY_local - std_inclY) > tol_incl) correcto_local = false;
  if (abs(inclZ_local - std_inclZ) > tol_incl) correcto_local = false;

  if (abs(ax_ms2_aux - std_ax) > tol_acc) correcto_aux = false;
  if (abs(ay_ms2_aux - std_ay) > tol_acc) correcto_aux = false;
  if (abs(az_ms2_aux - std_az) > tol_acc) correcto_aux = false;
  if (abs(inclX_aux - std_inclX) > tol_incl) correcto_aux = false;
  if (abs(inclY_aux - std_inclY) > tol_incl) correcto_aux = false;
  if (abs(inclZ_aux - std_inclZ) > tol_incl) correcto_aux = false;

  if (correcto_local && correcto_aux) {
    resultadoValidacion = "CORRECTO";
    Serial.println("Movimiento CORRECTO");
  } else {
    resultadoValidacion = "INCORRECTO";
    Serial.println("Movimiento INCORRECTO");
  }
}

void recibirValores() {
  String ax = httpGETRequest(serverNameAx);
  String ay = httpGETRequest(serverNameAy);
  String az = httpGETRequest(serverNameAz);

  ax_ms2_aux = ax.toFloat();
  ay_ms2_aux = ay.toFloat();
  az_ms2_aux = az.toFloat();

  inclX_aux = atan2(ax_ms2_aux, sqrt(ay_ms2_aux * ay_ms2_aux + az_ms2_aux * az_ms2_aux)) * 180.0 / PI;
  inclY_aux = atan2(ay_ms2_aux, sqrt(ax_ms2_aux * ax_ms2_aux + az_ms2_aux * az_ms2_aux)) * 180.0 / PI;
  inclZ_aux = atan2(az_ms2_aux, sqrt(ax_ms2_aux * ax_ms2_aux + ay_ms2_aux * ay_ms2_aux)) * 180.0 / PI;
}

void inicializarBLE() {
  NimBLEDevice::init("Techeck_ESP32");
  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new MiServerCallback());
  NimBLEService* pService = pServer->createService("11111111-1111-1111-1111-111111111111");
  pCharacteristic = pService->createCharacteristic(
    "22222222-2222-2222-2222-222222222222",
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    
  pCharacteristic->setValue("ESP32 listo");
  pService->start();
  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(pService->getUUID());
  NimBLEDevice::startAdvertising();
}

void funcionTimerBoton() {
  segundosBoton++;
}

void enviarFeedbackBLE(const String& mensaje) {
  // Sacamos el clientBLEConectado para forzar el envío siempre
  if (pCharacteristic != nullptr) {
    pCharacteristic->setValue(mensaje.c_str());
    pCharacteristic->notify();
    Serial.println("-> Intentando notificar por BLE: " + mensaje);
  }
}

String httpGETRequest(const char* serverName) {
  // Si no hay nadie conectado al AP, devolvemos 0.0 directo
  if (WiFi.softAPgetStationNum() == 0) {
    return "0.0";
  }

  HTTPClient http;
  http.begin(serverName);
  int httpResponseCode = http.GET();
  String payload = "0.0"; 
  if (httpResponseCode > 0) {
    payload = http.getString();
  }
  http.end();
  return payload;
}