#include <MPU6050.h>
#include "Wire.h"
#include "WiFi.h"
#include <HTTPClient.h>
#include <Ticker.h>
#include <NimBLEDevice.h>
#include "I2Cdev.h"

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

typedef enum { INICIALIZACION,
               MEDICIONES,
               ANALISIS_SERIE,
               C_APLICACION } estadoMaq_General_t;
estadoMaq_General_t estadoMaq_General = INICIALIZACION;

typedef enum { ESPERA,
               CONFIRMACION,
               LIBERACION } estadoAntirrebote_t;
estadoAntirrebote_t estadoBoton = ESPERA;
int msBoton = 0;
bool flagBoton = false;
#define T_REBOTE 10

int16_t ax_local, ay_local, az_local;
float ax_ms2_local, ay_ms2_local, az_ms2_local, inclX_local, inclY_local, inclZ_local;

// Ángulos de los 2 sensores auxiliares, ya recibidos por WiFi.
// El auxiliar manda sus 3 ejes YA con su propio offset aplicado.
float inclX_aux1, inclY_aux1, inclZ_aux1;
float inclX_aux2, inclY_aux2, inclZ_aux2;
bool aux1_ok = false;  // si el último request falló, no confiamos en el dato viejo
bool aux2_ok = false;

String resultadoValidacion = "";
int segundosBoton = 0;

#define PIN_BOTON 1
#define PIN_LED_R 9
#define PIN_LED_G 20
#define PIN_LED_B 10

const char* ssid = "ESP32_C3_Server";
const char* password = "GRUPO3";
int codigo;

// IPs fijas que le vamos a asignar a cada auxiliar (ver código auxiliar más adelante)
const char* serverAux1 = "http://192.168.4.2/datos";
const char* serverAux2 = "http://192.168.4.3/datos";

// ============================================================
// OFFSET GLOBAL DEL SENSOR LOCAL
// ============================================================
float offset_X, offset_Y, offset_Z;

void calibrarOffsetGlobal() {
  Serial.println("Calibrando sensor local, no muevas el dispositivo...");
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
  Serial.println("Sensor local calibrado.");
}

// Lee el sensor local (con offset ya aplicado)
void mediciones() {
  sensor.getAcceleration(&ax_local, &ay_local, &az_local);
  ax_ms2_local = (ax_local / 16384.0) * 9.81;
  ay_ms2_local = (ay_local / 16384.0) * 9.81;
  az_ms2_local = (az_local / 16384.0) * 9.81;

  inclX_local = atan2(ax_ms2_local, sqrt(ay_ms2_local * ay_ms2_local + az_ms2_local * az_ms2_local)) * 180.0 / PI - offset_X;
  inclY_local = atan2(ay_ms2_local, sqrt(ax_ms2_local * ax_ms2_local + az_ms2_local * az_ms2_local)) * 180.0 / PI - offset_Y;
  inclZ_local = atan2(az_ms2_local, sqrt(ax_ms2_local * ax_ms2_local + ay_ms2_local * ay_ms2_local)) * 180.0 / PI - offset_Z;
}

