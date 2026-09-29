// ============================================================
// TECHECK - MODULO PRINCIPAL (ESP32-C3) - SOLO SENSOR LOCAL
// Sin auxiliares por WiFi. Ejercicio: Curl de biceps.
//
// LOGICA DE LED:
//   AZUL     -> calibrando (offset inicial, y calibracion del ejercicio)
//   AMARILLO -> esperando que toques el boton / entre repeticiones
//   VERDE    -> repeticion correcta
//   ROJO     -> repeticion incorrecta
// ============================================================

#include <MPU6050.h>
#include "Wire.h"
#include <Ticker.h>
#include "I2Cdev.h"
#include <WiFi.h>
#include <HTTPClient.h>

MPU6050 sensor;

Ticker timerMedicion; // ya no usamos Ticker para el antirrebote, no hace falta

bool flagMedicion = false;
const int INTERVALO_MEDICION = 100;

// ------------------------------------------------------------
// PINES
// ------------------------------------------------------------
#define PIN_BOTON 1
#define PIN_LED_R 20
#define PIN_LED_G 3
#define PIN_LED_B 10

// ------------------------------------------------------------
// COLORES DE LED
// ------------------------------------------------------------
void setLED(bool r, bool g, bool b) {
  digitalWrite(PIN_LED_R, r ? HIGH : LOW);
  digitalWrite(PIN_LED_G, g ? HIGH : LOW);
  digitalWrite(PIN_LED_B, b ? HIGH : LOW);
}
void ledAzul()     { setLED(0, 0, 1); }
void ledAmarillo() { setLED(1, 1, 0); }
void ledVerde()    { setLED(0, 1, 0); }
void ledRojo()     { setLED(1, 0, 0); }

// ------------------------------------------------------------
// ANTIRREBOTE DEL BOTON (basado en millis(), sin ISR de 1ms)
// ------------------------------------------------------------
typedef enum { ESPERA_BOTON, CONFIRMACION, LIBERACION } estadoAntirrebote_t;
estadoAntirrebote_t estadoBoton = ESPERA_BOTON;
unsigned long tEstadoBoton = 0;
bool flagBoton = false;
#define T_REBOTE_MS 15

void maquinaAntirrebote() {
  bool lectura = digitalRead(PIN_BOTON);
  switch (estadoBoton) {
    case ESPERA_BOTON:
      if (lectura == LOW) {
        tEstadoBoton = millis();
        estadoBoton = CONFIRMACION;
      }
      break;
    case CONFIRMACION:
      if (millis() - tEstadoBoton >= T_REBOTE_MS) {
        estadoBoton = (lectura == LOW) ? LIBERACION : ESPERA_BOTON;
      }
      break;
    case LIBERACION:
      if (lectura == HIGH) {
        flagBoton = true;
        estadoBoton = ESPERA_BOTON;
      }
      break;
  }
}
void funcionTimerMedicion() { flagMedicion = true; }

// ------------------------------------------------------------
// LECTURA DEL SENSOR LOCAL
// ------------------------------------------------------------
int16_t ax_raw, ay_raw, az_raw;
int16_t gx_raw, gy_raw, gz_raw;
float inclX, inclY, inclZ;
float velInclX = 0, velInclY = 0, velInclZ = 0;   // velocidad angular (grados/seg) de cada eje
float inclX_ant = 0, inclY_ant = 0, inclZ_ant = 0; // valores anteriores, para derivar la velocidad
float offset_X, offset_Y, offset_Z;

const float ALPHA_FILTRO = 0.3;
const float ALPHA_VEL_FILTRO = 0.4; // suavizado de la velocidad medida (la derivada es ruidosa)
bool filtroInicializado = false;
unsigned long tUltimaLectura = 0;

// Sensibilidades por defecto del MPU6050 (acelerometro +-2g, giroscopio +-250 dps)
const float SENSIBILIDAD_ACC  = 16384.0;
const float SENSIBILIDAD_GYRO = 131.0;

