// ESP32-S3 (Principal) - UDP + BLE + Control de Estados
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include <MPU6050.h>
#include "Wire.h"
#include "WiFi.h"
#include <NetworkUdp.h>
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

void maquinaAntirrebote();  // declaracion previa

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

unsigned long tUltimoKeepalive = 0;
const unsigned long INTERVALO_KEEPALIVE_MS = 1000;

float offset_X, offset_Y, offset_Z;

float anguloAnteriorPrincipal = 0;
unsigned long tAnguloAnteriorPrincipal = 0;
float velocidadPrincipal = 0;
const float ALPHA_VEL = 0.4;

// ============================================================
// ESTADO DE LA REPETICION Y PARAMETROS
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
const float UMBRAL_SALIDA = 0.35;
const float UMBRAL_PICO = 0.60;
const float UMBRAL_RETORNO = 0.15;
const int MUESTRAS_CONFIRMACION = 6;

unsigned long tFinUltimaRep = 0;
const unsigned long T_MIN_ENTRE_REPS = 800;

unsigned long duracionCalibrada = 1500;
const float FACTOR_DURACION_MIN = 0.5;
const float FACTOR_DURACION_MAX = 3.0;
const unsigned long T_MIN = 200;

int contadorRetorno = 0;
int contadorErrorVelocidad = 0;
bool primeraRepHecha = false;
bool esperandoRetornoAInicio = false;  // <-- Candado para evitar ametralladora de errores
bool avisoRetornoEnviado = false;
unsigned long tUltimoDentroReposo = 0;  // ultima vez a menos de 5 grados del reposo
unsigned long tPrimerRetorno = 0;
float pInicioRep = 0;
float vSuave = 0;     // senal suavizada para medir el recorrido
float recorrido = 0;  // suma de |cambios| durante la repeticion

int contadorErrorRango = 0;
int contadorErrorPostura = 0;
const int MUESTRAS_CONFIRM_RANGO = 3;
const int MUESTRAS_CONFIRM_POSTURA = 3;
const int MUESTRAS_CONFIRM_VEL = 3;
const float RATIO_SUAVIDAD_MAX = 1.8;  // recorrido real / ideal: AJUSTAR con tus datos

int errExceso = 0;
int errTemblor = 0;
// === VARIABLES DE INACTIVIDAD ===
unsigned long tiempoUltimoMovimiento = 0;
float anguloAnteriorInactividad = 0.0;
const float UMBRAL_MOVIMIENTO = 3.0;
const unsigned long TIEMPO_MAX_INACTIVIDAD = 4000;

// ============================================================
// CONTADORES DE LA SERIE Y ERRORES
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

// ============================================================
// BLE: REPORTES
// ============================================================
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

void imprimirDetalleErrores() {
  Serial.println("--- Detalle de errores ---");
  Serial.print("Velocidad/sentido: ");
  Serial.println(errVelocidad);
  Serial.print("Postura s0: ");
  Serial.println(errPostura[0]);
  Serial.print("Postura s1: ");
  Serial.println(errPostura[1]);
  Serial.print("Postura s2: ");
  Serial.println(errPostura[2]);
  Serial.print("Rango: ");
  Serial.println(errRango);
  Serial.print("Rapido: ");
  Serial.println(errRapido);
  Serial.print("Lento: ");
  Serial.println(errLento);
  Serial.print("Desconectado: ");
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
  } else {
    ledVerde();
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
  if (codigo < 1 || codigo > CANT_EJERCICIOS) ejercicioActual = &ejercicios[0];
  else ejercicioActual = &ejercicios[codigo - 1];
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

void enviarDetenerAuxiliares(Ejercicio& ej) {
  for (int i = 0; i < 3; i++) {
    enviarComandoUDPAuxiliares("DETENER", ej);
    delay(20);
  }
}

bool iniciarMedicionEnAuxiliares(Ejercicio& ej) {
  bool necesitaAux1 = sensorUsado(ej, 1);
  bool necesitaAux2 = sensorUsado(ej, 2);
  if (!necesitaAux1 && !necesitaAux2) return true;

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
    if (aux1Listo && aux2Listo) return true;
  }
  if (!REQUERIR_AUXILIARES) return true;
  return false;
}

