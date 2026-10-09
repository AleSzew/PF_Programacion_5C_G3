// ESP32-C3 (Principal) - UDP + BLE + Control de Estados Corregido
#include <MPU6050.h>
#include "Wire.h"
#include "WiFi.h"
#include <WiFiUdp.h>
#include <Ticker.h>
#include <NimBLEDevice.h>
#include "I2Cdev.h"

struct Ejercicio {
  const char* nombre;
  uint8_t sensorPrincipal;  // 0=Local, 1=Aux1, 2=Aux2
  uint8_t ejePrincipal;     // 0=X, 1=Y, 2=Z
  float minAngulo;
  float maxAngulo;
  uint8_t sensorPostura[2];
  uint8_t ejePostura[2];
  float centroPostura[2];
  float toleranciaPostura[2];
};

#define CANT_EJERCICIOS 5
Ejercicio ejercicios[CANT_EJERCICIOS] = {
  { "Curl de biceps", 0, 1, 0, 0, { 1, 99 }, { 0, 99 }, { 0, 0 }, { 0, 0 } },
  { "Elevaciones Lat.", 0, 0, 0, 0, { 1, 99 }, { 1, 99 }, { 0, 0 }, { 0, 0 } },
  { "Press Militar", 0, 2, 0, 0, { 1, 2 }, { 0, 2 }, { 0, 0 }, { 0, 0 } },
  { "Sentadilla", 0, 0, 0, 0, { 1, 99 }, { 0, 99 }, { 0, 0 }, { 0, 0 } },
  { "Peso Muerto", 1, 0, 0, 0, { 0, 99 }, { 0, 99 }, { 0, 0 }, { 0, 0 } }
};

Ejercicio* ejercicioActual = &ejercicios[0];

MPU6050 sensor;
WiFiUDP udp;
const uint16_t PuertoUDP = 8888;
IPAddress ipAux1(192, 168, 4, 2);
IPAddress ipAux2(192, 168, 4, 3);

NimBLEServer* pServer = nullptr;
NimBLECharacteristic* pCharacteristic = nullptr;

Ticker timerBoton;
Ticker timerAntirrebote;
Ticker timerMedicion;

int contadorErrores = 0;
int contadorBien = 0;
bool primeraRepHecha = false;  // se resetea al arrancar cada serie

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

float inclX_aux1, inclY_aux1, inclZ_aux1;
float inclX_aux2, inclY_aux2, inclZ_aux2;
bool aux1_ok = false;
bool aux2_ok = false;
unsigned long tUltimoPaqueteAux1 = 0;
unsigned long tUltimoPaqueteAux2 = 0;
const unsigned long TIMEOUT_AUXILIAR_MS = 500;

String resultadoValidacion = "";
int segundosBoton = 0;
bool feedbackEnviado = false;
int codigo = 1;
// ============================================================
// INTERRUPTOR TEMPORAL: mientras esto este en false, el sistema
// NUNCA bloquea el inicio esperando a los auxiliares -- intenta
// avisarles, pero sigue de largo haya o no respuesta. Poner en
// true cuando los auxiliares sean obligatorios de nuevo.
// ============================================================
bool REQUERIR_AUXILIARES = false;

#define PIN_BOTON 1
#define PIN_LED_R 9
#define PIN_LED_G 20
#define PIN_LED_B 10

const char* ssid = "ESP32_C3_Server";
const char* password = "GRUPO3XX";

void setLED(bool r, bool g, bool b) {
  digitalWrite(PIN_LED_R, r ? HIGH : LOW);
  digitalWrite(PIN_LED_G, g ? HIGH : LOW);
  digitalWrite(PIN_LED_B, b ? HIGH : LOW);
}
void ledRojo() {
  setLED(1, 0, 0);
}
void ledVerde() {
  setLED(0, 1, 0);
}
void ledAmarillo() {
  setLED(1, 1, 0);
}

unsigned long tHoldLed = 0;
bool ledEnHold = false;
const unsigned long DURACION_HOLD_LED = 1000;