// Filtro complementario: normalmente confiamos poco en el giroscopio (deriva a largo plazo)
// y mucho en el acelerometro. Pero cuando hay un movimiento brusco, el acelerometro deja de
// medir solo gravedad (aparece aceleracion lineal encima) y el angulo que calcula se vuelve
// una mentira momentanea. Por eso, cuando la magnitud del vector de aceleracion se aleja de
// 9.81 m/s^2, invertimos la confianza: pasamos a confiar mucho mas en el giroscopio.
const float PESO_GYRO_BASE    = 0.20;  // peso del giroscopio en reposo/movimiento lento
const float PESO_GYRO_MAX     = 0.97;  // peso del giroscopio durante un movimiento brusco
const float TOLERANCIA_ACEL   = 1.2;   // m/s^2 de margen alrededor de 9.81 antes de desconfiar
const float GANANCIA_DESVIO   = 0.05;  // que tan rapido sube el peso del giroscopio con el desvio

void leerSensor() {
  sensor.getMotion6(&ax_raw, &ay_raw, &az_raw, &gx_raw, &gy_raw, &gz_raw);

  float x = (ax_raw / SENSIBILIDAD_ACC) * 9.81;
  float y = (ay_raw / SENSIBILIDAD_ACC) * 9.81;
  float z = (az_raw / SENSIBILIDAD_ACC) * 9.81;

  // Angulo "crudo" segun el acelerometro (solo es fiel a la realidad si no hay aceleracion lineal)
  float accX = atan2(x, sqrt(y * y + z * z)) * 180.0 / PI - offset_X;
  float accY = atan2(y, sqrt(x * x + z * z)) * 180.0 / PI - offset_Y;
  float accZ = atan2(z, sqrt(x * x + y * y)) * 180.0 / PI - offset_Z;

  // Velocidad angular en grados/segundo (misma letra de eje que el angulo que ayuda a corregir;
  // si el sensor esta montado distinto y la fusion se nota "invertida", probar cruzar gx/gy/gz)
  float gyroX = gx_raw / SENSIBILIDAD_GYRO;
  float gyroY = gy_raw / SENSIBILIDAD_GYRO;
  float gyroZ = gz_raw / SENSIBILIDAD_GYRO;

  unsigned long ahora = millis();
  float dt = (tUltimaLectura == 0) ? 0.1 : (ahora - tUltimaLectura) / 1000.0;
  tUltimaLectura = ahora;
  if (dt <= 0 || dt > 0.5) dt = 0.1; // primer ciclo o salto raro de tiempo

  // Cuanto se aleja la magnitud de la aceleracion medida de 1g (9.81 m/s^2): si es grande,
  // hay aceleracion lineal (se esta moviendo rapido) y el acelerometro ya no es confiable
  float magnitudAcel = sqrt(x * x + y * y + z * z);
  float desvio = fabs(magnitudAcel - 9.81);
  float pesoGyro = PESO_GYRO_BASE;
  if (desvio > TOLERANCIA_ACEL) {
    pesoGyro = PESO_GYRO_BASE + (desvio - TOLERANCIA_ACEL) * GANANCIA_DESVIO;
    if (pesoGyro > PESO_GYRO_MAX) pesoGyro = PESO_GYRO_MAX;
  }

  if (!filtroInicializado) {
    inclX = accX;
    inclY = accY;
    inclZ = accZ;
    filtroInicializado = true;
    velInclX = velInclY = velInclZ = 0;
  } else {
    // Giroscopio para el corto plazo (inmune a la aceleracion lineal), acelerometro para
    // corregir la deriva del giroscopio en el largo plazo
    float fusionX = pesoGyro * (inclX + gyroX * dt) + (1 - pesoGyro) * accX;
    float fusionY = pesoGyro * (inclY + gyroY * dt) + (1 - pesoGyro) * accY;
    float fusionZ = pesoGyro * (inclZ + gyroZ * dt) + (1 - pesoGyro) * accZ;

    inclX = ALPHA_FILTRO * fusionX + (1 - ALPHA_FILTRO) * inclX;
    inclY = ALPHA_FILTRO * fusionY + (1 - ALPHA_FILTRO) * inclY;
    inclZ = ALPHA_FILTRO * fusionZ + (1 - ALPHA_FILTRO) * inclZ;

    // Velocidad angular real (grados/seg), a partir de cuanto cambio el angulo ya filtrado.
    // Se usa para saber, en cada punto del recorrido, si el sensor esta yendo "hacia adelante"
    // o "hacia atras" (no es lo mismo estar en tal angulo subiendo que bajando).
    float nuevaVelX = (inclX - inclX_ant) / dt;
    float nuevaVelY = (inclY - inclY_ant) / dt;
    float nuevaVelZ = (inclZ - inclZ_ant) / dt;
    velInclX = ALPHA_VEL_FILTRO * nuevaVelX + (1 - ALPHA_VEL_FILTRO) * velInclX;
    velInclY = ALPHA_VEL_FILTRO * nuevaVelY + (1 - ALPHA_VEL_FILTRO) * velInclY;
    velInclZ = ALPHA_VEL_FILTRO * nuevaVelZ + (1 - ALPHA_VEL_FILTRO) * velInclZ;
  }
  inclX_ant = inclX;
  inclY_ant = inclY;
  inclZ_ant = inclZ;
}

