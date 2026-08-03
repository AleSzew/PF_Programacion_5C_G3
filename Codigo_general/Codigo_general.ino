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
Ticker timerAntirrebote; 
Ticker timerMedicion;

int contadorErrores = 0;
bool flagMedicion = false;
int INTERVALO_MEDICION = 100;

typedef enum { INICIALIZACION, MEDICIONES, ANALISIS_SERIE, C_APLICACION } estadoMaq_General_t;
estadoMaq_General_t estadoMaq_General = INICIALIZACION;

typedef enum { ESPERA, CONFIRMACION, LIBERACION } estadoAntirrebote_t;
estadoAntirrebote_t estadoBoton = ESPERA;
int msBoton = 0;
bool flagBoton = false;
#define T_REBOTE 10

int16_t ax_local, ay_local, az_local, gx_local, gy_local, gz_local;
float ax_ms2_local, ay_ms2_local, az_ms2_local, inclX_local, inclY_local, inclZ_local;

float ax_aux, ay_aux, az_aux, gx_aux, gy_aux, gz_aux;
float ax_ms2_aux, ay_ms2_aux, az_ms2_aux, inclX_aux, inclY_aux, inclZ_aux;

float std_ax, std_ay, std_az, std_inclX, std_inclY, std_inclZ;
bool estandarCalibrado = false;
String resultadoValidacion = "";
int segundosBoton = 0;

#define PIN_BOTON 1
#define PIN_LED_R 9
#define PIN_LED_G 20
#define PIN_LED_B 10

const char* serverNameAx = "http://192.168.4.2/Ax";
const char* serverNameAy = "http://192.168.4.2/Ay";
const char* serverNameAz = "http://192.168.4.2/Az";

const char* ssid = "ESP32_C3_Server";
const char* password = "GRUPO3";
int codigo;

class MiServerCallbacks: public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
    Serial.println("BLE Conectado");
  };
  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
    Serial.println("BLE Desconectado");
    NimBLEDevice::startAdvertising(); 
  }
};

class MiCharacteristicCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override { 
    std::string valor = pCharacteristic->getValue();
    if (valor.length() > 0) {
      codigo = atoi(valor.c_str());
      Serial.print("ID recibido: ");
      Serial.println(codigo);
    }
  }
};

void setup() {
  Serial.begin(115200);
  Wire.begin(6, 7);
  sensor.initialize();
  
  pinMode(PIN_BOTON, INPUT_PULLUP);
  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_B, OUTPUT);

  WiFi.mode(WIFI_AP); 
  WiFi.softAP(ssid, password);
  delay(500); 

  inicializarBLE();
  
  timerBoton.attach(1, funcionTimerBoton); 
  timerAntirrebote.attach_ms(1, funcionTimerAntirrebote);
  timerMedicion.attach_ms(INTERVALO_MEDICION, funcionTimerMedicion);
}

void loop() {
  maquinaAntirrebote();
  Maq_General();
}

void maquinaAntirrebote() {
  bool lecturaBoton = digitalRead(PIN_BOTON);
  switch (estadoBoton) {
    case ESPERA:
      if (lecturaBoton == LOW) { msBoton = 0; estadoBoton = CONFIRMACION; }
      break;
    case CONFIRMACION:
      if (msBoton >= T_REBOTE) {
        if (lecturaBoton == LOW) estadoBoton = LIBERACION;
        else estadoBoton = ESPERA; 
      }
      break;
    case LIBERACION:
      if (lecturaBoton == HIGH) { flagBoton = true; estadoBoton = ESPERA; }
      break;
  }
}

void Maq_General() {
  switch (estadoMaq_General) {
    case INICIALIZACION:
      digitalWrite(PIN_LED_G, HIGH);  
      if (flagBoton) {
        flagBoton = false; 
        mediciones();  
        recibirValores();
        calibrarEstandar();  
        estandarCalibrado = true;
        estadoMaq_General = MEDICIONES;
      }
      if (Serial.available() > 0) {
        if(Serial.readStringUntil('\n') == "pasar estado") estadoMaq_General = MEDICIONES;
      }
      break;

    case MEDICIONES:
      digitalWrite(PIN_LED_G, LOW);
      if (flagMedicion) {
        flagMedicion = false; 
        recibirValores();
        mediciones();
        if (estandarCalibrado) compararConEstandar();
      }
      if (flagBoton) {
        flagBoton = false;
        estadoMaq_General = ANALISIS_SERIE;
      }
      if (Serial.available() > 0) {
        if(Serial.readStringUntil('\n') == "pasar estado") estadoMaq_General = ANALISIS_SERIE;
      }
      break;

    case ANALISIS_SERIE:
      if (segundosBoton >= 5) {
        resultadoValidacion = "SERIE TERMINADA: " + String(contadorErrores) + " errores";
        segundosBoton = 0; 
        flagBoton = false; 
        estadoMaq_General = C_APLICACION;
      }
      if (Serial.available() > 0) {
        if(Serial.readStringUntil('\n') == "pasar estado") estadoMaq_General = C_APLICACION;
      }
      break;

    case C_APLICACION:
      enviarFeedbackBLE(resultadoValidacion);
      if (flagBoton) {
        flagBoton = false;
        estadoMaq_General = INICIALIZACION;
      }
      break;
  }
}

