// ESP32-S3 (Principal) - Modo Access Point propio + BLE Android/Motorola + Diagnóstico Serial
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include <MPU6050.h>
#include "Wire.h"
#include "WiFi.h"
#include <WiFiUdp.h>
#include <Ticker.h>
#include <NimBLEDevice.h>
#include "I2Cdev.h"

const bool MODO_DESARROLLO = true;
const unsigned long TIEMPO_ENCENDIDO_MS = 3000;

struct Ejercicio {
  const char* nombre;
  uint8_t sensorPrincipal;
  uint8_t ejePrincipal;
  float minAngulo;
  float maxAngulo;
  uint8_t sensorPostura[2];
  uint8_t ejePostura[2];
  float centroPostura[2];
  float toleranciaPostura[2];
};

#define CANT_EJERCICIOS 5
Ejercicio ejercicios[CANT_EJERCICIOS] = {
  // { nombre, sensorPrincipal, ejePrincipal, min, max, {sensorPost1, sensorPost2}, {ejePost1, ejePost2}, ... }
  { "Curl de biceps", 0, 1, 0, 0, { 1, 1 }, { 0, 1 }, { 0, 0 }, { 0, 0 } },
  { "Triceps Nuca", 0, 1, 0, 0, { 1, 1 }, { 0, 1 }, { 0, 0 }, { 0, 0 } },
  { "Press de Banca", 0, 2, 0, 0, { 1, 2 }, { 0, 2 }, { 0, 0 }, { 0, 0 } },
  { "Sentadilla", 0, 0, 0, 0, { 1, 1 }, { 0, 1 }, { 0, 0 }, { 0, 0 } },
  { "Peso Muerto", 1, 0, 0, 0, { 0, 0 }, { 0, 1 }, { 0, 0 }, { 0, 0 } },
};

Ejercicio* ejercicioActual = &ejercicios[0];

// ============================================================
// HARDWARE Y RED (ACCESS POINT EN ESP32-S3)
// ============================================================
MPU6050 sensor;
WiFiUDP udp;
const uint16_t PuertoUDP = 8888;
IPAddress ipAux1(0, 0, 0, 0);
IPAddress ipAux2(0, 0, 0, 0);
IPAddress ipBroadcastAP(192, 168, 4, 255);

NimBLEServer* pServer = nullptr;
NimBLECharacteristic* pCharacteristic = nullptr;

Ticker timerAntirrebote;
Ticker timerMedicion;

#define PIN_BOTON 5
#define PIN_LED_R 2
#define PIN_LED_G 4

const char* ssid = "ESP32_C3_Server";
const char* password = "GRUPO3XX";

bool REQUERIR_AUXILIARES = true;

// ============================================================
// ESTADOS
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
// DATOS DE SENSORES Y DIAGNÓSTICO
// ============================================================
float inclX_local = 0, inclY_local = 0, inclZ_local = 0;
float inclX_aux1 = 0, inclY_aux1 = 0, inclZ_aux1 = 0;
float inclX_aux2 = 0, inclY_aux2 = 0, inclZ_aux2 = 0;
bool aux1_ok = false;
bool aux2_ok = false;
bool aux1_previo = false;
bool aux2_previo = false;
int estacionesPrevias = -1;
unsigned long tUltimoPaqueteAux1 = 0;
unsigned long tUltimoPaqueteAux2 = 0;
const unsigned long TIMEOUT_AUXILIAR_MS = 1000;

unsigned long tUltimoKeepalive = 0;
const unsigned long INTERVALO_KEEPALIVE_MS = 1000;
unsigned long tUltimoLogEstado = 0;

float offset_X = 0, offset_Y = 0, offset_Z = 0;
float anguloAnteriorPrincipal = 0;
unsigned long tAnguloAnteriorPrincipal = 0;
float velocidadPrincipal = 0;
const float ALPHA_VEL = 0.3;

// ============================================================
// ESTADO DE LA REPETICION
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
const float TOLERANCIA_REPOSO_DEG = 7.0;
const float TOLERANCIA_VEL_DPS = 20.0;
const float UMBRAL_SALIDA = 0.30;
const float UMBRAL_PICO = 0.45;
const float UMBRAL_RETORNO = 0.35;
const int MUESTRAS_CONFIRMACION = 3;

unsigned long tFinUltimaRep = 0;
const unsigned long T_MIN_ENTRE_REPS = 800;

unsigned long duracionCalibrada = 1500;
const float FACTOR_DURACION_MIN = 0.4;
const float FACTOR_DURACION_MAX = 3.5;
const unsigned long T_MIN = 200;

int contadorRetorno = 0;
int contadorErrorVelocidad = 0;
bool primeraRepHecha = false;
bool esperandoRetornoAInicio = false;
bool avisoRetornoEnviado = false;
unsigned long tUltimoDentroReposo = 0;
unsigned long tPrimerRetorno = 0;
float pInicioRep = 0;
float vSuave = 0;
float recorrido = 0;