// ------------------------------------------------------------
// MODULO AUXILIAR (opcional, hasta 2 unidades por WiFi)
// ------------------------------------------------------------
// Cada auxiliar es un ESP8266+MPU6050 corriendo el firmware "auxiliar.ino": se conecta
// como estacion WiFi a este modulo (que hace de Access Point) con IP fija, y expone
// /iniciar, /detener y /datos (devuelve "x,y,z,inclY"). Este modulo:
//   - Al arrancar (y antes de cada calibracion) espera un momento a ver si hay
//     auxiliares conectados.
//   - Si NO detecta ninguno, funciona exactamente igual que antes (solo sensor local).
//   - Si detecta uno o dos, ademas hace la MISMA revision de tolerancia (linea base
//     adaptativa en reposo + tolerancia durante el movimiento) sobre cada auxiliar.
//
// NOTA: el codigo de auxiliar.ino provisto trae fija la IP 192.168.4.2 (primer
// auxiliar). Para usar un segundo auxiliar hay que flashear una segunda unidad con
// el mismo firmware pero cambiando esa IP fija a 192.168.4.3 (no pueden compartir IP).
const char* AP_SSID     = "ESP32_C3_Server";
const char* AP_PASSWORD = "GRUPO3XX";

const int NUM_AUX_MAX = 2;
IPAddress AUX_IP[NUM_AUX_MAX] = { IPAddress(192, 168, 4, 2), IPAddress(192, 168, 4, 3) };

bool  auxConectado[NUM_AUX_MAX]      = { false, false };
float auxBaseLocal[NUM_AUX_MAX]      = { 0, 0 };  // "cero" adaptativo de cada auxiliar, en reposo
float auxInclY[NUM_AUX_MAX]          = { 0, 0 };  // ultima lectura valida de cada auxiliar
int   auxFallosSeguidos[NUM_AUX_MAX] = { 0, 0 };
int   contadorErrorAux[NUM_AUX_MAX]  = { 0, 0 };

const uint16_t TIMEOUT_AUX_MS     = 150; // no bloquear mucho si un auxiliar no responde
const int INTENTOS_DETECCION_AUX  = 5;   // reintentos al buscar auxiliares
const int FALLOS_MAX_AUX          = 5;   // fallos de red seguidos antes de darlo por desconectado
const float TOLERANCIA_AUX_DEG    = 8.0; // grados de tolerancia para el chequeo de cada auxiliar

// Pide /datos a un auxiliar puntual. Devuelve true y llena outInclY si respondio bien.
bool consultarAuxiliar(int i, float &outInclY) {
  WiFiClient client;
  HTTPClient http;
  http.setConnectTimeout(TIMEOUT_AUX_MS);
  http.begin(client, AUX_IP[i].toString(), 80, "/datos");
  http.setTimeout(TIMEOUT_AUX_MS);
  int codigo = http.GET();
  bool ok = false;
  if (codigo == 200) {
    String resp = http.getString();
    int idx1 = resp.indexOf(',');
    int idx2 = (idx1 >= 0) ? resp.indexOf(',', idx1 + 1) : -1;
    int idx3 = (idx2 >= 0) ? resp.indexOf(',', idx2 + 1) : -1;
    if (idx3 >= 0) {
      outInclY = resp.substring(idx3 + 1).toFloat();
      ok = true;
    }
  }
  http.end();
  return ok;
}

// Avisa a un auxiliar conectado que arranca/termina una serie (endpoint "/iniciar" o "/detener").
void avisarAuxiliar(int i, const char* endpoint) {
  if (!auxConectado[i]) return;
  WiFiClient client;
  HTTPClient http;
  http.setConnectTimeout(TIMEOUT_AUX_MS);
  http.begin(client, AUX_IP[i].toString(), 80, endpoint);
  http.setTimeout(TIMEOUT_AUX_MS);
  http.GET();
  http.end();
}