void mostrarResultadoLed(bool acierto) {
  if (acierto) ledVerde();
  else ledRojo();
  ledEnHold = true;
  tHoldLed = millis();
}

float offset_X, offset_Y, offset_Z;

void calibrarOffsetGlobal() {
  Serial.println("Calibrando sensor local ESP32...");
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
  Serial.println("Calibracion local completada.");
}

void mediciones() {
  sensor.getAcceleration(&ax_local, &ay_local, &az_local);
  ax_ms2_local = (ax_local / 16384.0) * 9.81;
  ay_ms2_local = (ay_local / 16384.0) * 9.81;
  az_ms2_local = (az_local / 16384.0) * 9.81;

  inclX_local = atan2(ax_ms2_local, sqrt(ay_ms2_local * ay_ms2_local + az_ms2_local * az_ms2_local)) * 180.0 / PI - offset_X;
  inclY_local = atan2(ay_ms2_local, sqrt(ax_ms2_local * ax_ms2_local + az_ms2_local * az_ms2_local)) * 180.0 / PI - offset_Y;
  inclZ_local = atan2(az_ms2_local, sqrt(ax_ms2_local * ax_ms2_local + ay_ms2_local * ay_ms2_local)) * 180.0 / PI - offset_Z;
}

void recibirPaquetesUDP() {
  int packetSize = udp.parsePacket();
  while (packetSize > 0) {
    IPAddress remoteIP = udp.remoteIP();
    char buffer[64] = { 0 };
    udp.read(buffer, sizeof(buffer) - 1);
    String payload = String(buffer);

    /*Serial.print("UDP recibido de ");
    Serial.print(remoteIP);
    Serial.print(": ");
    Serial.println(payload);  // <-- AGREGAR
    */
    if (payload != "ERROR_I2C" && payload != "NOMIDIENDO") {
      int c1 = payload.indexOf(',');
      int c2 = payload.indexOf(',', c1 + 1);
      if (c1 > 0 && c2 > 0) {
        float x = payload.substring(0, c1).toFloat();
        float y = payload.substring(c1 + 1, c2).toFloat();
        float z = payload.substring(c2 + 1).toFloat();

        if (remoteIP == ipAux1) {
          inclX_aux1 = x;
          inclY_aux1 = y;
          inclZ_aux1 = z;
          tUltimoPaqueteAux1 = millis();
        } else if (remoteIP == ipAux2) {
          inclX_aux2 = x;
          inclY_aux2 = y;
          inclZ_aux2 = z;
          tUltimoPaqueteAux2 = millis();
        }
      }
    }
    packetSize = udp.parsePacket();
  }

  aux1_ok = (tUltimoPaqueteAux1 > 0) && (millis() - tUltimoPaqueteAux1 < TIMEOUT_AUXILIAR_MS);
  aux2_ok = (tUltimoPaqueteAux2 > 0) && (millis() - tUltimoPaqueteAux2 < TIMEOUT_AUXILIAR_MS);
}

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
  return (eje == 0) ? x : ((eje == 1) ? y : z);
}

bool sensorUsado(Ejercicio& ej, uint8_t sensorId) {
  if (ej.sensorPrincipal == sensorId) return true;
  for (int i = 0; i < 2; i++) {
    if (ej.sensorPostura[i] == sensorId) return true;
  }
  return false;
}

void seleccionarEjercicio() {
  if (codigo < 1 || codigo > CANT_EJERCICIOS) {
    ejercicioActual = &ejercicios[0];
  } else {
    ejercicioActual = &ejercicios[codigo - 1];
  }
}

void enviarComandoUDPAuxiliares(const char* cmd, Ejercicio& ej) {
  if (sensorUsado(ej, 1)) {
    udp.beginPacket(ipAux1, PuertoUDP);
    udp.print(cmd);
    udp.endPacket();
  }
  if (sensorUsado(ej, 2)) {
    udp.beginPacket(ipAux2, PuertoUDP);
    udp.print(cmd);
    udp.endPacket();
  }
}