int contadorErrorRango = 0;
int contadorErrorPostura = 0;
const int MUESTRAS_CONFIRM_RANGO = 5;    // Antes: 4
const int MUESTRAS_CONFIRM_POSTURA = 2;  // Antes: 3 (detecta inclinación al instante)
const int MUESTRAS_CONFIRM_VEL = 4;
const float RATIO_SUAVIDAD_MAX = 2.0;

int errExceso = 0;
int errTemblor = 0;

unsigned long tiempoUltimoMovimiento = 0;
float anguloAnteriorInactividad = 0.0;
const float UMBRAL_MOVIMIENTO = 1.8;
const unsigned long TIEMPO_MAX_INACTIVIDAD = 4500;

// ============================================================
// CONTADORES Y ERRORES
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
                 ERR_DESCONEXION,
                 ERR_INACTIVO,
                 ERR_EXCESO,
                 ERR_TEMBLOR };

int errVelocidad = 0;
int errPostura[3] = { 0, 0, 0 };
int errRango = 0;
int errRapido = 0;
int errLento = 0;
int errDesconexion = 0;
int errInactivo = 0;

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

String obtenerDetalleErrores() {
  String reporte = "";
  if (errRapido > 0) reporte += "Muy rapido. ";
  if (errLento > 0) reporte += "Muy lento. ";
  if (errRango > 0) reporte += "Recorrido corto. ";
  if (errVelocidad > 0) reporte += "Evita tirones. ";
  if (errPostura[0] > 0 || errPostura[1] > 0 || errPostura[2] > 0) reporte += "Mala postura. ";
  if (errDesconexion > 0) reporte += "Sensor caido. ";
  if (errInactivo > 0) reporte += "Inactividad. ";
  if (errExceso > 0) reporte += "Te pasaste. ";
  if (errTemblor > 0) reporte += "Movimiento brusco. ";
  if (reporte == "") reporte = "Excelente serie! ";
  return reporte;
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
    case ERR_INACTIVO: errInactivo++; break;
    case ERR_EXCESO: errExceso++; break;
    case ERR_TEMBLOR: errTemblor++; break;
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

void actualizarHoldLed() {
  if (ledEnHold && millis() - tHoldLed >= DURACION_HOLD_LED) ledEnHold = false;
}

void actualizarLedMedicion() {
  if (ledEnHold) return;
  if (faseRep == REPOSO) {
    if (primeraRepHecha) ledAmarillo();
    else ledVerde();
  } else ledVerde();
}

// ============================================================
// BOTON Y SENSORES
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
        Serial.println("[BOTON] Pulsacion detectada.");
        estadoBoton = ESPERA;
      }
      break;
  }
}
void funcionTimerAntirrebote() {
  msBoton++;
}
void funcionTimerMedicion() {
  flagMedicion = true;
}

