// ESP32-S3 (Principal) - UDP + BLE + Control de Estados
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include <MPU6050.h>
#include "Wire.h"
#include "WiFi.h"
#include <WiFiUdp.h>
#include <Ticker.h>
#include <NimBLEDevice.h>
#include "I2Cdev.h"

// ============================================================
// true  = nunca se duerme (para programar sin pelearse con el puerto COM)
// false = encendido con pulsacion larga (hay que mantener el boton 3 s)
// ============================================================
const bool MODO_DESARROLLO = true;
const unsigned long TIEMPO_ENCENDIDO_MS = 3000;

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

// ============================================================
// HARDWARE Y RED
// ============================================================
MPU6050 sensor;
WiFiUDP udp;
const uint16_t PuertoUDP = 8888;
IPAddress ipAux1(192, 168, 4, 2);
IPAddress ipAux2(192, 168, 4, 3);

NimBLEServer* pServer = nullptr;
NimBLECharacteristic* pCharacteristic = nullptr;

Ticker timerAntirrebote;
Ticker timerMedicion;

#define PIN_BOTON 5
#define PIN_LED_R 4
#define PIN_LED_G 2

const char* ssid = "ESP32_C3_Server";
const char* password = "GRUPO3XX";

// En false, el sistema NUNCA bloquea el inicio esperando a los auxiliares.
bool REQUERIR_AUXILIARES = false;

void maquinaAntirrebote();  // declaracion previa: se usa dentro de bucles bloqueantes

// ============================================================
// ESTADOS Y BOTON
// ============================================================
typedef enum { INICIALIZACION,
               MEDICIONES,
               ANALISIS_SERIE,
               C_APLICACION } estadoMaq_General_t;
estadoMaq_General_t estadoMaq_General = INICIALIZACION;

typedef enum { ESPERA,
               CONFIRMACION,
               LIBERACION } estadoAntirrebote_t;
estadoAntirrebote_t estadoBoton = ESPERA;
volatile int msBoton = 0;
bool flagBoton = false;
#define T_REBOTE 10

volatile bool flagMedicion = false;
int INTERVALO_MEDICION = 100;
volatile int codigo = 1;

// ============================================================
// DATOS DE SENSORES
// ============================================================
int16_t ax_local, ay_local, az_local;
float ax_ms2_local, ay_ms2_local, az_ms2_local, inclX_local, inclY_local, inclZ_local;

float inclX_aux1, inclY_aux1, inclZ_aux1;
float inclX_aux2, inclY_aux2, inclZ_aux2;
bool aux1_ok = false;
bool aux2_ok = false;
unsigned long tUltimoPaqueteAux1 = 0;
unsigned long tUltimoPaqueteAux2 = 0;
const unsigned long TIMEOUT_AUXILIAR_MS = 500;

// Keepalive: mientras se mide, se re-manda INICIAR cada 1 s para que el
// auxiliar sepa que el principal sigue vivo.
unsigned long tUltimoKeepalive = 0;
const unsigned long INTERVALO_KEEPALIVE_MS = 1000;

float offset_X, offset_Y, offset_Z;

// Velocidad por diferenciacion (sirve para sensor local o auxiliar)
float anguloAnteriorPrincipal = 0;
unsigned long tAnguloAnteriorPrincipal = 0;
float velocidadPrincipal = 0;
const float ALPHA_VEL = 0.4;

// ============================================================
// ESTADO DE LA REPETICION Y PARAMETROS DEL ALGORITMO
// ============================================================
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
const float UMBRAL_SALIDA = 0.25;   // la rep arranca al pasar el 25% del recorrido
const float UMBRAL_PICO = 0.60;     // para ser valida debe llegar al 60%
const float UMBRAL_RETORNO = 0.12;  // termina al bajar del 12% (siempre MENOR que la salida)
const int MUESTRAS_CONFIRMACION = 3;