bool iniciarMedicionEnAuxiliares(Ejercicio& ej) {
  bool necesitaAux1 = sensorUsado(ej, 1);
  bool necesitaAux2 = sensorUsado(ej, 2);

  if (!necesitaAux1 && !necesitaAux2) {
    Serial.println("Ejercicio requiere solo sensor Local.");
    return true;
  }

  Serial.println("Enviando orden INICIAR por UDP a los auxiliares requeridos...");

  unsigned long inicioHandshake = millis();
  while (millis() - inicioHandshake < 2500) {
    for (int i = 0; i < 3; i++) {
      enviarComandoUDPAuxiliares("INICIAR", ej);
      delay(20);
    }

    unsigned long tPausa = millis();
    while (millis() - tPausa < 150) {
      recibirPaquetesUDP();
      delay(10);
    }

    bool aux1Listo = !necesitaAux1 || aux1_ok;
    bool aux2Listo = !necesitaAux2 || aux2_ok;
    if (aux1Listo && aux2Listo) {
      Serial.println(" Handshake UDP exitoso. Auxiliares transmitiendo.");
      return true;
    }
  }

  Serial.println(" Timeout UDP: Un auxiliar requerido no respondio.");
  if (necesitaAux1 && !aux1_ok) Serial.println(" -> Auxiliar 1 sin señal");
  if (necesitaAux2 && !aux2_ok) Serial.println(" -> Auxiliar 2 sin señal");

  // Con REQUERIR_AUXILIARES en false, seguimos de largo igual:
  // la medicion arranca sin bloquear, usando lo que haya disponible.
  if (!REQUERIR_AUXILIARES) {
    Serial.println(" (REQUERIR_AUXILIARES=false, arrancando de todas formas)");
    return true;
  }

  return false;
}


enum FaseRep { REPOSO,
               EN_MOVIMIENTO };
FaseRep faseRep = REPOSO;
unsigned long tInicioRep = 0;

enum SubFaseRep { SUBIENDO,
                  BAJANDO };
SubFaseRep subFaseRep = SUBIENDO;
const float MARGEN_SUBFASE = 0.05;

float baseLocal = 0;
float amplitudObjetivo = 1.0;
int direccionMovimiento = 1;
float picoAlcanzado = 0;

const float ALPHA_BASE_REPOSO = 0.02;
const float TOLERANCIA_REPOSO_DEG = 4.0;
const float TOLERANCIA_VEL_DPS = 15.0;
const float UMBRAL_SALIDA = 0.15;
const float UMBRAL_PICO = 0.60;
const float UMBRAL_RETORNO = 0.25;
const int MUESTRAS_CONFIRMACION = 3;

unsigned long duracionCalibrada = 1500;
const float FACTOR_DURACION_MIN = 0.5;
const float FACTOR_DURACION_MAX = 3.0;
const unsigned long T_MIN = 200;
const unsigned long T_MAX = 5000;

int contadorRetorno = 0;
int contadorErrorVelocidad = 0;

float progreso(float v) {
  return direccionMovimiento * (v - baseLocal) / amplitudObjetivo;
}