void mediciones() {
  sensor.getAcceleration(&ax_local, &ay_local, &az_local);
  sensor.getRotation(&gx_local, &gy_local, &gz_local);

  ax_ms2_local = (ax_local / 16384.0) * 9.81;
  ay_ms2_local = (ay_local / 16384.0) * 9.81;
  az_ms2_local = (az_local / 16384.0) * 9.81;

  inclX_local = atan2(ax_ms2_local, sqrt(ay_ms2_local * ay_ms2_local + az_ms2_local * az_ms2_local)) * 180.0 / PI;
  inclY_local = atan2(ay_ms2_local, sqrt(ax_ms2_local * ax_ms2_local + az_ms2_local * az_ms2_local)) * 180.0 / PI;
  inclZ_local = atan2(az_ms2_local, sqrt(ax_ms2_local * ax_ms2_local + ay_ms2_local * ay_ms2_local)) * 180.0 / PI;
}

void calibrarEstandar() {
  if(codigo == 1){
    
  }
  if(codigo == 2){
    
  }
  if(codigo == 3){
    
  }
  if(codigo == 4){
    
  }
  if(codigo == 5){
    
  }
}

void compararConEstandar() {
  float tol_acc = 2.0;
  float tol_incl = 2.0;
  bool correcto_local = true;
  bool correcto_aux = true;

  if (abs(ax_ms2_local - std_ax) > tol_acc || abs(ay_ms2_local - std_ay) > tol_acc || abs(az_ms2_local - std_az) > tol_acc) correcto_local = false;
  if (abs(inclX_local - std_inclX) > tol_incl || abs(inclY_local - std_inclY) > tol_incl || abs(inclZ_local - std_inclZ) > tol_incl) correcto_local = false;

  if (abs(ax_ms2_aux - std_ax) > tol_acc || abs(ay_ms2_aux - std_ay) > tol_acc || abs(az_ms2_aux - std_az) > tol_acc) correcto_aux = false;
  if (abs(inclX_aux - std_inclX) > tol_incl || abs(inclY_aux - std_inclY) > tol_incl || abs(inclZ_aux - std_inclZ) > tol_incl) correcto_aux = false;

  if (correcto_local && correcto_aux) {
    resultadoValidacion = "CORRECTO";
  } else {
    resultadoValidacion = "INCORRECTO";
    contadorErrores++;
  }
}

void recibirValores() {
  ax_ms2_aux = httpGETRequest(serverNameAx).toFloat();
  ay_ms2_aux = httpGETRequest(serverNameAy).toFloat();
  az_ms2_aux = httpGETRequest(serverNameAz).toFloat();

  inclX_aux = atan2(ax_ms2_aux, sqrt(ay_ms2_aux * ay_ms2_aux + az_ms2_aux * az_ms2_aux)) * 180.0 / PI;
  inclY_aux = atan2(ay_ms2_aux, sqrt(ax_ms2_aux * ax_ms2_aux + az_ms2_aux * az_ms2_aux)) * 180.0 / PI;
  inclZ_aux = atan2(az_ms2_aux, sqrt(ax_ms2_aux * ax_ms2_aux + ay_ms2_aux * ay_ms2_aux)) * 180.0 / PI;
}

void inicializarBLE() {
  NimBLEDevice::init("Techeck_V2"); 
  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new MiServerCallbacks());
  
  NimBLEService* pService = pServer->createService("11111111-1111-1111-1111-111111111111");
  
  pCharacteristic = pService->createCharacteristic(
    "22222222-2222-2222-2222-222222222222",
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::NOTIFY);
    
  pCharacteristic->setCallbacks(new MiCharacteristicCallbacks());
  pCharacteristic->setValue("ESP32 listo");
  pService->start();
  
  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(pService->getUUID());
  NimBLEDevice::startAdvertising();
}

void enviarFeedbackBLE(const String& mensaje) {
  if (pCharacteristic != nullptr) {
    pCharacteristic->setValue(mensaje.c_str());
    pCharacteristic->notify();
  }
}

void funcionTimerBoton() {
  if (digitalRead(PIN_BOTON) == LOW) segundosBoton++;
  else segundosBoton = 0;
}

void funcionTimerAntirrebote() { msBoton++; }
void funcionTimerMedicion() { flagMedicion = true; }

String httpGETRequest(const char* serverName) {
  if (WiFi.softAPgetStationNum() == 0) return "0.0";
  
  HTTPClient http;
  http.begin(serverName);
  int httpResponseCode = http.GET();
  String payload = "0.0"; 
  
  if (httpResponseCode > 0) payload = http.getString();
  http.end();
  return payload;
}