unsigned long duracionCalibrada = 1500;
const float FACTOR_DURACION_MIN = 0.5;  // menos del 50% del tiempo calibrado = muy rapido
const float FACTOR_DURACION_MAX = 3.0;  // mas del 300% del tiempo calibrado = muy lento
const unsigned long T_MIN = 200;

int contadorRetorno = 0;
int contadorErrorVelocidad = 0;
bool primeraRepHecha = false;

// ============================================================
// CONTADORES DE LA SERIE Y DETALLE DE ERRORES
// ============================================================
int contadorErrores = 0;
int contadorBien = 0;
String resultadoValidacion = "";
bool feedbackEnviado = false;

enum TipoError { ERR_NINGUNO,
                 ERR_VELOCIDAD,
                 ERR_POSTURA,
                 ERR_RANGO,
                 ERR_RAPIDO,
                 ERR_LENTO,
                 ERR_DESCONEXION };

int errVelocidad = 0;
int errPostura[3] = { 0, 0, 0 };  // indice = id de sensor (0 local, 1 aux1, 2 aux2)
int errRango = 0;
int errRapido = 0;
int errLento = 0;
int errDesconexion = 0;

// ============================================================
// BLE: ENVIO
// ============================================================
void enviarFeedbackBLE(const String& mensaje) {
  if (pCharacteristic != nullptr && pServer != nullptr && pServer->getConnectedCount() > 0) {
    pCharacteristic->setValue(mensaje.c_str());
    pCharacteristic->notify();
  }
}

void enviarContadorBLE() {
  String msg = "CONTADOR:BIEN=" + String(contadorBien) + ",MAL=" + String(contadorErrores);
  enviarFeedbackBLE(msg);
}

// Orden: velocidad, postura s0, postura s1, postura s2, rango, rapido, lento, desconexion
void enviarDetalleErroresBLE() {
  char msg[40];
  snprintf(msg, sizeof(msg), "DET:%d,%d,%d,%d,%d,%d,%d,%d",
           errVelocidad, errPostura[0], errPostura[1], errPostura[2],
           errRango, errRapido, errLento, errDesconexion);
  enviarFeedbackBLE(String(msg));
}

void imprimirDetalleErrores() {
  Serial.println("--- Detalle de errores ---");
  Serial.print("Velocidad/sentido: ");
  Serial.println(errVelocidad);
  Serial.print("Postura sensor 0 (local): ");
  Serial.println(errPostura[0]);
  Serial.print("Postura sensor 1 (aux 1): ");
  Serial.println(errPostura[1]);
  Serial.print("Postura sensor 2 (aux 2): ");
  Serial.println(errPostura[2]);
  Serial.print("Rango incompleto: ");
  Serial.println(errRango);
  Serial.print("Muy rapido: ");
  Serial.println(errRapido);
  Serial.print("Muy lento: ");
  Serial.println(errLento);
  Serial.print("Sensor desconectado: ");
  Serial.println(errDesconexion);
}

void registrarError(TipoError tipo, int sensorId) {
  switch (tipo) {
    case ERR_VELOCIDAD: errVelocidad++; break;
    case ERR_POSTURA:
      if (sensorId >= 0 && sensorId <= 2) errPostura[sensorId]++;
      break;
    case ERR_RANGO: errRango++; break;
    case ERR_RAPIDO: errRapido++; break;
    case ERR_LENTO: errLento++; break;
    case ERR_DESCONEXION: errDesconexion++; break;
    default: break;
  }
}