// ============================================================
// CALIBRACION DEL EJERCICIO
// ============================================================
bool calibrarEjercicio(Ejercicio& ej, unsigned long duracionMaximaMs) {
  enviarFeedbackBLE("Quedate quieto en posicion inicial");

  const float TOLERANCIA_ESTABILIDAD = 3.0;
  const int MUESTRAS_ESTABILIDAD_REQUERIDAS = 20;

  float reposoValor = 0;
  float reposoPostura[2] = { 0, 0 };
  int muestrasEstables = 0;
  float ultimoValor = 0;

  unsigned long inicioEstabilidad = millis();
  while (millis() - inicioEstabilidad < 5000) {
    maquinaAntirrebote();
    mediciones();
    recibirPaquetesUDP();
    float vActual = leerAngulo(ej.sensorPrincipal, ej.ejePrincipal);

    if (muestrasEstables == 0) {
      ultimoValor = vActual;
      reposoValor = vActual;
      muestrasEstables = 1;
    } else {
      if (fabs(vActual - ultimoValor) > TOLERANCIA_ESTABILIDAD) {
        muestrasEstables = 0;
      } else {
        reposoValor = (reposoValor * muestrasEstables + vActual) / (muestrasEstables + 1);
        for (int p = 0; p < 2; p++) {
          if (ej.sensorPostura[p] != 99) {
            float vp = leerAngulo(ej.sensorPostura[p], ej.ejePostura[p]);
            if (muestrasEstables == 1) reposoPostura[p] = vp;
            else reposoPostura[p] = (reposoPostura[p] * muestrasEstables + vp) / (muestrasEstables + 1);
          }
        }
        muestrasEstables++;
        ultimoValor = vActual;
        if (muestrasEstables >= MUESTRAS_ESTABILIDAD_REQUERIDAS) break;
      }
    }
    delay(50);
  }

  if (muestrasEstables < MUESTRAS_ESTABILIDAD_REQUERIDAS) {
    enviarFeedbackBLE("ERROR: Movimiento antes de tiempo");
    return false;
  }

  enviarFeedbackBLE("Hace 1 repeticion lenta para calibrar:");

  const float UMBRAL_SALIDA_CALIB = 5.0;
  const float AMPLITUD_MINIMA_CALIB = 12.0;
  // Constantes: antes 6.0 y 0.25
  const float TOLERANCIA_RETORNO_DEG = TOLERANCIA_REPOSO_DEG;  // igual que en las repeticiones
  const float FRACCION_RETORNO_CALIB = UMBRAL_RETORNO;         // igual que en las repeticiones
  const int MUESTRAS_CONFIRM_CALIB = 5;
  const unsigned long TIEMPO_MINIMO_MOVIMIENTO_CALIB = 800;
  unsigned long tRetornoCalib = 0;

  float minV = reposoValor, maxV = reposoValor;
  float amplitudMax = 0;
  float lateralMax[2] = { 0, 0 };
  bool salioDeReposo = false;
  bool retornoCompletado = false;
  int contadorRetornoCalib = 0;
  unsigned long tInicioMovCalib = 0;
  float vFiltrado = reposoValor;
  const float ALPHA_CALIB = 0.6;

  unsigned long inicio = millis();
  while (millis() - inicio < duracionMaximaMs) {
    maquinaAntirrebote();
    mediciones();
    recibirPaquetesUDP();
    float vRaw = leerAngulo(ej.sensorPrincipal, ej.ejePrincipal);
    vFiltrado = ALPHA_CALIB * vRaw + (1 - ALPHA_CALIB) * vFiltrado;
    float distancia = fabs(vFiltrado - reposoValor);

    if (!salioDeReposo && distancia >= UMBRAL_SALIDA_CALIB) {
      salioDeReposo = true;
      tInicioMovCalib = millis();
    }
    if (vFiltrado < minV) minV = vFiltrado;
    if (vFiltrado > maxV) maxV = vFiltrado;
    if (distancia > amplitudMax) amplitudMax = distancia;

    for (int p = 0; p < 2; p++) {
      if (ej.sensorPostura[p] == 99) continue;
      float vp = leerAngulo(ej.sensorPostura[p], ej.ejePostura[p]);
      float d = fabs(vp - reposoPostura[p]);
      if (d > lateralMax[p]) lateralMax[p] = d;
    }

    if (salioDeReposo && amplitudMax >= AMPLITUD_MINIMA_CALIB && (millis() - tInicioMovCalib) >= TIEMPO_MINIMO_MOVIMIENTO_CALIB) {
      bool enPosicionInicial = (distancia <= TOLERANCIA_RETORNO_DEG) || ((distancia / amplitudMax) <= FRACCION_RETORNO_CALIB);
      if (enPosicionInicial) {
        if (contadorRetornoCalib == 0) tRetornoCalib = millis();
        contadorRetornoCalib++;
        if (contadorRetornoCalib >= MUESTRAS_CONFIRM_CALIB) {
          retornoCompletado = true;
          break;
        }
      } else {
        contadorRetornoCalib = 0;
      }
    }
    delay(50);
  }

  if (!salioDeReposo) {
    enviarFeedbackBLE("ERROR: No te moviste");
    return false;
  }
  if (!retornoCompletado) {
    enviarFeedbackBLE("ERROR: No volviste al inicio");
    return false;
  }
  if (amplitudMax < AMPLITUD_MINIMA_CALIB) {
    enviarFeedbackBLE("ERROR: Recorrido muy corto");
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

    // TOPE DURO PARA POSTURA: No permite desviar mas de 12 grados aunque la calibracion haya sido mala
    if (tol > 12.0) tol = 12.0;

    ej.toleranciaPostura[p] = tol;
  }

  duracionCalibrada = tRetornoCalib - tInicioMovCalib;
  if (duracionCalibrada < 500) duracionCalibrada = 500;

  enviarFeedbackBLE("Calibracion exitosa");
  return true;
}