// Busca auxiliares conectados. Se llama al arrancar y antes de cada calibracion, para
// que sea indistinto conectar uno, dos, o ninguno en cualquier momento entre series.
void detectarAuxiliares() {
  Serial.println("Buscando auxiliares conectados...");
  for (int i = 0; i < NUM_AUX_MAX; i++) {
    auxConectado[i] = false;
    for (int intento = 0; intento < INTENTOS_DETECCION_AUX && !auxConectado[i]; intento++) {
      float v;
      if (consultarAuxiliar(i, v)) {
        auxConectado[i] = true;
        auxFallosSeguidos[i] = 0;
      } else {
        delay(200);
      }
    }
    Serial.print("Auxiliar "); Serial.print(i + 1);
    Serial.println(auxConectado[i] ? ": detectado." : ": no detectado, se sigue sin el.");
  }
}

// Actualiza auxInclY[] de cada auxiliar conectado. Si un auxiliar deja de responder
// varias veces seguidas, se lo da por desconectado y de ahi en mas se ignora (el
// sistema sigue funcionando normal, solo con los que sigan respondiendo).
void leerAuxiliares() {
  for (int i = 0; i < NUM_AUX_MAX; i++) {
    if (!auxConectado[i]) continue;
    float v;
    if (consultarAuxiliar(i, v)) {
      auxInclY[i] = v;
      auxFallosSeguidos[i] = 0;
    } else {
      auxFallosSeguidos[i]++;
      if (auxFallosSeguidos[i] >= FALLOS_MAX_AUX) {
        auxConectado[i] = false;
        contadorErrorAux[i] = 0;
        Serial.print("Auxiliar "); Serial.print(i + 1);
        Serial.println(": se perdio la conexion, se sigue sin el.");
      }
    }
  }
}

void calibrarOffsetGlobal() {
  Serial.println("Calibrando sensor, no muevas el dispositivo...");
  ledAzul();

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
  Serial.println("Sensor calibrado.");
}

// ------------------------------------------------------------
// CALIBRACION DEL EJERCICIO - umbral de retorno ADAPTATIVO
// (relativo a la amplitud real detectada, no grados fijos)
// ------------------------------------------------------------
float minAngulo, maxAngulo;
float reposoValor, picoValor;

#define AMPLITUD_MINIMA_CALIB  8.0   // grados: por debajo de esto se considera que todavia no se movio (filtra ruido)
#define FRACCION_RETORNO_CALIB 0.25  // debe bajar a este % de la amplitud maxima vista para contar como "volvio"
#define MUESTRAS_CONFIRM_CALIB 3     // lecturas seguidas para confirmar el retorno

// ------------------------------------------------------------
// POSICION INICIAL LOCAL ("0,0,0" de cada movimiento)
// ------------------------------------------------------------
// En vez de exigir que el brazo vuelva exactamente al angulo de la calibracion
// (reposoValor), cada movimiento tiene su propio cero: baseLocal. Se actualiza
// lentamente mientras estamos quietos en reposo (para tolerar pequeñas derivas
// o que la persona no arranque siempre en el angulo exacto) y queda "fijado"
// como referencia apenas se detecta el inicio de un movimiento.
float baseLocal;
float baseLocalX, baseLocalZ;   // "cero" lateral (ejes X y Z) de cada movimiento
float amplitudObjetivo = 1.0;   // amplitud esperada, en grados, segun la calibracion
int direccionMovimiento = 1;    // +1 si el pico esta por encima del reposo, -1 si esta por debajo

const float ALPHA_BASE_REPOSO     = 0.02; // que tan rapido se adapta baseLocal en reposo
const float TOLERANCIA_REPOSO_DEG = 4.0;  // tolerancia (bajada) para considerar "volvio al inicio"
const float TOLERANCIA_VEL_DPS    = 15.0; // margen de ruido (grados/seg) antes de marcar la velocidad como "en sentido incorrecto"

// Tolerancia lateral (X/Z): NO es un numero fijo. Como inclX/inclY/inclZ se calculan con
// formulas no ortogonales entre si, incluso una curl perfecta (un solo eje) hace que X y Z
// se muevan un poco solo por geometria -- no porque el brazo se fue para el costado. Por
// eso la tolerancia se calibra sola: se mide cuanto se movieron X/Z durante la repeticion
// "correcta" de la calibracion, se le deja un margen, y ESE es el umbral que se usa despues.
float toleranciaLateralUsada = 8.0;         // se sobreescribe en calibrarEjercicio()
const float TOLERANCIA_LATERAL_MINIMA = 3.0; // piso: nunca queda mas exigente que esto
const float FACTOR_MARGEN_LATERAL     = 1.5; // margen sobre el desvio lateral natural detectado