// ============================================================
// COMUNICACIÓN CON LOS AUXILIARES
// ============================================================
// Pide "x,y,z" en UN solo request (evita 3 lecturas de instantes distintos).
// Devuelve false si falló (auxiliar apagado, timeout, etc).
bool pedirDatosAux(const char* url, float& x, float& y, float& z) {
  if (WiFi.softAPgetStationNum() == 0) return false;

  HTTPClient http;
  http.setTimeout(300);
  http.begin(url);
  int codigoHttp = http.GET();

  if (codigoHttp != 200) {
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  if (payload == "NOMIDIENDO") return false;  // no está midiendo, no es un dato válido

  int c1 = payload.indexOf(',');
  int c2 = payload.indexOf(',', c1 + 1);
  if (c1 < 0 || c2 < 0) return false;

  x = payload.substring(0, c1).toFloat();
  y = payload.substring(c1 + 1, c2).toFloat();
  z = payload.substring(c2 + 1).toFloat();
  return true;
}

void recibirValoresAux() {
  aux1_ok = pedirDatosAux(serverAux1, inclX_aux1, inclY_aux1, inclZ_aux1);
  aux2_ok = pedirDatosAux(serverAux2, inclX_aux2, inclY_aux2, inclZ_aux2);

  if (!aux1_ok) Serial.println("Aviso: no se pudo leer auxiliar 1");
  if (!aux2_ok) Serial.println("Aviso: no se pudo leer auxiliar 2");
}

// Devuelve el ángulo de cualquiera de los 3 sensores.
// sensor: 0=local, 1=aux1, 2=aux2 | eje: 0=X, 1=Y, 2=Z
float leerAngulo(uint8_t sensorId, uint8_t eje) {
  float x, y, z;
  if (sensorId == 0) {
    x = inclX_local;
    y = inclY_local;
    z = inclZ_local;
  } else if (sensorId == 1) {
    x = inclX_aux1;
    y = inclY_aux1;
    z = inclZ_aux1;
  } else {
    x = inclX_aux2;
    y = inclY_aux2;
    z = inclZ_aux2;
  }

  if (eje == 0) return x;
  if (eje == 1) return y;
  return z;
}

// ============================================================
// DEFINICIÓN DE EJERCICIO (ahora con sensores de postura)
// ============================================================
#define CANT_POSTURA 2

struct Ejercicio {
  const char* nombre;

  uint8_t sensorPrincipal;  // 0=local, 1=aux1, 2=aux2
  uint8_t ejePrincipal;
  float minAngulo;
  float maxAngulo;

  uint8_t sensorPostura[CANT_POSTURA];
  uint8_t ejePostura[CANT_POSTURA];
  float centroPostura[CANT_POSTURA];
  float toleranciaPostura[CANT_POSTURA];
};

// PLACEHOLDER: hay que definir con pruebas reales qué sensor/eje
// es el principal y cuáles son de postura para cada ejercicio.
#define CANT_EJERCICIOS 5
Ejercicio ejercicios[CANT_EJERCICIOS] = {
  { "Curl de biceps", 0, 1, 0, 0, { 1, 2 }, { 1, 1 }, { 0, 0 }, { 0, 0 } },  // principal: local eje Y | postura: aux1 y aux2 eje Y
  { "Ejercicio 2", 0, 0, 0, 0, { 1, 2 }, { 1, 1 }, { 0, 0 }, { 0, 0 } },
  { "Ejercicio 3", 0, 0, 0, 0, { 1, 2 }, { 1, 1 }, { 0, 0 }, { 0, 0 } },
  { "Ejercicio 4", 0, 0, 0, 0, { 1, 2 }, { 1, 1 }, { 0, 0 }, { 0, 0 } },
  { "Ejercicio 5", 0, 0, 0, 0, { 1, 2 }, { 1, 1 }, { 0, 0 }, { 0, 0 } }
};

Ejercicio* ejercicioActual = &ejercicios[0];

// ============================================================
// CALIBRACIÓN POR EJERCICIO
// ============================================================
void calibrarEjercicio(Ejercicio& ej, unsigned long duracionMs) {
  Serial.print("Calibrando: ");
  Serial.println(ej.nombre);
  Serial.println("Hace UNA repeticion completa, lenta y CORRECTA ahora.");

  float minV = 999, maxV = -999;

  float sumaPostura[CANT_POSTURA] = { 0, 0 };
  float minPostura[CANT_POSTURA] = { 999, 999 };
  float maxPostura[CANT_POSTURA] = { -999, -999 };
  int cantidadLecturas = 0;

  unsigned long inicio = millis();
  while (millis() - inicio < duracionMs) {
    mediciones();
    recibirValoresAux();

    float v = leerAngulo(ej.sensorPrincipal, ej.ejePrincipal);
    if (v < minV) minV = v;
    if (v > maxV) maxV = v;

    for (int i = 0; i < CANT_POSTURA; i++) {
      float vp = leerAngulo(ej.sensorPostura[i], ej.ejePostura[i]);
      sumaPostura[i] += vp;
      if (vp < minPostura[i]) minPostura[i] = vp;
      if (vp > maxPostura[i]) maxPostura[i] = vp;
    }

    cantidadLecturas++;
    delay(50);
  }

  ej.minAngulo = minV;
  ej.maxAngulo = maxV;

  for (int i = 0; i < CANT_POSTURA; i++) {
    ej.centroPostura[i] = sumaPostura[i] / cantidadLecturas;
    float variacion = (maxPostura[i] - minPostura[i]) / 2.0;
    ej.toleranciaPostura[i] = variacion + 5.0;
  }

  Serial.print("Rango principal: ");
  Serial.print(minV);
  Serial.print(" a ");
  Serial.println(maxV);
  for (int i = 0; i < CANT_POSTURA; i++) {
    Serial.print("Postura ");
    Serial.print(i);
    Serial.print(" centro: ");
    Serial.println(ej.centroPostura[i]);
  }
}

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
enum FaseRep { REPOSO,
               MEDIO,
               FIN };
FaseRep faseRep = REPOSO;
unsigned long tInicioRep = 0;

const unsigned long T_MIN = 200;
const unsigned long T_MAX = 5000;
const float MARGEN = 0.15;

bool posturaOK(Ejercicio& ej) {
  for (int i = 0; i < CANT_POSTURA; i++) {
    uint8_t s = ej.sensorPostura[i];
    // Si el sensor de postura es un auxiliar y no respondió, no podemos confiar en el dato
    if (s == 1 && !aux1_ok) return false;
    if (s == 2 && !aux2_ok) return false;

    float vp = leerAngulo(s, ej.ejePostura[i]);
    if (abs(vp - ej.centroPostura[i]) > ej.toleranciaPostura[i]) return false;
  }
  return true;
}

void evaluarRepeticion(Ejercicio& ej) {
  float v = leerAngulo(ej.sensorPrincipal, ej.ejePrincipal);
  bool okPostura = posturaOK(ej);

  float rango = ej.maxAngulo - ej.minAngulo;
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
      if (!okPostura) {
        resultadoValidacion = "MAL (postura)";
        contadorErrores++;
        faseRep = REPOSO;
        break;
        if (!okPostura) {
          resultadoValidacion = (!aux1_ok || !aux2_ok) ? "MAL (sensor desconectado)" : "MAL (postura)";
          contadorErrores++;
          faseRep = REPOSO;
          break;
        }
      }
      if (abs(v - centro) <= rango * MARGEN) {
        faseRep = FIN;
      } else if (millis() - tInicioRep > T_MAX) {
        resultadoValidacion = "MAL (tiempo)";
        contadorErrores++;
        faseRep = REPOSO;
      }
      break;

    case FIN:
      {
        if (!okPostura) {
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
  Serial.println(resultadoValidacion);
}

// ============================================================
// BLE
// ============================================================
class MiServerCallbacks : public NimBLEServerCallbacks {
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
// BOTÓN
// ============================================================
void maquinaAntirrebote() {
  bool lecturaBoton = digitalRead(PIN_BOTON);
  switch (estadoBoton) {
    case ESPERA:
      if (lecturaBoton == LOW) {
        msBoton = 0;
        estadoBoton = CONFIRMACION;
      }
      break;
    case CONFIRMACION:
      if (msBoton >= T_REBOTE) {
        if (lecturaBoton == LOW) estadoBoton = LIBERACION;
        else estadoBoton = ESPERA;
      }
      break;
    case LIBERACION:
      if (lecturaBoton == HIGH) {
        flagBoton = true;
        estadoBoton = ESPERA;
      }
      break;
  }
}

void funcionTimerBoton() {
  if (digitalRead(PIN_BOTON) == LOW) segundosBoton++;
  else segundosBoton = 0;
}
void funcionTimerAntirrebote() {
  msBoton++;
}
void funcionTimerMedicion() {
  flagMedicion = true;
}
// Manda un comando simple (sin parámetros) a un auxiliar.
// No importa mucho si falla -- por eso timeout corto y no se
// reintenta, para no bloquear el resto del sistema.
// Intenta enviar un comando hasta "intentos" veces, con una
// pequeña espera entre reintentos. Devuelve true si en algún
// intento respondió 200 OK.
bool enviarComandoAux(const char* urlBase, const char* comando, int intentos = 3) {
  String url = String(urlBase) + comando;

  for (int i = 0; i < intentos; i++) {
    if (WiFi.softAPgetStationNum() == 0) return false;

    HTTPClient http;
    http.setTimeout(300);
    http.begin(url);
    int codigoHttp = http.GET();
    http.end();

    if (codigoHttp == 200) return true;  // éxito, no hace falta reintentar

    Serial.print("Intento ");
    Serial.print(i + 1);
    Serial.print(" fallido para ");
    Serial.println(url);
    delay(150);  // pequeña pausa antes de reintentar
  }

  return false;  // se agotaron los intentos sin respuesta
}

// Devuelve true solo si ambos auxiliares confirmaron el inicio.
// Si alguno falla, avisa por BLE indicando cuál.
bool iniciarMedicionEnAuxiliares() {
  bool ok1 = enviarComandoAux("http://192.168.4.2/", "iniciar");
  bool ok2 = enviarComandoAux("http://192.168.4.3/", "iniciar");

  if (!ok1 && !ok2) {
    enviarFeedbackBLE("ERROR:no responden los 2 sensores auxiliares");
  } else if (!ok1) {
    enviarFeedbackBLE("ERROR:sensor auxiliar 1 no responde");
  } else if (!ok2) {
    enviarFeedbackBLE("ERROR:sensor auxiliar 2 no responde");
  }

  return ok1 && ok2;
}

void detenerMedicionEnAuxiliares() {
  // Acá no hace falta bloquear el flujo si falla: ya se está
  // terminando la serie de todos modos.
  enviarComandoAux("http://192.168.4.2/", "detener", 1);
  enviarComandoAux("http://192.168.4.3/", "detener", 1);
}
// ============================================================
// MÁQUINA GENERAL
// ============================================================
void Maq_General() {
  switch (estadoMaq_General) {
    case INICIALIZACION:
      digitalWrite(PIN_LED_G, HIGH);
      if (flagBoton) {
        flagBoton = false;

        if (iniciarMedicionEnAuxiliares()) {
          // Todo respondió bien: recién ahora calibra y arranca
          calibrarEstandar();
          faseRep = REPOSO;
          contadorErrores = 0;
          estadoMaq_General = MEDICIONES;
        } else {
          // Algún auxiliar no respondió: se queda en INICIALIZACION,
          // ya se avisó el motivo por BLE dentro de iniciarMedicionEnAuxiliares()
          Serial.println("No se pudo iniciar: revisar sensores auxiliares");
        }
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
        recibirValoresAux();
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

      bool feedbackEnviado = false;  // global

    case ANALISIS_SERIE:
      if (segundosBoton >= 5) {
        resultadoValidacion = "SERIE TERMINADA: " + String(contadorErrores) + " errores";
        segundosBoton = 0;
        flagBoton = false;
        detenerMedicionEnAuxiliares();
        feedbackEnviado = false;  // <- resetea antes de entrar a C_APLICACION
        estadoMaq_General = C_APLICACION;
      }
      break;

    case C_APLICACION:
      if (!feedbackEnviado) {
        enviarFeedbackBLE(resultadoValidacion);
        feedbackEnviado = true;
      }
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
  calibrarOffsetGlobal();

  timerBoton.attach(1, funcionTimerBoton);
  timerAntirrebote.attach_ms(1, funcionTimerAntirrebote);
  timerMedicion.attach_ms(INTERVALO_MEDICION, funcionTimerMedicion);
}

void loop() {
  maquinaAntirrebote();
  Maq_General();
}