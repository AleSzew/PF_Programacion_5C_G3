#include <MPU6050.h>
#include "Wire.h"
#include "WiFi.h"
#include <HTTPClient.h>
#include <WebServer.h>
#include <Ticker.h>
#include <NimBLEDevice.h>

struct Ejercicio;

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

int16_t ax_local, ay_local, az_local;
float ax_ms2_local, ay_ms2_local, az_ms2_local, inclX_local, inclY_local, inclZ_local;

String resultadoValidacion = "";
int segundosBoton = 0;

#define PIN_BOTON 1
#define PIN_LED_R 9
#define PIN_LED_G 20
#define PIN_LED_B 10

const char* ssid = "ESP32_C3_Server";
const char* password = "GRUPO3";
int codigo; // llega por BLE, elige qué ejercicio calibrar/evaluar

// ============================================================
// OFFSET GLOBAL DEL SENSOR (se calibra UNA sola vez en setup())
// ============================================================
float offset_X, offset_Y, offset_Z;

void calibrarOffsetGlobal() {
  Serial.println("Calibrando sensor, no muevas el dispositivo...");
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
  Serial.println("Sensor calibrado.");
}

// Lee el sensor local y actualiza inclX_local/Y/Z ya con offset aplicado.
// Reemplaza a la vieja mediciones(), que no restaba offset.
void mediciones() {
  sensor.getAcceleration(&ax_local, &ay_local, &az_local);
  ax_ms2_local = (ax_local / 16384.0) * 9.81;
  ay_ms2_local = (ay_local / 16384.0) * 9.81;
  az_ms2_local = (az_local / 16384.0) * 9.81;

  inclX_local = atan2(ax_ms2_local, sqrt(ay_ms2_local * ay_ms2_local + az_ms2_local * az_ms2_local)) * 180.0 / PI - offset_X;
  inclY_local = atan2(ay_ms2_local, sqrt(ax_ms2_local * ax_ms2_local + az_ms2_local * az_ms2_local)) * 180.0 / PI - offset_Y;
  inclZ_local = atan2(az_ms2_local, sqrt(ax_ms2_local * ax_ms2_local + ay_ms2_local * ay_ms2_local)) * 180.0 / PI - offset_Z;
}

float valorEje(uint8_t eje) {
  if (eje == 0) return inclX_local;
  if (eje == 1) return inclY_local;
  return inclZ_local;
}

// ============================================================
// DEFINICIÓN DE EJERCICIO
// ============================================================
struct Ejercicio {
  const char* nombre;
  uint8_t ejePrincipal;
  float minAngulo;
  float maxAngulo;
  uint8_t ejeSecundario;
  float centroSecundario;
  float toleranciaSecundario;
};

// Un ejercicio por cada "codigo" que puede llegar por BLE (1 a 5).
// Los ejes de cada uno hay que definirlos probando con Serial
// (igual que hicimos con el curl): cuál eje sube/baja con el
// movimiento (principal) y cuál se mantiene estable si se hace
// bien (secundario). minAngulo/maxAngulo/centroSecundario/tolerancia
// NO se tocan a mano: los llena calibrarEjercicio().
#define CANT_EJERCICIOS 5
Ejercicio ejercicios[CANT_EJERCICIOS] = {
  { "Curl de biceps",     1, 0, 0,  0, 0, 0 }, // codigo 1: principal Y, secundario X
  { "Ejercicio 2",        0, 0, 0,  1, 0, 0 }, // codigo 2: PLACEHOLDER, ajustar ejes
  { "Ejercicio 3",        0, 0, 0,  1, 0, 0 }, // codigo 3: PLACEHOLDER
  { "Ejercicio 4",        0, 0, 0,  1, 0, 0 }, // codigo 4: PLACEHOLDER
  { "Ejercicio 5",        0, 0, 0,  1, 0, 0 }  // codigo 5: PLACEHOLDER
};

Ejercicio* ejercicioActual = &ejercicios[0]; // por defecto, curl de biceps

// ============================================================
// CALIBRACIÓN POR EJERCICIO (repetición de muestra, bloqueante)
// ============================================================
void calibrarEjercicio(Ejercicio &ej, unsigned long duracionMs) {
  Serial.print("Calibrando: ");
  Serial.println(ej.nombre);
  Serial.println("Hace UNA repeticion completa, lenta y CORRECTA ahora.");

  float minV = 999, maxV = -999;
  float sumaSecundario = 0;
  int cantidadLecturas = 0;
  float minSecundario = 999, maxSecundario = -999;

  unsigned long inicio = millis();
  while (millis() - inicio < duracionMs) {
    mediciones();
    float v = valorEje(ej.ejePrincipal);
    float vs = valorEje(ej.ejeSecundario);

    if (v < minV) minV = v;
    if (v > maxV) maxV = v;

    sumaSecundario += vs;
    cantidadLecturas++;
    if (vs < minSecundario) minSecundario = vs;
    if (vs > maxSecundario) maxSecundario = vs;

    delay(50);
  }

  ej.minAngulo = minV;
  ej.maxAngulo = maxV;
  ej.centroSecundario = sumaSecundario / cantidadLecturas;
  float variacionVista = (maxSecundario - minSecundario) / 2.0;
  ej.toleranciaSecundario = variacionVista + 5.0;

  Serial.print("Rango principal: "); Serial.print(minV); Serial.print(" a "); Serial.println(maxV);
  Serial.print("Centro secundario: "); Serial.println(ej.centroSecundario);
  Serial.print("Tolerancia secundario: "); Serial.println(ej.toleranciaSecundario);
}