// ------------------------------------------------------------
// DURACION DE REFERENCIA (segun la repeticion hecha en la calibracion)
// ------------------------------------------------------------
// En vez de un tiempo minimo/maximo fijo para todo el mundo, la repeticion "correcta"
// que la persona hizo durante la calibracion define cuanto deberia tardar una rep. Las
// repeticiones reales se comparan contra ESE tiempo, con tolerancia (factor min/max).
unsigned long duracionCalibrada = 1500; // ms; se sobreescribe en calibrarEjercicio()
const float FACTOR_DURACION_MIN = 0.4;  // no puede durar menos del 40% de lo calibrado
const float FACTOR_DURACION_MAX = 2.2;  // no puede durar mas del 220% de lo calibrado

void calibrarEjercicio(unsigned long duracionMaximaMs) {
  Serial.println("Calibrando ejercicio: Curl de biceps");
  Serial.println("Mantene el brazo quieto, en la posicion de reposo...");
  ledAzul();

  // Arrancamos el filtro de fusion (acelerometro+giroscopio) de cero, para no
  // arrastrar ninguna deriva acumulada de antes de apretar el boton.
  filtroInicializado = false;
  tUltimaLectura = 0;

  // Se vuelve a buscar auxiliares antes de cada serie: puede que se hayan conectado
  // o desconectado desde la ultima vez.
  detectarAuxiliares();
  for (int a = 0; a < NUM_AUX_MAX; a++) avisarAuxiliar(a, "/iniciar");

  const int MUESTRAS_REPOSO = 10;
  float sumaReposo = 0, sumaReposoX = 0, sumaReposoZ = 0;
  float sumaReposoAux[NUM_AUX_MAX] = { 0, 0 };
  int muestrasReposoAux[NUM_AUX_MAX] = { 0, 0 };
  for (int i = 0; i < MUESTRAS_REPOSO; i++) {
    leerSensor();
    leerAuxiliares();
    sumaReposo += inclY;
    sumaReposoX += inclX;
    sumaReposoZ += inclZ;
    for (int a = 0; a < NUM_AUX_MAX; a++) {
      if (auxConectado[a]) {
        sumaReposoAux[a] += auxInclY[a];
        muestrasReposoAux[a]++;
      }
    }
    delay(50);
  }
  reposoValor = sumaReposo / MUESTRAS_REPOSO;
  float reposoX = sumaReposoX / MUESTRAS_REPOSO;
  float reposoZ = sumaReposoZ / MUESTRAS_REPOSO;
  for (int a = 0; a < NUM_AUX_MAX; a++) {
    if (auxConectado[a] && muestrasReposoAux[a] > 0) {
      auxBaseLocal[a] = sumaReposoAux[a] / muestrasReposoAux[a];
    }
  }

  Serial.println("Ahora hace UNA repeticion completa, lenta y CORRECTA...");
  Serial.println("(la calibracion termina sola cuando volves a la posicion inicial)");

  float minV = reposoValor, maxV = reposoValor;
  float amplitudMax = 0;
  float lateralXMax = 0, lateralZMax = 0; // cuanto se movio X/Z durante la repeticion "correcta"
  bool salioDeReposo = false;
  int contadorRetornoCalib = 0;
  unsigned long tInicioMovCalib = 0;

  unsigned long inicio = millis();
  unsigned long ultimoDebug = 0;

  while (millis() - inicio < duracionMaximaMs) {
    leerSensor();
    if (inclY < minV) minV = inclY;
    if (inclY > maxV) maxV = inclY;

    float distancia = abs(inclY - reposoValor);
    if (distancia > amplitudMax) amplitudMax = distancia;

    float distanciaX = fabs(inclX - reposoX);
    float distanciaZ = fabs(inclZ - reposoZ);
    if (distanciaX > lateralXMax) lateralXMax = distanciaX;
    if (distanciaZ > lateralZMax) lateralZMax = distanciaZ;

    if (!salioDeReposo && amplitudMax > AMPLITUD_MINIMA_CALIB) {
      salioDeReposo = true;
      tInicioMovCalib = millis();
    }

    if (salioDeReposo) {
      float progresoCalib = distancia / amplitudMax;
      if (progresoCalib <= FRACCION_RETORNO_CALIB) {
        contadorRetornoCalib++;
        if (contadorRetornoCalib >= MUESTRAS_CONFIRM_CALIB) break;
      } else {
        contadorRetornoCalib = 0;
      }
    }

    // Debug en vivo cada 200ms, para ver si el sensor esta reaccionando de verdad
    if (millis() - ultimoDebug > 200) {
      ultimoDebug = millis();
      Serial.print("inclY="); Serial.print(inclY, 1);
      Serial.print("  dist="); Serial.print(distancia, 1);
      Serial.print("  amplitudMax="); Serial.println(amplitudMax, 1);
    }

    delay(50);
  }

  minAngulo = minV;
  maxAngulo = maxV;
  picoValor = (abs(maxAngulo - reposoValor) > abs(minAngulo - reposoValor)) ? maxAngulo : minAngulo;

  // La posicion inicial local (el "0,0,0" del movimiento) arranca en el reposo detectado
  // aca, pero despues se va a ir ajustando sola cada vez que el brazo este quieto.
  baseLocal = reposoValor;
  baseLocalX = reposoX;
  baseLocalZ = reposoZ;
  amplitudObjetivo = abs(picoValor - reposoValor);
  if (amplitudObjetivo < 1.0) amplitudObjetivo = 1.0; // evita dividir por (casi) cero
  direccionMovimiento = (picoValor >= reposoValor) ? 1 : -1;

  // Tolerancia lateral autocalibrada: el mayor desvio natural visto en X o Z durante la
  // repeticion "correcta", con margen. Asi cada persona/montaje tiene su propia tolerancia,
  // en vez de un numero fijo adivinado que puede ser demasiado exigente (falsos "MAL") o
  // demasiado laxo (no detecta nada).
  float lateralMax = (lateralXMax > lateralZMax) ? lateralXMax : lateralZMax;
  toleranciaLateralUsada = lateralMax * FACTOR_MARGEN_LATERAL;
  if (toleranciaLateralUsada < TOLERANCIA_LATERAL_MINIMA) toleranciaLateralUsada = TOLERANCIA_LATERAL_MINIMA;

  // Duracion de referencia: cuanto tardo la repeticion "correcta" de la calibracion, desde
  // que salio de reposo hasta que termino el bucle (volvio o se corto el tiempo maximo).
  if (tInicioMovCalib > 0) {
    duracionCalibrada = millis() - tInicioMovCalib;
  }
  if (duracionCalibrada < 300) duracionCalibrada = 1500; // no se pudo medir bien: usar un valor por defecto

  Serial.print("Reposo: "); Serial.print(reposoValor);
  Serial.print(" | Pico: "); Serial.print(picoValor);
  Serial.print(" | Amplitud detectada: "); Serial.print(amplitudMax);
  Serial.print(" | Desvio lateral (X/Z) natural: "); Serial.print(lateralMax, 1);
  Serial.print(" | Tolerancia lateral usada: "); Serial.print(toleranciaLateralUsada, 1);
  Serial.print(" | Duracion calibrada: "); Serial.print(duracionCalibrada);
  Serial.println(" ms");

  if (amplitudMax < AMPLITUD_MINIMA_CALIB) {
    Serial.println("ADVERTENCIA: casi no se detecto movimiento.");
    Serial.println("Revisa la orientacion del sensor, o baja AMPLITUD_MINIMA_CALIB.");
  }
  Serial.println("Calibracion finalizada.");
}