void calibrarOffsetGlobal() {
  Serial.println("[MPU-LOCAL] Calibrando offset inicial...");
  float sumaX = 0, sumaY = 0, sumaZ = 0;
  int muestrasValidas = 0;
  for (int i = 0; i < 100; i++) {
    int16_t ax, ay, az;
    sensor.getAcceleration(&ax, &ay, &az);
    if (ax != 0 || ay != 0 || az != 0) {
      float x = (ax / 16384.0) * 9.81;
      float y = (ay / 16384.0) * 9.81;
      float z = (az / 16384.0) * 9.81;
      sumaX += atan2(x, sqrt(y * y + z * z)) * 180.0 / PI;
      sumaY += atan2(y, sqrt(x * x + z * z)) * 180.0 / PI;
      sumaZ += atan2(z, sqrt(x * x + y * y)) * 180.0 / PI;
      muestrasValidas++;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  if (muestrasValidas > 0) {
    offset_X = sumaX / muestrasValidas;
    offset_Y = sumaY / muestrasValidas;
    offset_Z = sumaZ / muestrasValidas;
    Serial.printf("[MPU-LOCAL] Calibrado OK (%d muestras) -> Offsets X:%.2f Y:%.2f Z:%.2f\n", muestrasValidas, offset_X, offset_Y, offset_Z);
  } else {
    Serial.println("[MPU-LOCAL] ADVERTENCIA: 0 muestras validas al calibrar (revisar I2C local).");
  }
}

void mediciones() {
  int16_t ax, ay, az;
  sensor.getAcceleration(&ax, &ay, &az);
  if (ax == 0 && ay == 0 && az == 0) return;

  float x = (ax / 16384.0) * 9.81;
  float y = (ay / 16384.0) * 9.81;
  float z = (az / 16384.0) * 9.81;
  inclX_local = atan2(x, sqrt(y * y + z * z)) * 180.0 / PI - offset_X;
  inclY_local = atan2(y, sqrt(x * x + z * z)) * 180.0 / PI - offset_Y;
  inclZ_local = atan2(z, sqrt(x * x + y * y)) * 180.0 / PI - offset_Z;
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
  if (amplitudObjetivo == 0) return 0;
  return direccionMovimiento * (v - baseLocal) / amplitudObjetivo;
}

// ============================================================
// RED Y AUXILIARES (IPs ESTÁTICAS 192.168.4.2 y 192.168.4.3)
// ============================================================
void recibirPaquetesUDP() {
  int packetSize = udp.parsePacket();
  while (packetSize > 0) {
    IPAddress remoteIP = udp.remoteIP();
    char buffer[64] = { 0 };
    udp.read(buffer, sizeof(buffer) - 1);
    String payload = String(buffer);

    int rolAux = 0;
    if (remoteIP != IPAddress(0, 0, 0, 0) && remoteIP != WiFi.softAPIP()) {
      if (ipAux1 == remoteIP) {
        rolAux = 1;
        tUltimoPaqueteAux1 = millis();
      } else if (ipAux2 == remoteIP) {
        rolAux = 2;
        tUltimoPaqueteAux2 = millis();
      } else if (ipAux1 == IPAddress(0, 0, 0, 0)) {
        ipAux1 = remoteIP;
        rolAux = 1;
        tUltimoPaqueteAux1 = millis();
        Serial.print("[RED] Primer modulo conectado -> Asignado como AUXILIAR 1 (IP: ");
        Serial.print(ipAux1);
        Serial.println(")");
      } else if (ipAux2 == IPAddress(0, 0, 0, 0)) {
        ipAux2 = remoteIP;
        rolAux = 2;
        tUltimoPaqueteAux2 = millis();
        Serial.print("[RED] Segundo modulo conectado -> Asignado como AUXILIAR 2 (IP: ");
        Serial.print(ipAux2);
        Serial.println(")");
      }
    }

    if (payload == "ERROR_I2C") {
      Serial.printf("[UDP-ALERTA] Auxiliar %d (IP %s) reporta ERROR_I2C en su MPU6050!\n",
                    rolAux, remoteIP.toString().c_str());
    } else if (payload != "NOMIDIENDO" && rolAux > 0) {
      int c1 = payload.indexOf(',');
      int c2 = payload.indexOf(',', c1 + 1);
      if (c1 > 0 && c2 > c1) {
        float x = payload.substring(0, c1).toFloat();
        float y = payload.substring(c1 + 1, c2).toFloat();
        float z = payload.substring(c2 + 1).toFloat();
        if (x != 0.0 || y != 0.0 || z != 0.0) {
          if (rolAux == 1) {
            inclX_aux1 = x;
            inclY_aux1 = y;
            inclZ_aux1 = z;
          } else if (rolAux == 2) {
            inclX_aux2 = x;
            inclY_aux2 = y;
            inclZ_aux2 = z;
          }
        }
      }
    }
    packetSize = udp.parsePacket();
  }

  aux1_ok = (ipAux1 != IPAddress(0, 0, 0, 0)) && (millis() - tUltimoPaqueteAux1 < TIMEOUT_AUXILIAR_MS);
  aux2_ok = (ipAux2 != IPAddress(0, 0, 0, 0)) && (millis() - tUltimoPaqueteAux2 < TIMEOUT_AUXILIAR_MS);

  if (!aux1_ok && ipAux1 != IPAddress(0, 0, 0, 0)) {
    Serial.printf("[RED] Auxiliar 1 (%s) DESCONECTADO. Ranura liberada.\n", ipAux1.toString().c_str());
    ipAux1 = IPAddress(0, 0, 0, 0);
    inclX_aux1 = inclY_aux1 = inclZ_aux1 = 0;
  }
  if (!aux2_ok && ipAux2 != IPAddress(0, 0, 0, 0)) {
    Serial.printf("[RED] Auxiliar 2 (%s) DESCONECTADO. Ranura liberada.\n", ipAux2.toString().c_str());
    ipAux2 = IPAddress(0, 0, 0, 0);
    inclX_aux2 = inclY_aux2 = inclZ_aux2 = 0;
  }

  if (aux1_ok != aux1_previo) {
    aux1_previo = aux1_ok;
    if (aux1_ok) Serial.printf("[RED] Auxiliar 1 (%s) ONLINE.\n", ipAux1.toString().c_str());
  }
  if (aux2_ok != aux2_previo) {
    aux2_previo = aux2_ok;
    if (aux2_ok) Serial.printf("[RED] Auxiliar 2 (%s) ONLINE.\n", ipAux2.toString().c_str());
  }
}

bool sensorUsado(Ejercicio& ej, uint8_t sensorId) {
  if (ej.sensorPrincipal == sensorId) return true;
  for (int i = 0; i < 2; i++)
    if (ej.sensorPostura[i] == sensorId) return true;
  return false;
}

void seleccionarEjercicio() {
  if (codigo < 1 || codigo > CANT_EJERCICIOS) ejercicioActual = &ejercicios[0];
  else ejercicioActual = &ejercicios[codigo - 1];
  Serial.print("[EJERCICIO] Seleccionado: ");
  Serial.println(ejercicioActual->nombre);
}

void enviarComandoUDPAuxiliares(const char* cmd, Ejercicio& ej) {
  bool enviadoDirecto = false;

  if (sensorUsado(ej, 1) && ipAux1 != IPAddress(0, 0, 0, 0)) {
    udp.beginPacket(ipAux1, PuertoUDP);
    udp.print(cmd);
    udp.endPacket();
    enviadoDirecto = true;
  }
  if (sensorUsado(ej, 2) && ipAux2 != IPAddress(0, 0, 0, 0)) {
    udp.beginPacket(ipAux2, PuertoUDP);
    udp.print(cmd);
    udp.endPacket();
    enviadoDirecto = true;
  }

  if (!enviadoDirecto || strcmp(cmd, "DETENER") == 0) {
    udp.beginPacket(ipBroadcastAP, PuertoUDP);
    udp.print(cmd);
    udp.endPacket();
  }
}

void enviarDetenerAuxiliares(Ejercicio& ej) {
  Serial.println("[UDP] Enviando orden DETENER a auxiliares...");
  for (int i = 0; i < 3; i++) {
    enviarComandoUDPAuxiliares("DETENER", ej);
    delay(20);
  }
}

bool iniciarMedicionEnAuxiliares(Ejercicio& ej) {
  bool necesitaAux1 = true;  // Siempre exige al menos 1 auxiliar conectado
  bool necesitaAux2 = sensorUsado(ej, 2);

  unsigned long inicioHandshake = millis();
  while (millis() - inicioHandshake < 2500) {
    maquinaAntirrebote();
    for (int i = 0; i < 3; i++) {
      enviarComandoUDPAuxiliares("INICIAR", ej);
      vTaskDelay(pdMS_TO_TICKS(20));
    }
    unsigned long tPausa = millis();
    while (millis() - tPausa < 150) {
      maquinaAntirrebote();
      recibirPaquetesUDP();
      vTaskDelay(pdMS_TO_TICKS(10));
    }
    if ((!necesitaAux1 || aux1_ok) && (!necesitaAux2 || aux2_ok)) {
      return true;
    }
  }
  return false;  // Bloquea el inicio si falta el auxiliar
}

// ============================================================
// CALIBRACION
// ============================================================
bool calibrarEjercicio(Ejercicio& ej, unsigned long duracionMaximaMs) {
  Serial.println("[CALIB] Fase 1: Quedate quieto (1.5s)...");
  enviarFeedbackBLE("Quedate quieto");

  float reposoValor = 0;
  float reposoPostura[2] = { 0, 0 };
  int muestras = 0;

  unsigned long tReposo = millis();
  while (millis() - tReposo < 1500) {
    maquinaAntirrebote();
    mediciones();
    recibirPaquetesUDP();
    if (millis() - tReposo > 500) {
      reposoValor += leerAngulo(ej.sensorPrincipal, ej.ejePrincipal);
      for (int p = 0; p < 2; p++) {
        if (ej.sensorPostura[p] != 99) reposoPostura[p] += leerAngulo(ej.sensorPostura[p], ej.ejePostura[p]);
      }
      muestras++;
    }
    vTaskDelay(pdMS_TO_TICKS(20));
    yield();
  }

  if (muestras > 0) {
    reposoValor /= muestras;
    for (int p = 0; p < 2; p++)
      if (ej.sensorPostura[p] != 99) reposoPostura[p] /= muestras;
  }

  Serial.printf("[CALIB] Reposo base: %.2f deg. Fase 2: Hace 1 rep lenta...\n", reposoValor);
  enviarFeedbackBLE("Hace 1 rep lenta para grabar");

  float minV = reposoValor, maxV = reposoValor;
  float amplitudMax = 0;
  float lateralMax[2] = { 0, 0 };
  bool salioDeReposo = false;
  bool retornoCompletado = false;
  unsigned long tInicioMovCalib = 0;
  unsigned long tRetornoCalib = 0;

  unsigned long inicio = millis();
  float velCalibracion = 0;
  float anguloAnterior = reposoValor;
  unsigned long tAnterior = millis();

  while (true) {
    unsigned long ahora = millis();

    if (ahora - inicio > duracionMaximaMs) {
      if (amplitudMax >= 10.0) retornoCompletado = true;
      break;
    }

    maquinaAntirrebote();
    mediciones();
    recibirPaquetesUDP();

    float vActual = leerAngulo(ej.sensorPrincipal, ej.ejePrincipal);
    float distancia = fabs(vActual - reposoValor);

    float dt = (ahora - tAnterior) / 1000.0;
    if (dt > 0.05) {
      velCalibracion = fabs(vActual - anguloAnterior) / dt;
      anguloAnterior = vActual;
      tAnterior = ahora;
    }

    if (!salioDeReposo && distancia >= 5.0) {
      salioDeReposo = true;
      tInicioMovCalib = ahora;
    }

    if (vActual < minV) minV = vActual;
    if (vActual > maxV) maxV = vActual;
    if (distancia > amplitudMax) amplitudMax = distancia;

    for (int p = 0; p < 2; p++) {
      if (ej.sensorPostura[p] == 99) continue;
      float d = fabs(leerAngulo(ej.sensorPostura[p], ej.ejePostura[p]) - reposoPostura[p]);
      if (d > lateralMax[p]) lateralMax[p] = d;
    }

    if (salioDeReposo && amplitudMax >= 10.0 && (ahora - tInicioMovCalib) >= 1500) {
      if (distancia <= (amplitudMax * 0.45) || (distancia < (amplitudMax * 0.8) && velCalibracion < 5.0)) {
        tRetornoCalib = ahora;
        retornoCompletado = true;
        break;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(30));
    yield();
  }

  if (!retornoCompletado || amplitudMax < 10.0) {
    Serial.println("[CALIB] ERROR: Repeticion no detectada (amplitud insuficiente).");
    enviarFeedbackBLE("Err: Repeticion no detectada");
    return false;
  }

  ej.minAngulo = minV;
  ej.maxAngulo = maxV;
  float picoValor = (fabs(maxV - reposoValor) > fabs(minV - reposoValor)) ? maxV : minV;
  baseLocal = reposoValor;
  amplitudObjetivo = max(1.0f, fabs(picoValor - reposoValor));
  direccionMovimiento = (picoValor >= reposoValor) ? 1 : -1;

  for (int p = 0; p < 2; p++) {
    if (ej.sensorPostura[p] == 99) continue;
    ej.centroPostura[p] = reposoPostura[p];
    float tol = lateralMax[p] * 1.2;  // Antes: 1.5
    if (tol < 6.0) tol = 6.0;         // Antes: 7.0
    if (tol > 11.0) tol = 11.0;       // Antes: 20.0 (más de 11° hacia adelante = MAL)
    ej.toleranciaPostura[p] = tol;
  }
  duracionCalibrada = max(500UL, tRetornoCalib - tInicioMovCalib);

  Serial.printf("[CALIB] Completada! Amplitud: %.2f deg | Duracion: %lu ms. Toca el boton para iniciar.\n", amplitudObjetivo, duracionCalibrada);
  enviarFeedbackBLE("Calibrado. Toca el boton");
  flagBoton = false;
  while (true) {
    maquinaAntirrebote();
    recibirPaquetesUDP();
    if (flagBoton) {
      flagBoton = false;
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(20));
    yield();
  }
  return true;
}

// ============================================================
// VALIDACIONES Y LOGICA PRINCIPAL
// ============================================================
int sensorPosturaFallido(Ejercicio& ej, bool& caido) {
  caido = false;
  for (int i = 0; i < 2; i++) {
    uint8_t s = ej.sensorPostura[i];
    if (s == 99) continue;
    if ((s == 1 && !aux1_ok) || (s == 2 && !aux2_ok)) {
      if (!REQUERIR_AUXILIARES) continue;
      caido = true;
      return s;
    }
    float vp = leerAngulo(s, ej.ejePostura[i]);
    if (fabs(vp - ej.centroPostura[i]) > ej.toleranciaPostura[i]) return s;
  }
  return -1;
}

void cerrarRepeticion(const String& resultado, TipoError tipo, int sensorId) {
  resultadoValidacion = resultado;
  mostrarResultadoLed(resultado == "BIEN");
  primeraRepHecha = true;
  faseRep = REPOSO;
  tFinUltimaRep = millis();
  esperandoRetornoAInicio = true;
  avisoRetornoEnviado = false;
  contadorRetorno = 0;
  contadorErrorVelocidad = 0;

  if (resultado == "BIEN") contadorBien++;
  else {
    contadorErrores++;
    registrarError(tipo, sensorId);
  }
  Serial.printf("[REP] Resultado: %s | Total BIEN: %d | MAL: %d\n", resultado.c_str(), contadorBien, contadorErrores);
  enviarContadorBLE();
}

void evaluarRepeticion(Ejercicio& ej, float velocidadActual) {
  if (!aux1_ok || (sensorUsado(ej, 2) && !aux2_ok)) {
    registrarError(ERR_DESCONEXION, (!aux1_ok) ? 1 : 2);
    mostrarResultadoLed(false);
    enviarDetenerAuxiliares(ej);
    enviarFeedbackBLE("ERR: Auxiliar desconectado");
    estadoMaq_General = INICIALIZACION;
    return;
  }
  if ((ej.sensorPrincipal == 1 && !aux1_ok) || (ej.sensorPrincipal == 2 && !aux2_ok)) {
    if (faseRep != REPOSO) cerrarRepeticion("MAL (sensor desconectado)", ERR_DESCONEXION, ej.sensorPrincipal);
    return;
  }

  float v = leerAngulo(ej.sensorPrincipal, ej.ejePrincipal);
  float p = progreso(v);
  bool caido = false;
  int posturaFalla = sensorPosturaFallido(ej, caido);

  String resultado = "";
  TipoError tipo = ERR_NINGUNO;
  int sensorFallido = -1;

  switch (faseRep) {
    case REPOSO:
      if (fabs(v - baseLocal) < TOLERANCIA_REPOSO_DEG) baseLocal = ALPHA_BASE_REPOSO * v + (1 - ALPHA_BASE_REPOSO) * baseLocal;
      if (fabs(v - baseLocal) < 8.0) tUltimoDentroReposo = millis();

      if (p < UMBRAL_RETORNO || fabs(v - baseLocal) <= TOLERANCIA_REPOSO_DEG) {
        esperandoRetornoAInicio = false;
        avisoRetornoEnviado = false;
      } else if (esperandoRetornoAInicio && !avisoRetornoEnviado && (millis() - tFinUltimaRep > 4000)) {
        enviarFeedbackBLE("Vuelve a posicion inicial");
        avisoRetornoEnviado = true;
      }

      if (!esperandoRetornoAInicio && p >= UMBRAL_SALIDA && (millis() - tFinUltimaRep > T_MIN_ENTRE_REPS)) {
        faseRep = EN_MOVIMIENTO;
        subFaseRep = SUBIENDO;
        tInicioRep = (millis() - tUltimoDentroReposo < 1500) ? tUltimoDentroReposo : millis();
        tiempoUltimoMovimiento = millis();
        anguloAnteriorInactividad = v;
        picoAlcanzado = p;
        pInicioRep = p;
        vSuave = v;
        recorrido = 0;
        contadorRetorno = 0;
        contadorErrorVelocidad = 0;
        contadorErrorRango = 0;
        contadorErrorPostura = 0;
      }
      break;

    case EN_MOVIMIENTO:
      unsigned long duracionMax = (unsigned long)(duracionCalibrada * FACTOR_DURACION_MAX);
      if (fabs(v - anguloAnteriorInactividad) > UMBRAL_MOVIMIENTO) {
        tiempoUltimoMovimiento = millis();
        anguloAnteriorInactividad = v;
      }

      float vPrev = vSuave;
      vSuave = 0.5 * v + 0.5 * vSuave;
      recorrido += fabs(vSuave - vPrev);

      float margenSub = max(MARGEN_SUBFASE, 3.0f / amplitudObjetivo);
      if (p > picoAlcanzado) {
        picoAlcanzado = p;
        subFaseRep = SUBIENDO;
      } else if (subFaseRep == SUBIENDO && p < picoAlcanzado - margenSub) subFaseRep = BAJANDO;

      float velEnDireccion = direccionMovimiento * velocidadActual;
      float velEsperada = (subFaseRep == SUBIENDO) ? velEnDireccion : -velEnDireccion;
      bool chequearVel = (subFaseRep == SUBIENDO) ? (p > 0.2 && p < 0.8) : (p > 0.2 && p < picoAlcanzado - 0.15);
      if (chequearVel && velEsperada < -TOLERANCIA_VEL_DPS) contadorErrorVelocidad++;
      else contadorErrorVelocidad = 0;

      if (v < ej.minAngulo - 10.0 || v > ej.maxAngulo + 10.0) contadorErrorRango++;
      else contadorErrorRango = 0;

      if (posturaFalla >= 0) contadorErrorPostura++;
      else contadorErrorPostura = 0;

      if (millis() - tiempoUltimoMovimiento > TIEMPO_MAX_INACTIVIDAD) {
        resultado = "MAL (Inactivo o vibrando)";
        tipo = ERR_INACTIVO;
        sensorFallido = ej.sensorPrincipal;
      } else if (contadorErrorRango >= MUESTRAS_CONFIRM_RANGO) {
        resultado = "MAL (te pasaste del recorrido)";
        tipo = ERR_EXCESO;
        sensorFallido = ej.sensorPrincipal;
      } else if (contadorErrorVelocidad >= MUESTRAS_CONFIRM_VEL) {
        resultado = "MAL (sentido incorrecto)";
        tipo = ERR_VELOCIDAD;
        sensorFallido = ej.sensorPrincipal;
      } else if (contadorErrorPostura >= MUESTRAS_CONFIRM_POSTURA) {
        if (caido) {
          resultado = "MAL (sensor desconectado)";
          tipo = ERR_DESCONEXION;
        } else {
          resultado = "MAL (postura)";
          tipo = ERR_POSTURA;
        }
        sensorFallido = posturaFalla;
      } else if (subFaseRep == BAJANDO && (p <= UMBRAL_RETORNO || fabs(v - baseLocal) <= TOLERANCIA_REPOSO_DEG)) {
        if (contadorRetorno == 0) tPrimerRetorno = millis();
        if (++contadorRetorno >= MUESTRAS_CONFIRMACION) {
          unsigned long duracion = tPrimerRetorno - tInicioRep;
          unsigned long duracionMin = max(T_MIN, (unsigned long)(duracionCalibrada * FACTOR_DURACION_MIN));
          float idealGrados = ((picoAlcanzado - pInicioRep) + (picoAlcanzado - p)) * amplitudObjetivo;
          float ratio = (idealGrados > 1.0) ? (recorrido / idealGrados) : 1.0;

          if (picoAlcanzado < UMBRAL_PICO) {
            resultado = "MAL (incompleto)";
            tipo = ERR_RANGO;
          } else if (ratio > RATIO_SUAVIDAD_MAX) {
            resultado = "MAL (movimiento brusco)";
            tipo = ERR_TEMBLOR;
          } else if (duracion < duracionMin) {
            resultado = "MAL (muy rapido)";
            tipo = ERR_RAPIDO;
          } else if (duracion > duracionMax) {
            resultado = "MAL (muy lento)";
            tipo = ERR_LENTO;
          } else resultado = "BIEN";
        }
      } else contadorRetorno = 0;

      if (resultado == "" && millis() - tInicioRep > duracionMax + 1000) {
        resultado = "MAL (tiempo superado)";
        tipo = ERR_LENTO;
      }

      if (resultado != "") cerrarRepeticion(resultado, tipo, sensorFallido);
      break;
  }
}

void resetearSerie() {
  faseRep = REPOSO;
  subFaseRep = SUBIENDO;
  contadorErrores = 0;
  contadorBien = 0;
  contadorErrorVelocidad = 0;
  contadorRetorno = 0;
  primeraRepHecha = false;
  esperandoRetornoAInicio = false;
  tUltimoDentroReposo = millis();
  avisoRetornoEnviado = false;
  contadorErrorRango = 0;
  contadorErrorPostura = 0;
  errExceso = 0;
  errTemblor = 0;
  errVelocidad = 0;
  errPostura[0] = errPostura[1] = errPostura[2] = 0;
  errRango = 0;
  errRapido = 0;
  errLento = 0;
  errDesconexion = 0;
  errInactivo = 0;
  anguloAnteriorPrincipal = leerAngulo(ejercicioActual->sensorPrincipal, ejercicioActual->ejePrincipal);
  tAnguloAnteriorPrincipal = 0;
  velocidadPrincipal = 0;
}

// ============================================================
// BLE COMPATIBLE CON ANDROID Y MOTOROLA
// ============================================================
class MiServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
    Serial.println("[BLE] Celular Android/Motorola CONECTADO!");
    pServer->updateConnParams(connInfo.getConnHandle(), 12, 24, 0, 100);
  }
  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
    Serial.printf("[BLE] Celular DESCONECTADO (razon: %d). Reiniciando publicidad...\n", reason);
    NimBLEDevice::startAdvertising();
  }
};

class MiCharacteristicCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
    std::string valor = pCharacteristic->getValue();
    if (valor.length() > 0) {
      codigo = atoi(valor.c_str());
      Serial.printf("[BLE] Comando recibido desde App: '%s' -> Codigo ejercicio = %d\n", valor.c_str(), codigo);
    }
  }
};

void inicializarBLE() {
  Serial.println("[BLE] Iniciando servidor NimBLE 'Techeck_V2'...");
  NimBLEDevice::init("Techeck_V2");
  NimBLEDevice::setMTU(128);
  NimBLEDevice::setSecurityAuth(false, false, false);

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
  NimBLEAdvertisementData advData;
  advData.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
  advData.addServiceUUID(pService->getUUID());
  pAdvertising->setAdvertisementData(advData);

  NimBLEAdvertisementData scanData;
  scanData.setName("Techeck_V2");
  pAdvertising->setScanResponseData(scanData);

  pAdvertising->start();
  Serial.println("[BLE] Visible y anunciando para dispositivos Android/Motorola.");
}

unsigned long tUltimoAvisoBLE = 0;

void Maq_General() {
  switch (estadoMaq_General) {
    case INICIALIZACION:
      recibirPaquetesUDP();
      if (!ledEnHold) {
        if (aux1_ok) ledVerde();
        else ledRojo();  // LED rojo fijo mientras no haya auxiliar conectado
      }
      if (millis() - tUltimoAvisoBLE > 3000) {
        if (aux1_ok) {
          enviarFeedbackBLE("Auxiliar OK. Toca boton");
        } else {
          enviarFeedbackBLE("ERR: Sin auxiliar conectado");
        }
        tUltimoAvisoBLE = millis();
      }
      if (flagBoton) {
        flagBoton = false;
        ledAmarillo();
        seleccionarEjercicio();
        if (iniciarMedicionEnAuxiliares(*ejercicioActual)) {
          ledVerde();
          if (calibrarEjercicio(*ejercicioActual, 15000)) {
            resetearSerie();
            tUltimoKeepalive = millis();
            estadoMaq_General = MEDICIONES;
            enviarFeedbackBLE("Calibrado. Entrena");
          } else {
            mostrarResultadoLed(false);
            enviarDetenerAuxiliares(*ejercicioActual);
          }
        } else {
          mostrarResultadoLed(false);
          enviarFeedbackBLE("ERR: No se conecto auxiliar");
        }
        flagBoton = false;
      }
      break;

    case MEDICIONES:
      recibirPaquetesUDP();
      if (millis() - tUltimoKeepalive >= INTERVALO_KEEPALIVE_MS) {
        tUltimoKeepalive = millis();
        enviarComandoUDPAuxiliares("INICIAR", *ejercicioActual);
      }
      if (flagMedicion) {
        flagMedicion = false;
        mediciones();
        recibirPaquetesUDP();
        evaluarRepeticion(*ejercicioActual, calcularVelocidad(leerAngulo(ejercicioActual->sensorPrincipal, ejercicioActual->ejePrincipal)));
      }
      actualizarLedMedicion();
      if (flagBoton) {
        flagBoton = false;
        feedbackEnviado = false;
        estadoMaq_General = ANALISIS_SERIE;
        Serial.println("[ESTADO] -> ANALISIS_SERIE");
      }
      break;

    case ANALISIS_SERIE:
      if (!feedbackEnviado) {
        resultadoValidacion = "BIEN: " + String(contadorBien) + " | MAL: " + String(contadorErrores);
        enviarDetenerAuxiliares(*ejercicioActual);
        enviarFeedbackBLE(resultadoValidacion + " -> Toca boton");
        feedbackEnviado = true;
      }
      if (flagBoton) {
        flagBoton = false;
        feedbackEnviado = false;
        estadoMaq_General = C_APLICACION;
        Serial.println("[ESTADO] -> C_APLICACION (Reporte de errores)");
      }
      break;

    case C_APLICACION:
      if (!feedbackEnviado) {
        String reporte = obtenerDetalleErrores();
        Serial.print("[REPORTE] ");
        Serial.println(reporte);
        enviarFeedbackBLE(reporte + " -> Toca reiniciar");
        feedbackEnviado = true;
      }
      if (flagBoton) {
        flagBoton = false;
        estadoMaq_General = INICIALIZACION;
        Serial.println("[ESTADO] -> INICIALIZACION");
      }
      break;
  }
}