// Elige el ejercicio según el "codigo" recibido por BLE y lo calibra.
void calibrarEstandar() {
  if (codigo < 1 || codigo > CANT_EJERCICIOS) {
    Serial.println("Codigo de ejercicio invalido, uso el default.");
    ejercicioActual = &ejercicios[0];
  } else {
    ejercicioActual = &ejercicios[codigo - 1];
  }
  calibrarEjercicio(*ejercicioActual, 4000);
}

// ============================================================
// MÁQUINA DE ESTADOS DE LA REPETICIÓN
// ============================================================
enum FaseRep { REPOSO, MEDIO, FIN };
FaseRep faseRep = REPOSO;
unsigned long tInicioRep = 0;

const unsigned long T_MIN = 200;
const unsigned long T_MAX = 5000;
const float MARGEN = 0.15;

// Evalúa la repetición en curso. Reemplaza a compararConEstandar().
void evaluarRepeticion(Ejercicio &ej) {
  float v  = valorEje(ej.ejePrincipal);
  float vs = valorEje(ej.ejeSecundario);
  bool posturaOK = abs(vs - ej.centroSecundario) <= ej.toleranciaSecundario;

  float rango  = ej.maxAngulo - ej.minAngulo;
  float inicio = ej.minAngulo + rango * MARGEN;
  float centro = ej.minAngulo + rango * 0.50;
  float final_ = ej.maxAngulo - rango * MARGEN;

  switch (faseRep) {
    case REPOSO:
      if (v <= inicio) {
        faseRep = MEDIO;
        tInicioRep = millis();
      }
      break;

    case MEDIO:
      if (!posturaOK) {
        resultadoValidacion = "MAL (postura)";
        contadorErrores++;
        faseRep = REPOSO;
        break;
      }
      if (abs(v - centro) <= rango * MARGEN) {
        faseRep = FIN;
      } else if (millis() - tInicioRep > T_MAX) {
        resultadoValidacion = "MAL (tiempo)";
        contadorErrores++;
        faseRep = REPOSO;
      }
      break;

    case FIN: {
      if (!posturaOK) {
        resultadoValidacion = "MAL (postura)";
        contadorErrores++;
        faseRep = REPOSO;
        break;
      }
      unsigned long duracion = millis() - tInicioRep;
      if (v >= final_) {
        if (duracion >= T_MIN) {
          resultadoValidacion = "BIEN";
        } else {
          resultadoValidacion = "MAL (muy rapido)";
          contadorErrores++;
        }
        faseRep = REPOSO;
      } else if (duracion > T_MAX) {
        resultadoValidacion = "MAL (tiempo)";
        contadorErrores++;
        faseRep = REPOSO;
      }
      break;
    }
  }
  Serial.println(resultadoValidacion); // debug, después se manda por BLE
}

// ============================================================
// BLE (igual que antes)
// ============================================================
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

// ============================================================
// BOTÓN (antirrebote, igual que antes)
// ============================================================
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

void funcionTimerBoton() {
  if (digitalRead(PIN_BOTON) == LOW) segundosBoton++;
  else segundosBoton = 0;
}
void funcionTimerAntirrebote() { msBoton++; }
void funcionTimerMedicion() { flagMedicion = true; }

// ============================================================
// MÁQUINA GENERAL
// ============================================================
void Maq_General() {
  switch (estadoMaq_General) {
    case INICIALIZACION:
      digitalWrite(PIN_LED_G, HIGH);
      if (flagBoton) {
        flagBoton = false;
        calibrarEstandar();       // calibra el ejercicio elegido por BLE
        faseRep = REPOSO;         // arranca la máquina de repeticiones limpia
        contadorErrores = 0;
        estadoMaq_General = MEDICIONES;
      }
      if (Serial.available() > 0) {
        if (Serial.readStringUntil('\n') == "pasar estado") estadoMaq_General = MEDICIONES;
      }
      break;

    case MEDICIONES:
      digitalWrite(PIN_LED_G, LOW);
      if (flagMedicion) {
        flagMedicion = false;
        mediciones();
        evaluarRepeticion(*ejercicioActual);
      }
      if (flagBoton) {
        flagBoton = false;
        estadoMaq_General = ANALISIS_SERIE;
      }
      if (Serial.available() > 0) {
        if (Serial.readStringUntil('\n') == "pasar estado") estadoMaq_General = ANALISIS_SERIE;
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
        if (Serial.readStringUntil('\n') == "pasar estado") estadoMaq_General = C_APLICACION;
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

  calibrarOffsetGlobal(); // una sola vez, con el sensor quieto al arrancar

  timerBoton.attach(1, funcionTimerBoton);
  timerAntirrebote.attach_ms(1, funcionTimerAntirrebote);
  timerMedicion.attach_ms(INTERVALO_MEDICION, funcionTimerMedicion);
}

void loop() {
  maquinaAntirrebote();
  Maq_General();
}