// ------------------------------------------------------------
// MAQUINA DE ESTADOS DE LA REPETICION
// ------------------------------------------------------------
enum FaseRep { REPOSO, EN_MOVIMIENTO };
FaseRep faseRep = REPOSO;
unsigned long tInicioRep = 0;
float picoAlcanzado = 0;
int contadorRetorno = 0;

// Subfase dentro de EN_MOVIMIENTO: hacia el pico (SUBIENDO) o de vuelta al reposo (BAJANDO).
// No es lo mismo estar en tal angulo subiendo que estar en el mismo angulo bajando: la
// velocidad esperada en cada punto del recorrido depende de en cual de las dos se esta.
enum SubFaseRep { SUBIENDO, BAJANDO };
SubFaseRep subFaseRep = SUBIENDO;
const float MARGEN_SUBFASE = 0.05; // cuanto tiene que bajar el progreso desde el pico para contar como "ya esta bajando"

int contadorErrorLateral = 0;
int contadorErrorVelocidad = 0;

const unsigned long T_MIN = 200;
const unsigned long T_MAX = 5000;
const float UMBRAL_SALIDA  = 0.15;
const float UMBRAL_PICO    = 0.60;
const float UMBRAL_RETORNO = 0.25;
const int MUESTRAS_CONFIRMACION = 3;