void calibrarEjercicio(Ejercicio& ej, unsigned long duracionMaximaMs) {
  Serial.print("Calibrando movimiento para: ");
  Serial.println(ej.nombre);
  Serial.println("Mantene la posicion de reposo...");

  // --- Paso 1: reposo del sensor principal + de los sensores de postura ---
  const int MUESTRAS_REPOSO = 10;
  float sumaReposo = 0;
  float sumaPosturaReposo[2] = { 0, 0 };
  int muestrasPostura[2] = { 0, 0 };

  for (int i = 0; i < MUESTRAS_REPOSO; i++) {
    mediciones();
    recibirPaquetesUDP();
    sumaReposo += leerAngulo(ej.sensorPrincipal, ej.ejePrincipal);
    for (int p = 0; p < 2; p++) {
      if (ej.sensorPostura[p] == 99) continue;
      sumaPosturaReposo[p] += leerAngulo(ej.sensorPostura[p], ej.ejePostura[p]);
      muestrasPostura[p]++;
    }
    delay(50);
  }
  float reposoValor = sumaReposo / MUESTRAS_REPOSO;
  float reposoPostura[2] = { 0, 0 };
  for (int p = 0; p < 2; p++) {
    if (muestrasPostura[p] > 0) reposoPostura[p] = sumaPosturaReposo[p] / muestrasPostura[p];
  }

  Serial.println("Ahora hace UNA repeticion completa, lenta y CORRECTA...");
  Serial.println("(la calibracion termina sola cuando volves a la posicion inicial)");

  const float AMPLITUD_MINIMA_CALIB = 8.0;
  const float FRACCION_RETORNO_CALIB = 0.25;
  const int MUESTRAS_CONFIRM_CALIB = 3;

  float minV = reposoValor, maxV = reposoValor;
  float amplitudMax = 0;
  float lateralMax[2] = { 0, 0 };
  bool salioDeReposo = false;
  int contadorRetornoCalib = 0;
  unsigned long tInicioMovCalib = 0;

  unsigned long inicio = millis();
  while (millis() - inicio < duracionMaximaMs) {
    mediciones();
    recibirPaquetesUDP();

    float v = leerAngulo(ej.sensorPrincipal, ej.ejePrincipal);
    if (v < minV) minV = v;
    if (v > maxV) maxV = v;

    float distancia = fabs(v - reposoValor);
    if (distancia > amplitudMax) amplitudMax = distancia;

    for (int p = 0; p < 2; p++) {
      if (ej.sensorPostura[p] == 99) continue;
      float vp = leerAngulo(ej.sensorPostura[p], ej.ejePostura[p]);
      float d = fabs(vp - reposoPostura[p]);
      if (d > lateralMax[p]) lateralMax[p] = d;
    }

    const unsigned long TIEMPO_MINIMO_MOVIMIENTO_CALIB = 400;  // ms, evita cortar por un temblor inicial

    if (salioDeReposo && (millis() - tInicioMovCalib) >= TIEMPO_MINIMO_MOVIMIENTO_CALIB) {
      float progresoCalib = distancia / amplitudMax;
      if (progresoCalib <= FRACCION_RETORNO_CALIB) {
        contadorRetornoCalib++;
        if (contadorRetornoCalib >= MUESTRAS_CONFIRM_CALIB) break;
      } else {
        contadorRetornoCalib = 0;
      }
    }
    delay(50);
  }

  ej.minAngulo = minV;
  ej.maxAngulo = maxV;

  float picoValor = (fabs(maxV - reposoValor) > fabs(minV - reposoValor)) ? maxV : minV;

  baseLocal = reposoValor;
  amplitudObjetivo = fabs(picoValor - reposoValor);
  if (amplitudObjetivo < 1.0) amplitudObjetivo = 1.0;
  direccionMovimiento = (picoValor >= reposoValor) ? 1 : -1;

  const float FACTOR_MARGEN_LATERAL = 1.5;
  const float TOLERANCIA_LATERAL_MINIMA = 3.0;
  for (int p = 0; p < 2; p++) {
    if (ej.sensorPostura[p] == 99) continue;
    ej.centroPostura[p] = reposoPostura[p];
    float tol = lateralMax[p] * FACTOR_MARGEN_LATERAL;
    if (tol < TOLERANCIA_LATERAL_MINIMA) tol = TOLERANCIA_LATERAL_MINIMA;
    ej.toleranciaPostura[p] = tol;
  }

  if (tInicioMovCalib > 0) {
    duracionCalibrada = millis() - tInicioMovCalib;
  }
  if (duracionCalibrada < 300) duracionCalibrada = 1500;

  Serial.print("Reposo: ");
  Serial.print(reposoValor);
  Serial.print(" | Pico: ");
  Serial.print(picoValor);
  Serial.print(" | Amplitud: ");
  Serial.print(amplitudMax);
  Serial.print(" | Duracion calibrada: ");
  Serial.print(duracionCalibrada);
  Serial.println(" ms");

  if (amplitudMax < AMPLITUD_MINIMA_CALIB) {
    Serial.println("ADVERTENCIA: casi no se detecto movimiento durante la calibracion.");
  }
}