// ============================================================
// SETUP / LOOP
// ============================================================
void irADormir() {
  Serial.println("[SISTEMA] Entrando en Deep Sleep...");
  delay(50);
#if defined(CONFIG_IDF_TARGET_ESP32C3)
  esp_deep_sleep_enable_gpio_wakeup(1ULL << PIN_BOTON, ESP_GPIO_WAKEUP_GPIO_LOW);
#else
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_BOTON, 0);
  rtc_gpio_pullup_en((gpio_num_t)PIN_BOTON);
  rtc_gpio_pulldown_dis((gpio_num_t)PIN_BOTON);
#endif
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n==================================================");
  Serial.println("   INICIANDO ESP32-S3 PRINCIPAL (MODO ACCESS POINT)");
  Serial.println("==================================================");

  pinMode(PIN_BOTON, INPUT_PULLUP);
  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  setLED(0, 0);
  delay(50);

  unsigned long tInicio = millis();
  bool encendido = false;
  while (digitalRead(PIN_BOTON) == LOW) {
    digitalWrite(PIN_LED_G, ((millis() - tInicio) / 250) % 2);
    if (millis() - tInicio >= TIEMPO_ENCENDIDO_MS) {
      encendido = true;
      break;
    }
    delay(10);
  }
  setLED(0, 0);
  if (!encendido && !MODO_DESARROLLO) irADormir();

  ledVerde();
  unsigned long tEspera = millis();
  while (digitalRead(PIN_BOTON) == LOW && millis() - tEspera < 5000) delay(10);
  delay(50);
  ledAmarillo();
  delay(2000);

  Serial.println("[I2C] Iniciando MPU6050 local (SDA=44, SCL=1)...");
  Wire.begin(44, 1);
  sensor.initialize();
  if (sensor.testConnection()) {
    Serial.println("[MPU-LOCAL] CONECTADO correctamente!");
  } else {
    Serial.println("[MPU-LOCAL] ERROR: No responde el sensor MPU6050 local!");
  }

  Serial.printf("[WIFI-AP] Creando punto de acceso '%s' en canal 1...\n", ssid);
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP(ssid, password, 1, 0, 4);

  Serial.print("[WIFI-AP] Access Point activo! IP Servidor: ");
  Serial.println(WiFi.softAPIP());

  udp.begin(PuertoUDP);
  Serial.printf("[UDP] Escuchando en puerto %d\n", PuertoUDP);

  inicializarBLE();
  calibrarOffsetGlobal();

  timerAntirrebote.attach_ms(1, funcionTimerAntirrebote);
  timerMedicion.attach_ms(INTERVALO_MEDICION, funcionTimerMedicion);
  ledVerde();
  estadoBoton = ESPERA;
  flagBoton = false;
  Serial.println("[SISTEMA] Setup finalizado. Listo en estado INICIALIZACION.\n");
}

void loop() {
  // Detectar si los ESP-01 se asocian a nivel Wi-Fi con el AP del ESP32-S3
  int estacionesActuales = WiFi.softAPgetStationNum();
  if (estacionesActuales != estacionesPrevias) {
    estacionesPrevias = estacionesActuales;
    Serial.printf("[WIFI-AP] Dispositivos Wi-Fi asociados al ESP32-S3: %d\n", estacionesActuales);
  }

  // Reporte periodico de diagnostico cada 3 segundos
  if (millis() - tUltimoLogEstado >= 3000) {
    tUltimoLogEstado = millis();
    Serial.printf("[DIAG] AP_Stations:%d | BLE_Clients:%d | Aux1:%s (X:%.1f) | Aux2:%s (X:%.1f) | Local_X:%.1f\n",
                  WiFi.softAPgetStationNum(),
                  (pServer != nullptr) ? pServer->getConnectedCount() : 0,
                  aux1_ok ? "OK" : "OFF", inclX_aux1,
                  aux2_ok ? "OK" : "OFF", inclX_aux2,
                  inclX_local);
  }

  maquinaAntirrebote();
  actualizarHoldLed();
  Maq_General();
}