// ============================================================
// VALIDACIONES
// ============================================================
int sensorPosturaFallido(Ejercicio& ej, bool& caido) {
  caido = false;
  for (int i = 0; i < 2; i++) {
    uint8_t s = ej.sensorPostura[i];
    if (s == 99) continue;
    bool sensorCaido = (s == 1 && !aux1_ok) || (s == 2 && !aux2_ok);
    if (sensorCaido) {
      if (!REQUERIR_AUXILIARES) continue;
      caido = true;
      return s;
    }
    float vp = leerAngulo(s, ej.ejePostura[i]);
    if (fabs(vp - ej.centroPostura[i]) > ej.toleranciaPostura[i]) return s;
  }
  return -1;
}

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

  // FIX 1: Forza el tiempo de espera y el candado de retorno para evitar ametralladora de errores
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
  enviarContadorBLE();
}

void evaluarRepeticion(Ejercicio& ej, float velocidadActual) {
  if (!sensorPrincipalOK(ej)) {
    if (faseRep != REPOSO) cerrarRepeticion("MAL (sensor desconectado)", ERR_DESCONEXION, ej.sensorPrincipal);
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
      {
        if (fabs(v - baseLocal) < TOLERANCIA_REPOSO_DEG) {
          baseLocal = ALPHA_BASE_REPOSO * v + (1 - ALPHA_BASE_REPOSO) * baseLocal;
        }
        if (fabs(v - baseLocal) < 5.0) tUltimoDentroReposo = millis();  // misma regla que la calibracion

        // El candado se libera con la MISMA regla con la que se cierra una repeticion
        bool enReposo = (p < UMBRAL_RETORNO) || (fabs(v - baseLocal) <= TOLERANCIA_REPOSO_DEG);
        if (enReposo) {
          esperandoRetornoAInicio = false;
          avisoRetornoEnviado = false;
        } else if (esperandoRetornoAInicio && !avisoRetornoEnviado && (millis() - tFinUltimaRep > 4000)) {
          enviarFeedbackBLE("Vuelve a posicion inicial");  // evita quedarse trabado sin avisar
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
      }

    case EN_MOVIMIENTO:
      {
        unsigned long duracionMax = (unsigned long)(duracionCalibrada * FACTOR_DURACION_MAX);

        if (fabs(v - anguloAnteriorInactividad) > UMBRAL_MOVIMIENTO) {
          tiempoUltimoMovimiento = millis();
          anguloAnteriorInactividad = v;
        }

        // Recorrido total sobre la senal suavizada (mide temblor y tirones)
        float vPrev = vSuave;
        vSuave = 0.5 * v + 0.5 * vSuave;
        recorrido += fabs(vSuave - vPrev);

        // Subfase con histeresis: margen minimo de 3 grados y vuelta a SUBIENDO si hay pico nuevo
        float margenSub = MARGEN_SUBFASE;
        if (margenSub < 3.0 / amplitudObjetivo) margenSub = 3.0 / amplitudObjetivo;
        if (p > picoAlcanzado) {
          picoAlcanzado = p;
          subFaseRep = SUBIENDO;
        } else if (subFaseRep == SUBIENDO && p < picoAlcanzado - margenSub) {
          subFaseRep = BAJANDO;
        }

        // Velocidad en el sentido esperado
        float velEnDireccion = direccionMovimiento * velocidadActual;
        float velEsperada = (subFaseRep == SUBIENDO) ? velEnDireccion : -velEnDireccion;
        bool chequearVel = (subFaseRep == SUBIENDO) ? (p > 0.2 && p < 0.8) : (p > 0.2 && p < picoAlcanzado - 0.15);
        if (chequearVel && velEsperada < -TOLERANCIA_VEL_DPS) contadorErrorVelocidad++;
        else contadorErrorVelocidad = 0;

        // Rango y postura: tienen que repetirse varias muestras seguidas
        const float toleranciaLimites = 10.0;
        bool fueraDeRango = (v < ej.minAngulo - toleranciaLimites) || (v > ej.maxAngulo + toleranciaLimites);
        if (fueraDeRango) contadorErrorRango++;
        else contadorErrorRango = 0;
        if (!okPostura) contadorErrorPostura++;
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
          contadorRetorno++;
          if (contadorRetorno >= MUESTRAS_CONFIRMACION) {
            unsigned long duracion = tPrimerRetorno - tInicioRep;  // igual que en la calibracion
            unsigned long duracionMin = (unsigned long)(duracionCalibrada * FACTOR_DURACION_MIN);
            if (duracionMin < T_MIN) duracionMin = T_MIN;

            float idealGrados = ((picoAlcanzado - pInicioRep) + (picoAlcanzado - p)) * amplitudObjetivo;
            float ratio = (idealGrados > 1.0) ? (recorrido / idealGrados) : 1.0;
            if (MODO_DESARROLLO) {
              Serial.print("Suavidad: ");
              Serial.println(ratio);
            }

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

        if (resultado != "") cerrarRepeticion(resultado, tipo, sensorFallido);
        break;
      }
  }
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
// BLE CLASES
// ============================================================
class MiServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {}
  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
    NimBLEDevice::startAdvertising();
  }
};

class MiCharacteristicCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
    std::string valor = pCharacteristic->getValue();
    if (valor.length() > 0) codigo = atoi(valor.c_str());
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
unsigned long tUltimoAvisoBLE = 0;
const unsigned long INTERVALO_AVISO_BLE = 4000;

void Maq_General() {
  switch (estadoMaq_General) {
    case INICIALIZACION:
      if (!ledEnHold) ledVerde();
      recibirPaquetesUDP();
      if (millis() - tUltimoAvisoBLE > INTERVALO_AVISO_BLE) {
        enviarFeedbackBLE("Toca boton para CALIBRAR");
        tUltimoAvisoBLE = millis();
      }

      if (flagBoton) {
        flagBoton = false;
        ledAmarillo();
        seleccionarEjercicio();

        if (iniciarMedicionEnAuxiliares(*ejercicioActual)) {
          ledVerde();
          if (calibrarEjercicio(*ejercicioActual, 12000)) {
            resetearSerie();
            tUltimoKeepalive = millis();
            estadoMaq_General = MEDICIONES;
            enviarFeedbackBLE("Calibracion OK A entrenar");
          } else {
            mostrarResultadoLed(false);
            enviarDetenerAuxiliares(*ejercicioActual);
          }
        } else {
          mostrarResultadoLed(false);
          enviarFeedbackBLE("ERROR:Sensores sin respuesta");
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
        float v = leerAngulo(ejercicioActual->sensorPrincipal, ejercicioActual->ejePrincipal);
        float velocidadActual = calcularVelocidad(v);
        Serial.print("Angulo:");
        Serial.print(v);
        Serial.print(",Base:");
        Serial.println(baseLocal);
        evaluarRepeticion(*ejercicioActual, velocidadActual);
      }
      actualizarLedMedicion();

      if (flagBoton) {
        flagBoton = false;
        feedbackEnviado = false;
        estadoMaq_General = ANALISIS_SERIE;
      }
      break;

    case ANALISIS_SERIE:
      if (!feedbackEnviado) {
        resultadoValidacion = "BIEN: " + String(contadorBien) + " | MAL: " + String(contadorErrores);
        imprimirDetalleErrores();
        enviarDetenerAuxiliares(*ejercicioActual);
        enviarFeedbackBLE(resultadoValidacion + " -> Toca boton");
        feedbackEnviado = true;
      }
      if (flagBoton) {
        flagBoton = false;
        feedbackEnviado = false;
        estadoMaq_General = C_APLICACION;
      }
      break;

    case C_APLICACION:
      if (!feedbackEnviado) {
        String detalles = obtenerDetalleErrores();
        enviarFeedbackBLE(detalles + " -> Toca reiniciar");
        feedbackEnviado = true;
      }
      if (flagBoton) {
        flagBoton = false;
        estadoMaq_General = INICIALIZACION;
      }
      break;
  }
}

// ============================================================
// DEEP SLEEP
// ============================================================
void irADormir() {
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

// ============================================================
// SETUP / LOOP
// ============================================================
void setup() {
  pinMode(PIN_BOTON, INPUT_PULLUP);
  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  setLED(0, 0);
  delay(50);

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

  ledVerde();
  unsigned long tEspera = millis();
  while (digitalRead(PIN_BOTON) == LOW && millis() - tEspera < 5000) delay(10);
  delay(50);
  ledAmarillo();

  Serial.begin(115200);
  unsigned long t = millis();
  while (!Serial && millis() - t < 1500) delay(10);

  Wire.begin(44, 1);
  sensor.initialize();

  WiFi.mode(WIFI_AP);
  IPAddress local_ip(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  WiFi.softAP(ssid, password, 1, 0, 4);

  udp.begin(PuertoUDP);
  inicializarBLE();
  calibrarOffsetGlobal();

  timerAntirrebote.attach_ms(1, funcionTimerAntirrebote);
  timerMedicion.attach_ms(INTERVALO_MEDICION, funcionTimerMedicion);

  ledVerde();
  estadoBoton = ESPERA;
  flagBoton = false;
}

void loop() {
  maquinaAntirrebote();
  actualizarHoldLed();
  Maq_General();
}