// ============================================================
// LED
// ============================================================
void setLED(bool r, bool g) {
  digitalWrite(PIN_LED_R, r ? HIGH : LOW);
  digitalWrite(PIN_LED_G, g ? HIGH : LOW);
}
void ledRojo() {
  setLED(1, 0);
}
void ledVerde() {
  setLED(0, 1);
}
void ledAmarillo() {
  setLED(1, 1);
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

// Se llama en CADA vuelta del loop: el hold siempre se libera.
void actualizarHoldLed() {
  if (ledEnHold && millis() - tHoldLed >= DURACION_HOLD_LED) ledEnHold = false;
}

void actualizarLedMedicion() {
  if (ledEnHold) return;
  if (faseRep == REPOSO) {
    if (primeraRepHecha) ledAmarillo();  // esperando la siguiente
    else ledVerde();                     // antes de la primera repeticion
  } else {
    ledVerde();  // en movimiento
  }
}

// ============================================================
// BOTON
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

void funcionTimerAntirrebote() {
  msBoton = msBoton + 1;
}
void funcionTimerMedicion() {
  flagMedicion = true;
}

// ============================================================
// SENSOR LOCAL
// ============================================================
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

float progreso(float v) {
  return direccionMovimiento * (v - baseLocal) / amplitudObjetivo;
}

// ============================================================
// UDP: AUXILIARES
// ============================================================
void recibirPaquetesUDP() {
  int packetSize = udp.parsePacket();
  while (packetSize > 0) {
    IPAddress remoteIP = udp.remoteIP();
    char buffer[64] = { 0 };
    udp.read(buffer, sizeof(buffer) - 1);
    String payload = String(buffer);

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

// UDP puede perder paquetes: DETENER se manda en rafaga de 3.
void enviarDetenerAuxiliares(Ejercicio& ej) {
  for (int i = 0; i < 3; i++) {
    enviarComandoUDPAuxiliares("DETENER", ej);
    delay(20);
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
    maquinaAntirrebote();
    for (int i = 0; i < 3; i++) {
      enviarComandoUDPAuxiliares("INICIAR", ej);
      delay(20);
    }

    unsigned long tPausa = millis();
    while (millis() - tPausa < 150) {
      maquinaAntirrebote();
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

  if (!REQUERIR_AUXILIARES) {
    Serial.println(" (REQUERIR_AUXILIARES=false, arrancando de todas formas)");
    return true;
  }
  return false;
}

// ============================================================
// CALIBRACION DEL EJERCICIO (devuelve false si no hubo movimiento)
// ============================================================
bool calibrarEjercicio(Ejercicio& ej, unsigned long duracionMaximaMs) {
  Serial.print("Calibrando movimiento para: ");
  Serial.println(ej.nombre);
  Serial.println("Mantene la posicion de reposo...");
  enviarFeedbackBLE("CAL:QUIETO");

  // --- Paso 1: reposo del sensor principal y de los de postura (1 segundo) ---
  const int MUESTRAS_REPOSO = 20;
  float sumaReposo = 0;
  float sumaPosturaReposo[2] = { 0, 0 };
  int muestrasPostura[2] = { 0, 0 };

  for (int i = 0; i < MUESTRAS_REPOSO; i++) {
    maquinaAntirrebote();
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
  enviarFeedbackBLE("CAL:MOVER");

  // --- Paso 2: seguir el movimiento hasta que vuelva solo ---
  const float UMBRAL_SALIDA_CALIB = 5.0;     // grados para considerar que empezo el movimiento
  const float AMPLITUD_MINIMA_CALIB = 8.0;   // amplitud minima para que cuente como repeticion
  const float TOLERANCIA_RETORNO_DEG = 4.0;  // siempre MENOR que UMBRAL_SALIDA_CALIB
  const float FRACCION_RETORNO_CALIB = 0.25;
  const int MUESTRAS_CONFIRM_CALIB = 3;
  const unsigned long TIEMPO_MINIMO_MOVIMIENTO_CALIB = 400;  // ms

  float minV = reposoValor, maxV = reposoValor;
  float amplitudMax = 0;
  float lateralMax[2] = { 0, 0 };
  bool salioDeReposo = false;
  int contadorRetornoCalib = 0;
  unsigned long tInicioMovCalib = 0;

  unsigned long inicio = millis();
  while (millis() - inicio < duracionMaximaMs) {
    maquinaAntirrebote();
    mediciones();
    recibirPaquetesUDP();

    float v = leerAngulo(ej.sensorPrincipal, ej.ejePrincipal);
    float distancia = fabs(v - reposoValor);

    if (!salioDeReposo && distancia >= UMBRAL_SALIDA_CALIB) {
      salioDeReposo = true;
      tInicioMovCalib = millis();
      Serial.println("[CALIBRACION] Movimiento iniciado...");
      enviarFeedbackBLE("CAL:EN_CURSO");
    }

    if (v < minV) minV = v;
    if (v > maxV) maxV = v;
    if (distancia > amplitudMax) amplitudMax = distancia;

    for (int p = 0; p < 2; p++) {
      if (ej.sensorPostura[p] == 99) continue;
      float vp = leerAngulo(ej.sensorPostura[p], ej.ejePostura[p]);
      float d = fabs(vp - reposoPostura[p]);
      if (d > lateralMax[p]) lateralMax[p] = d;
    }

    // Solo se puede cortar si ya hubo un movimiento real (amplitud y tiempo minimos)
    if (salioDeReposo && amplitudMax >= AMPLITUD_MINIMA_CALIB && (millis() - tInicioMovCalib) >= TIEMPO_MINIMO_MOVIMIENTO_CALIB) {
      bool enPosicionInicial = (distancia <= TOLERANCIA_RETORNO_DEG) || ((distancia / amplitudMax) <= FRACCION_RETORNO_CALIB);
      if (enPosicionInicial) {
        contadorRetornoCalib++;
        if (contadorRetornoCalib >= MUESTRAS_CONFIRM_CALIB) {
          Serial.println("[CALIBRACION] Repeticion inicial detectada.");
          break;
        }
      } else {
        contadorRetornoCalib = 0;
      }
    }
    delay(50);
  }

  // Calibracion invalida: no se movio. Se rechaza antes de tocar nada.
  if (amplitudMax < AMPLITUD_MINIMA_CALIB) {
    Serial.println("Calibracion invalida: no se detecto movimiento.");
    enviarFeedbackBLE("CAL:FALLO");
    return false;
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

  if (tInicioMovCalib > 0) duracionCalibrada = millis() - tInicioMovCalib;
  if (duracionCalibrada < 300) duracionCalibrada = 300;

  Serial.print("Reposo: ");
  Serial.print(reposoValor);
  Serial.print(" | Pico: ");
  Serial.print(picoValor);
  Serial.print(" | Amplitud: ");
  Serial.print(amplitudMax);
  Serial.print(" | Duracion calibrada: ");
  Serial.print(duracionCalibrada);
  Serial.println(" ms");

  enviarFeedbackBLE("CAL:OK");
  return true;
}

// ============================================================
// VALIDACIONES
// ============================================================
// Devuelve -1 si la postura esta bien; si no, el id (0,1,2) del sensor que fallo.
// "caido" = true si fallo porque ese sensor esta desconectado.
int sensorPosturaFallido(Ejercicio& ej, bool& caido) {
  caido = false;
  for (int i = 0; i < 2; i++) {
    uint8_t s = ej.sensorPostura[i];
    if (s == 99) continue;

    bool sensorCaido = (s == 1 && !aux1_ok) || (s == 2 && !aux2_ok);
    if (sensorCaido) {
      if (!REQUERIR_AUXILIARES) continue;  // auxiliares opcionales: no se evalua
      caido = true;
      return s;
    }

    float vp = leerAngulo(s, ej.ejePostura[i]);
    if (fabs(vp - ej.centroPostura[i]) > ej.toleranciaPostura[i]) return s;
  }
  return -1;
}

// Falso si el sensor principal es un auxiliar caido (no hay nada confiable que medir)
bool sensorPrincipalOK(Ejercicio& ej) {
  if (ej.sensorPrincipal == 1 && !aux1_ok) return false;
  if (ej.sensorPrincipal == 2 && !aux2_ok) return false;
  return true;
}

// ============================================================
// CIERRE Y EVALUACION DE UNA REPETICION
// ============================================================
void cerrarRepeticion(const String& resultado, TipoError tipo, int sensorId) {
  resultadoValidacion = resultado;
  Serial.println("Resultado Repeticion: " + resultado);
  mostrarResultadoLed(resultado == "BIEN");
  primeraRepHecha = true;
  faseRep = REPOSO;
  contadorRetorno = 0;
  contadorErrorVelocidad = 0;

  if (resultado == "BIEN") {
    contadorBien++;
  } else {
    contadorErrores++;
    registrarError(tipo, sensorId);
  }
  enviarContadorBLE();  // solo el conteo; el detalle va al final de la serie
}

void evaluarRepeticion(Ejercicio& ej, float velocidadActual) {
  if (!sensorPrincipalOK(ej)) {
    if (faseRep != REPOSO) {
      cerrarRepeticion("MAL (sensor principal desconectado)", ERR_DESCONEXION, ej.sensorPrincipal);
    }
    return;
  }

  float v = leerAngulo(ej.sensorPrincipal, ej.ejePrincipal);
  float p = progreso(v);

  bool caido = false;
  int posturaFalla = sensorPosturaFallido(ej, caido);
  bool okPostura = (posturaFalla < 0);

  String resultado = "";
  TipoError tipo = ERR_NINGUNO;
  int sensorFallido = -1;

  switch (faseRep) {
    case REPOSO:
      // Quieto cerca del cero: el cero se ajusta solo
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
        // Tiempo maximo permitido, segun la repeticion calibrada
        unsigned long duracionMax = (unsigned long)(duracionCalibrada * FACTOR_DURACION_MAX);

        if (p > picoAlcanzado) picoAlcanzado = p;

        if (subFaseRep == SUBIENDO && p < picoAlcanzado - MARGEN_SUBFASE) {
          subFaseRep = BAJANDO;
        }

        // Velocidad esperada segun la subfase: subiendo, positiva; volviendo, negativa.
        float velEnDireccion = direccionMovimiento * velocidadActual;
        float velEsperada = (subFaseRep == SUBIENDO) ? velEnDireccion : -velEnDireccion;

        // Solo se controla lejos de los extremos: en el vertice la velocidad cambia de
        // signo y el filtro tiene retraso (daria falsos errores).
        bool chequearVel = (subFaseRep == SUBIENDO) ? (p > 0.2 && p < 0.8)
                                                    : (p > 0.2 && p < picoAlcanzado - 0.15);
        if (chequearVel && velEsperada < -TOLERANCIA_VEL_DPS) {
          contadorErrorVelocidad++;
        } else {
          contadorErrorVelocidad = 0;
        }

        if (contadorErrorVelocidad >= MUESTRAS_CONFIRMACION) {
          resultado = "MAL (velocidad o sentido del movimiento incorrecto)";
          tipo = ERR_VELOCIDAD;
          sensorFallido = ej.sensorPrincipal;
        } else if (!okPostura) {
          if (caido) {
            resultado = "MAL (sensor desconectado)";
            tipo = ERR_DESCONEXION;
          } else {
            resultado = "MAL (postura)";
            tipo = ERR_POSTURA;
          }
          sensorFallido = posturaFalla;
        } else if (subFaseRep == BAJANDO && (p <= UMBRAL_RETORNO || fabs(v - baseLocal) <= TOLERANCIA_REPOSO_DEG)) {
          contadorRetorno++;
          if (contadorRetorno >= MUESTRAS_CONFIRMACION) {
            unsigned long duracion = millis() - tInicioRep;
            unsigned long duracionMin = (unsigned long)(duracionCalibrada * FACTOR_DURACION_MIN);
            if (duracionMin < T_MIN) duracionMin = T_MIN;

            if (picoAlcanzado < UMBRAL_PICO) {
              resultado = "MAL (rango de movimiento incompleto)";
              tipo = ERR_RANGO;
            } else if (duracion < duracionMin) {
              resultado = "MAL (muy rapido)";
              tipo = ERR_RAPIDO;
            } else if (duracion > duracionMax) {
              resultado = "MAL (muy lento)";
              tipo = ERR_LENTO;
            } else {
              resultado = "BIEN";
            }
          }
        } else {
          contadorRetorno = 0;
        }

        if (resultado == "" && millis() - tInicioRep > duracionMax + 1000) {
          resultado = "MAL (tiempo superado)";
          tipo = ERR_LENTO;
        }
        break;
      }
  }

  if (resultado != "") cerrarRepeticion(resultado, tipo, sensorFallido);
}

// ============================================================
// INICIO DE SERIE
// ============================================================
void resetearSerie() {
  faseRep = REPOSO;
  subFaseRep = SUBIENDO;
  contadorErrores = 0;
  contadorBien = 0;
  contadorErrorVelocidad = 0;
  contadorRetorno = 0;
  primeraRepHecha = false;

  errVelocidad = 0;
  errPostura[0] = errPostura[1] = errPostura[2] = 0;
  errRango = 0;
  errRapido = 0;
  errLento = 0;
  errDesconexion = 0;

  // Reinicia el estado de velocidad para no arrastrar datos de la serie anterior
  anguloAnteriorPrincipal = leerAngulo(ejercicioActual->sensorPrincipal, ejercicioActual->ejePrincipal);
  tAnguloAnteriorPrincipal = 0;
  velocidadPrincipal = 0;
}

// ============================================================
// BLE
// ============================================================
class MiServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
    Serial.println("App conectada por BLE.");
  }
  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
    Serial.print("App desconectada, motivo: 0x");
    Serial.println(reason, HEX);
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

// ============================================================
// MAQUINA GENERAL
// ============================================================
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
          if (calibrarEjercicio(*ejercicioActual, 8000)) {
            resetearSerie();
            tUltimoKeepalive = millis();
            estadoMaq_General = MEDICIONES;
            enviarFeedbackBLE("ESTADO:MEDICION_INICIADA");
            Serial.println(">>> ESTADO ACTUAL: MEDICIONES <<<");
          } else {
            mostrarResultadoLed(false);
            enviarDetenerAuxiliares(*ejercicioActual);
          }
        } else {
          mostrarResultadoLed(false);
          enviarFeedbackBLE("ERROR:Sensores auxiliares sin respuesta");
        }

        // Descarta los toques que hayan entrado durante el handshake o la calibracion
        flagBoton = false;
      }
      break;

    case MEDICIONES:
      recibirPaquetesUDP();

      // Keepalive: el auxiliar se apaga solo si deja de recibir esto
      if (millis() - tUltimoKeepalive >= INTERVALO_KEEPALIVE_MS) {
        tUltimoKeepalive = millis();
        enviarComandoUDPAuxiliares("INICIAR", *ejercicioActual);
      }

      if (flagMedicion) {
        flagMedicion = false;
        mediciones();
        recibirPaquetesUDP();
        float v = leerAngulo(ejercicioActual->sensorPrincipal, ejercicioActual->ejePrincipal);
        float velocidadActual = calcularVelocidad(v);
        evaluarRepeticion(*ejercicioActual, velocidadActual);
      }
      actualizarLedMedicion();

      if (flagBoton) {
        flagBoton = false;
        Serial.println("\n[BOTON PRESIONADO] -> Finalizando serie...");
        estadoMaq_General = ANALISIS_SERIE;
      }
      break;

    case ANALISIS_SERIE:
      resultadoValidacion = "SERIE TERMINADA: " + String(contadorBien) + " bien, " + String(contadorErrores) + " mal";
      Serial.println(resultadoValidacion);
      imprimirDetalleErrores();
      flagBoton = false;
      enviarDetenerAuxiliares(*ejercicioActual);
      feedbackEnviado = false;
      estadoMaq_General = C_APLICACION;
      Serial.println(">>> ESTADO ACTUAL: C_APLICACION <<<");
      break;

    case C_APLICACION:
      if (!feedbackEnviado) {
        enviarFeedbackBLE(resultadoValidacion);
        delay(50);  // separa las dos notificaciones para que no se pierda ninguna
        enviarDetalleErroresBLE();
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

// ============================================================
// DEEP SLEEP (encendido con pulsación larga para ESP32-S3)
// ============================================================
void irADormir() {
  delay(50);  // deja asentar el rebote antes de dormir

#if defined(CONFIG_IDF_TARGET_ESP32C3)
  esp_deep_sleep_enable_gpio_wakeup(1ULL << PIN_BOTON, ESP_GPIO_WAKEUP_GPIO_LOW);
#else
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_BOTON, 0);  // despierta con el pin en LOW
  // El pull-up interno se apaga durante el deep sleep: se mantiene en el dominio RTC
  rtc_gpio_pullup_en((gpio_num_t)PIN_BOTON);
  rtc_gpio_pulldown_dis((gpio_num_t)PIN_BOTON);
#endif
  esp_deep_sleep_start();
}

// ============================================================
// SETUP / LOOP
// ============================================================
void setup() {
  pinMode(PIN_BOTON, INPUT_PULLUP);
  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  setLED(0, 0);
  delay(50);  // deja estabilizar el pull-up antes de leer el boton

  // Encendido con pulsacion larga: el LED verde parpadea mientras cuenta los 3 s
  unsigned long tInicio = millis();
  bool encendido = false;
  while (digitalRead(PIN_BOTON) == LOW) {
    unsigned long transcurrido = millis() - tInicio;
    digitalWrite(PIN_LED_G, (transcurrido / 250) % 2);
    if (transcurrido >= TIEMPO_ENCENDIDO_MS) {
      encendido = true;
      break;
    }
    delay(10);
  }
  setLED(0, 0);

  if (!encendido && !MODO_DESARROLLO) irADormir();

  // Verde fijo hasta que suelte el boton (maximo 5 s, para no colgarse si el pin
  // quedara en LOW por cableado). Si no espera, esa pulsacion cuenta despues
  // como "iniciar serie".
  ledVerde();
  unsigned long tEspera = millis();
  while (digitalRead(PIN_BOTON) == LOW && millis() - tEspera < 5000) delay(10);
  delay(50);
  ledAmarillo();  // amarillo = arrancando

  Serial.begin(115200);
  unsigned long t = millis();
  while (!Serial && millis() - t < 1500) delay(10);
  Serial.println("\n==================================");
  Serial.println("  INICIANDO ESP32 PRINCIPAL      ");
  Serial.println("==================================");

  Wire.begin(44, 1);
  sensor.initialize();
  Serial.println("A: sensor OK");
  Serial.flush();

  // Access Point: canal 1 fijo, visible, maximo 4 clientes
  WiFi.mode(WIFI_AP);
  IPAddress local_ip(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  bool apOk = WiFi.softAP(ssid, password, 1, 0, 4);

  Serial.print("Access Point creado: ");
  Serial.println(apOk ? "SI" : "NO");
  Serial.print("IP del AP: ");
  Serial.println(WiFi.softAPIP());

  udp.begin(PuertoUDP);
  Serial.println("B: WiFi OK");
  Serial.flush();

  inicializarBLE();
  Serial.println("C: BLE OK");
  Serial.flush();

  calibrarOffsetGlobal();
  Serial.println("D: calibracion OK");
  Serial.flush();

  timerAntirrebote.attach_ms(1, funcionTimerAntirrebote);
  timerMedicion.attach_ms(INTERVALO_MEDICION, funcionTimerMedicion);
  Serial.println("SISTEMA LISTO EN ESTADO INICIALIZACION.");
  Serial.flush();

  ledVerde();  // verde fijo = listo
  estadoBoton = ESPERA;
  flagBoton = false;
}

void loop() {
  maquinaAntirrebote();
  actualizarHoldLed();
  Maq_General();
}