void enviarFeedbackBLE(const String& mensaje) {
  if (pCharacteristic != nullptr && pServer->getConnectedCount() > 0) {
    pCharacteristic->setValue(mensaje.c_str());
    pCharacteristic->notify();
  }
}

void actualizarLedMedicion(Ejercicio& ej) {
  if (ledEnHold) {
    if (millis() - tHoldLed >= DURACION_HOLD_LED) ledEnHold = false;
    else return;
  }
  if (faseRep == REPOSO) {
    // Antes de la primera repeticion: verde (igual que mientras se mide).
    // Despues de la primera: amarillo, esperando la siguiente.
    if (primeraRepHecha) ledAmarillo();
    else ledVerde();
  } else {
    ledVerde();  // en movimiento, siempre verde
  }
}
bool posturaOK(Ejercicio& ej) {
  for (int i = 0; i < 2; i++) {
    uint8_t s = ej.sensorPostura[i];
    if (s == 99) continue;

    bool sensorCaido = (s == 1 && !aux1_ok) || (s == 2 && !aux2_ok);
    if (sensorCaido) {
      // Si los auxiliares no son obligatorios ahora mismo, un sensor de
      // postura caido simplemente no se evalua (no cuenta como error).
      if (!REQUERIR_AUXILIARES) continue;
      return false;
    }

    float vp = leerAngulo(s, ej.ejePostura[i]);
    if (abs(vp - ej.centroPostura[i]) > ej.toleranciaPostura[i]) return false;
  }
  return true;
}
// Devuelve false si el sensor principal del ejercicio es un auxiliar
// y ese auxiliar esta desconectado -- evita evaluar contra datos viejos.
bool sensorPrincipalOK(Ejercicio& ej) {
  if (ej.sensorPrincipal == 1 && !aux1_ok) return false;
  if (ej.sensorPrincipal == 2 && !aux2_ok) return false;
  return true;
}