int contadorErrores = 0;

float progreso(float v) {
  return direccionMovimiento * (v - baseLocal) / amplitudObjetivo;
}

void evaluarRepeticion() {
  float p = progreso(inclY);
  String resultado = "";

  switch (faseRep) {
    case REPOSO:
      // Mientras el brazo esta quieto cerca del cero actual, dejamos que las tres
      // referencias (adelante/atras y los dos costados) se ajusten solas.
      if (fabs(inclY - baseLocal) < TOLERANCIA_REPOSO_DEG) {
        baseLocal = ALPHA_BASE_REPOSO * inclY + (1 - ALPHA_BASE_REPOSO) * baseLocal;
      }
      if (fabs(inclX - baseLocalX) < toleranciaLateralUsada) {
        baseLocalX = ALPHA_BASE_REPOSO * inclX + (1 - ALPHA_BASE_REPOSO) * baseLocalX;
      }
      if (fabs(inclZ - baseLocalZ) < toleranciaLateralUsada) {
        baseLocalZ = ALPHA_BASE_REPOSO * inclZ + (1 - ALPHA_BASE_REPOSO) * baseLocalZ;
      }
      // Igual que con el sensor local: mientras estamos en reposo, cada auxiliar
      // conectado tambien ajusta su propio "cero" (misma logica, con su propia tolerancia).
      for (int a = 0; a < NUM_AUX_MAX; a++) {
        if (!auxConectado[a]) continue;
        if (fabs(auxInclY[a] - auxBaseLocal[a]) < TOLERANCIA_AUX_DEG) {
          auxBaseLocal[a] = ALPHA_BASE_REPOSO * auxInclY[a] + (1 - ALPHA_BASE_REPOSO) * auxBaseLocal[a];
        }
      }

      if (p >= UMBRAL_SALIDA) {
        // Se detecta el inicio del movimiento: las referencias quedan fijas como el
        // "0,0,0" de esta repeticion (ya no se siguen adaptando durante el movimiento).
        faseRep = EN_MOVIMIENTO;
        subFaseRep = SUBIENDO;
        tInicioRep = millis();
        picoAlcanzado = p;
        contadorRetorno = 0;
        contadorErrorLateral = 0;
        contadorErrorVelocidad = 0;
        for (int a = 0; a < NUM_AUX_MAX; a++) contadorErrorAux[a] = 0;
      }
      break;

    case EN_MOVIMIENTO: {
      if (p > picoAlcanzado) picoAlcanzado = p;

      // Una vez que el progreso empieza a bajar claramente desde el pico, arranco la
      // "vuelta" (BAJANDO); antes de eso, se espera que siga subiendo hacia el pico.
      if (subFaseRep == SUBIENDO && p < picoAlcanzado - MARGEN_SUBFASE) {
        subFaseRep = BAJANDO;
      }

      // --- Velocidad y direccion: en cada punto del recorrido, chequeamos (con tolerancia)
      // que la velocidad vaya para el lado que corresponde a la subfase actual. Si el sensor
      // se frena o invierte el sentido de forma sostenida donde no corresponde, es un error.
      float velEnDireccion = direccionMovimiento * velInclY;
      float velEsperada = (subFaseRep == SUBIENDO) ? velEnDireccion : -velEnDireccion;
      if (velEsperada < -TOLERANCIA_VEL_DPS) {
        contadorErrorVelocidad++;
      } else {
        contadorErrorVelocidad = 0;
      }

      // --- Movimiento lateral: el ejercicio es de un solo eje. Si el brazo se corre para
      // el costado (X o Z) mas alla de la tolerancia, tambien es una repeticion incorrecta.
      bool fueraDeEje = fabs(inclX - baseLocalX) > toleranciaLateralUsada ||
                         fabs(inclZ - baseLocalZ) > toleranciaLateralUsada;
      if (fueraDeEje) {
        contadorErrorLateral++;
      } else {
        contadorErrorLateral = 0;
      }

      // --- Auxiliares: misma revision de tolerancia que el sensor local, aplicada a cada
      // auxiliar conectado. Si no hay ninguno conectado, este bloque no hace nada (el
      // sistema sigue funcionando exactamente igual que sin auxiliares).
      bool auxFueraDeRango = false;
      int auxCulpable = -1;
      for (int a = 0; a < NUM_AUX_MAX; a++) {
        if (!auxConectado[a]) { contadorErrorAux[a] = 0; continue; }
        if (fabs(auxInclY[a] - auxBaseLocal[a]) > TOLERANCIA_AUX_DEG) {
          contadorErrorAux[a]++;
        } else {
          contadorErrorAux[a] = 0;
        }
        if (contadorErrorAux[a] >= MUESTRAS_CONFIRMACION) {
          auxFueraDeRango = true;
          auxCulpable = a;
        }
      }

      if (contadorErrorVelocidad >= MUESTRAS_CONFIRMACION) {
        resultado = "MAL (velocidad o sentido del movimiento incorrecto)";
        faseRep = REPOSO;
        contadorRetorno = 0;
      } else if (contadorErrorLateral >= MUESTRAS_CONFIRMACION) {
        resultado = "MAL (te moviste para el costado)";
        faseRep = REPOSO;
        contadorRetorno = 0;
      } else if (auxFueraDeRango) {
        resultado = "MAL (auxiliar " + String(auxCulpable + 1) + ": movimiento fuera de tolerancia)";
        faseRep = REPOSO;
        contadorRetorno = 0;
      } else if (p <= UMBRAL_RETORNO || fabs(inclY - baseLocal) <= TOLERANCIA_REPOSO_DEG) {
        // Se considera que "volvio al inicio" si el progreso cayo por debajo del umbral
        // relativo, O si en grados absolutos ya esta dentro de la tolerancia de reposo.
        contadorRetorno++;
        if (contadorRetorno >= MUESTRAS_CONFIRMACION) {
          unsigned long duracion = millis() - tInicioRep;

          // El tiempo aceptable se calcula a partir de lo que tardo la repeticion de
          // la calibracion, con tolerancia (no un valor fijo igual para todos).
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
    Serial.println("==============================");
    Serial.print("RESULTADO: ");
    Serial.print(resultado);
    Serial.print("  (pico alcanzado: ");
    Serial.print(picoAlcanzado * 100);
    Serial.println("%)");
    Serial.println("==============================");

    if (resultado == "BIEN") {
      ledVerde();
    } else {
      ledRojo();
      contadorErrores++;
    }
  }
}

// ------------------------------------------------------------
// MAQUINA GENERAL
// ------------------------------------------------------------
typedef enum { ESPERA, CALIBRANDO, MIDIENDO } estadoGeneral_t;
estadoGeneral_t estadoGeneral = ESPERA;

void maquinaGeneral() {
  switch (estadoGeneral) {
    case ESPERA:
      ledAmarillo();
      if (flagBoton) {
        flagBoton = false;
        estadoGeneral = CALIBRANDO;
      }
      break;

    case CALIBRANDO:
      calibrarEjercicio(8000);
      contadorErrores = 0;
      faseRep = REPOSO;
      picoAlcanzado = 0;
      ledAmarillo();
      estadoGeneral = MIDIENDO;
      break;

    case MIDIENDO:
      if (flagMedicion) {
        flagMedicion = false;
        leerSensor();
        leerAuxiliares();
        evaluarRepeticion();
      }
      if (flagBoton) {
        flagBoton = false;
        for (int a = 0; a < NUM_AUX_MAX; a++) avisarAuxiliar(a, "/detener");
        Serial.print("Serie finalizada. Errores: ");
        Serial.println(contadorErrores);
        estadoGeneral = ESPERA;
      }
      break;
  }
}

// ------------------------------------------------------------
// SETUP / LOOP
// ------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  Wire.begin(6, 7);
  sensor.initialize();

  pinMode(PIN_BOTON, INPUT_PULLUP);
  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_B, OUTPUT);

  calibrarOffsetGlobal();
  ledAmarillo();

  // Access Point para que se conecten hasta 2 auxiliares (mismas credenciales que
  // esperan en auxiliar.ino). Si ninguno se conecta, el resto del codigo sigue
  // funcionando igual que sin este modulo.
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  delay(300); // le da tiempo al AP a levantar antes de buscar auxiliares
  detectarAuxiliares();

  timerMedicion.attach_ms(INTERVALO_MEDICION, funcionTimerMedicion);
}

void loop() {
  maquinaAntirrebote();
  maquinaGeneral();
}