void evaluarRepeticion(Ejercicio& ej, float velocidadActual) {
  if (!sensorPrincipalOK(ej)) {
    if (faseRep != REPOSO) {
      resultadoValidacion = "MAL (sensor principal desconectado)";
      Serial.println("Resultado Repeticion: " + resultadoValidacion);
      contadorErrores++;
      faseRep = REPOSO;
      contadorErrorVelocidad = 0;
      contadorRetorno = 0;
      primeraRepHecha = true;
      mostrarResultadoLed(false);

      String msgContador = "CONTADOR:BIEN=" + String(contadorBien) + ",MAL=" + String(contadorErrores);
      enviarFeedbackBLE(msgContador);
    }
    return;
  }

  float v = leerAngulo(ej.sensorPrincipal, ej.ejePrincipal);
  float p = progreso(v);
  bool okPostura = posturaOK(ej);
  String resultado = "";

  switch (faseRep) {
    case REPOSO:
      // Mientras esta quieto cerca del cero actual, el cero se va ajustando solo
      if (fabs(v - baseLocal) < TOLERANCIA_REPOSO_DEG) {
        baseLocal = ALPHA_BASE_REPOSO * v + (1 - ALPHA_BASE_REPOSO) * baseLocal;
      }

      if (p >= UMBRAL_SALIDA) {
        faseRep = EN_MOVIMIENTO;
        subFaseRep = SUBIENDO;
        tInicioRep = millis();
        picoAlcanzado = p;
        contadorRetorno = 0;
        contadorErrorVelocidad = 0;
      }
      break;

    case EN_MOVIMIENTO:
      {
        if (p > picoAlcanzado) picoAlcanzado = p;

        if (subFaseRep == SUBIENDO && p < picoAlcanzado - MARGEN_SUBFASE) {
          subFaseRep = BAJANDO;
        }

        // Velocidad esperada segun la subfase: yendo al pico, positiva; volviendo, negativa.
        float velEnDireccion = direccionMovimiento * velocidadActual;
        float velEsperada = (subFaseRep == SUBIENDO) ? velEnDireccion : -velEnDireccion;
        if (velEsperada < -TOLERANCIA_VEL_DPS) {
          contadorErrorVelocidad++;
        } else {
          contadorErrorVelocidad = 0;
        }

        if (contadorErrorVelocidad >= MUESTRAS_CONFIRMACION) {
          resultado = "MAL (velocidad o sentido del movimiento incorrecto)";
          faseRep = REPOSO;
          contadorRetorno = 0;
        } else if (!okPostura) {
          resultado = (!aux1_ok || !aux2_ok) ? "MAL (sensor desconectado)" : "MAL (postura)";
          faseRep = REPOSO;
          contadorRetorno = 0;
        } else if (p <= UMBRAL_RETORNO || fabs(v - baseLocal) <= TOLERANCIA_REPOSO_DEG) {
          contadorRetorno++;
          if (contadorRetorno >= MUESTRAS_CONFIRMACION) {
            unsigned long duracion = millis() - tInicioRep;
            unsigned long duracionMin = (unsigned long)(duracionCalibrada * FACTOR_DURACION_MIN);
            unsigned long duracionMax = (unsigned long)(duracionCalibrada * FACTOR_DURACION_MAX);
            if (duracionMin < T_MIN) duracionMin = T_MIN;

            if (picoAlcanzado < UMBRAL_PICO) {
              resultado = "MAL (rango de movimiento incompleto)";
            } else if (duracion < duracionMin) {
              resultado = "MAL (muy rapido)";
            } else if (duracion > duracionMax) {
              resultado = "MAL (muy lento)";
            } else {
              resultado = "BIEN";
            }
            faseRep = REPOSO;
            contadorRetorno = 0;
          }
        } else {
          contadorRetorno = 0;
        }

        if (faseRep == EN_MOVIMIENTO && millis() - tInicioRep > T_MAX) {
          resultado = "MAL (tiempo superado)";
          faseRep = REPOSO;
          contadorRetorno = 0;
        }
        break;
      }
  }

  if (resultado != "") {
    resultadoValidacion = resultado;
    Serial.println("Resultado Repeticion: " + resultadoValidacion);
    mostrarResultadoLed(resultado == "BIEN");
    contadorErrorVelocidad = 0;
    primeraRepHecha = true;
    if (resultado == "BIEN") contadorBien++;
    else contadorErrores++;

    // En vez de mandar el detalle del error, se manda solo el conteo acumulado
    String msgContador = "CONTADOR:BIEN=" + String(contadorBien) + ",MAL=" + String(contadorErrores);
    enviarFeedbackBLE(msgContador);
  }
}


// ============================================================
// VELOCIDAD GENÉRICA POR DIFERENCIACIÓN (sirve para local o auxiliar)
// ============================================================
float anguloAnteriorPrincipal = 0;
unsigned long tAnguloAnteriorPrincipal = 0;
float velocidadPrincipal = 0;
const float ALPHA_VEL = 0.4;  // mismo suavizado que usaba doc 23

float calcularVelocidad(float anguloActual) {
  unsigned long ahora = millis();
  float dt = (tAnguloAnteriorPrincipal == 0) ? 0.1 : (ahora - tAnguloAnteriorPrincipal) / 1000.0;
  if (dt <= 0 || dt > 0.5) dt = 0.1;
  tAnguloAnteriorPrincipal = ahora;

  float velNueva = (anguloActual - anguloAnteriorPrincipal) / dt;
  velocidadPrincipal = ALPHA_VEL * velNueva + (1 - ALPHA_VEL) * velocidadPrincipal;
  anguloAnteriorPrincipal = anguloActual;
  return velocidadPrincipal;
}

class MiServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
    Serial.println("App conectada por BLE.");
  }
  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
    Serial.println("App desconectada. Reiniciando publicidad BLE...");
    NimBLEDevice::startAdvertising();
  }
};

class MiCharacteristicCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
    std::string valor = pCharacteristic->getValue();
    if (valor.length() > 0) {
      codigo = atoi(valor.c_str());
      Serial.print("Codigo de ejercicio recibido por BLE: ");
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

void Maq_General() {
  switch (estadoMaq_General) {
    case INICIALIZACION:
      if (!ledEnHold) ledVerde();
      recibirPaquetesUDP();

      if (flagBoton) {
        flagBoton = false;
        Serial.println("\n[BOTON PRESIONADO] -> Iniciando secuencia...");
        ledAmarillo();

        seleccionarEjercicio();
        Serial.print("Ejercicio a ejecutar: ");
        Serial.println(ejercicioActual->nombre);

        if (iniciarMedicionEnAuxiliares(*ejercicioActual)) {
          ledVerde();
          Serial.println("Iniciando calibracion de movimiento...");
          calibrarEjercicio(*ejercicioActual, 8000);
          faseRep = REPOSO;
          contadorErrores = 0;
          contadorErrorVelocidad = 0;
          primeraRepHecha = false;
          contadorBien = 0;
          contadorRetorno = 0;
          subFaseRep = SUBIENDO;
          estadoMaq_General = MEDICIONES;
          enviarFeedbackBLE("ESTADO:MEDICION_INICIADA");
          Serial.println(">>> ESTADO ACTUAL: MEDICIONES <<<");
        } else {
          mostrarResultadoLed(false);  // usa el hold de 1 segundo que ya existe, en rojo
          enviarFeedbackBLE("ERROR:Sensores auxiliares sin respuesta");
        }
      }
      break;

    case MEDICIONES:
      recibirPaquetesUDP();
      if (flagMedicion) {
        mediciones();
        recibirPaquetesUDP();
        flagMedicion = false;
        float v = leerAngulo(ejercicioActual->sensorPrincipal, ejercicioActual->ejePrincipal);
        float velocidadActual = calcularVelocidad(v);
        evaluarRepeticion(*ejercicioActual, velocidadActual);
      }
      actualizarLedMedicion(*ejercicioActual);

      if (flagBoton) {
        flagBoton = false;
        Serial.println("\n[BOTON PRESIONADO] -> Finalizando serie...");
        estadoMaq_General = ANALISIS_SERIE;
      }
      break;

    case ANALISIS_SERIE:
      resultadoValidacion = "SERIE TERMINADA: " + String(contadorBien) + " bien, " + String(contadorErrores) + " mal";
      Serial.println(resultadoValidacion);
      segundosBoton = 0;
      flagBoton = false;
      enviarComandoUDPAuxiliares("DETENER", *ejercicioActual);
      feedbackEnviado = false;
      estadoMaq_General = C_APLICACION;
      Serial.println(">>> ESTADO ACTUAL: C_APLICACION <<<");
      break;

    case C_APLICACION:
      if (!feedbackEnviado) {
        enviarFeedbackBLE(resultadoValidacion);
        feedbackEnviado = true;
      }
      if (flagBoton) {
        flagBoton = false;
        Serial.println("\n[BOTON PRESIONADO] -> Volviendo a INICIALIZACION...");
        estadoMaq_General = INICIALIZACION;
      }
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n==================================");
  Serial.println("  INICIANDO ESP32-C3 PRINCIPAL   ");
  Serial.println("==================================");

  Wire.begin(6, 7);
  sensor.initialize();

  pinMode(PIN_BOTON, INPUT_PULLUP);
  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_B, OUTPUT);

  WiFi.mode(WIFI_AP);
  bool apOk = WiFi.softAP(ssid, password);
  Serial.print("Access Point creado: ");
  Serial.println(apOk ? "SI" : "NO");
  Serial.print("IP del AP: ");
  Serial.println(WiFi.softAPIP());

  udp.begin(PuertoUDP);
  inicializarBLE();
  calibrarOffsetGlobal();

  timerBoton.attach(1, funcionTimerBoton);
  timerAntirrebote.attach_ms(1, funcionTimerAntirrebote);
  timerMedicion.attach_ms(INTERVALO_MEDICION, funcionTimerMedicion);

  Serial.println("SISTEMA LISTO EN ESTADO INICIALIZACION.");
}

void loop() {
  maquinaAntirrebote();
  Maq_General();
}
