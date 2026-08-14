//no borrar comentarios
#include "I2Cdev.h"
#include "MPU6050.h"
#include "Wire.h"

struct Ejercicio;

MPU6050 sensor;

// Guardan la inclinación actual en cada eje, ya corregida
// con el offset. Se recalculan en cada vuelta del loop().

float inclX, inclY, inclZ;

// ============================================================
// OFFSET GLOBAL DEL SENSOR
// ============================================================
// El offset representa el "cero" del sensor: el valor que
// marca cuando está en reposo. Depende de cómo quedó montado
// físicamente (el sensor, no el ejercicio), por eso se calcula
// UNA sola vez al arrancar y se usa para TODOS los ejercicios.
float offset_X, offset_Y, offset_Z;
// Calcula el offset promediando varias lecturas mientras el
// sensor está quieto

void calibrarOffsetGlobal() {
  Serial.println("Calibrando sensor, no te muevas...");

  float sumaX = 0, sumaY = 0, sumaZ = 0;
  const int MUESTRAS = 100; // cuantas más muestras, más estable el offset
  for (int i = 0; i < MUESTRAS; i++) {
    int16_t ax, ay, az;
    sensor.getAcceleration(&ax, &ay, &az);
    // Convertimos a m/s^2
    float x = (ax / 16384.0) * 9.81;
    float y = (ay / 16384.0) * 9.81;
    float z = (az / 16384.0) * 9.81;

    // Ángulo de inclinación crudo (sin offset todavía) por eje
    sumaX += atan2(x, sqrt(y * y + z * z)) * 180.0 / PI;
    sumaY += atan2(y, sqrt(x * x + z * z)) * 180.0 / PI;
    sumaZ += atan2(z, sqrt(x * x + y * y)) * 180.0 / PI;

    delay(10);
  }

  // El offset es el promedio de esas lecturas "en reposo"
  offset_X = sumaX / MUESTRAS;
  offset_Y = sumaY / MUESTRAS;
  offset_Z = sumaZ / MUESTRAS;

  Serial.println("Sensor calibrado.");
}

// Lee el sensor y actualiza inclX, inclY, inclZ ya con el
// offset aplicado. Se llama en cada vuelta del loop().
void leerInclinacion() {
  int16_t ax, ay, az;
  sensor.getAcceleration(&ax, &ay, &az);

  float x = (ax / 16384.0) * 9.81;
  float y = (ay / 16384.0) * 9.81;
  float z = (az / 16384.0) * 9.81;

  inclX = atan2(x, sqrt(y * y + z * z)) * 180.0 / PI - offset_X;
  inclY = atan2(y, sqrt(x * x + z * z)) * 180.0 / PI - offset_Y;
  inclZ = atan2(z, sqrt(x * x + y * y)) * 180.0 / PI - offset_Z;
}

// Devuelve el ángulo del eje que le pidas (0=X, 1=Y, 2=Z).
// Sirve para que el resto del código no tenga que saber
// "a mano" cuál eje usa cada ejercicio.
float valorEje(uint8_t eje) {
  if (eje == 0) return inclX;
  if (eje == 1) return inclY;
  return inclZ;
}

// ============================================================
// DEFINICIÓN DE UN EJERCICIO
// ============================================================
// Cada ejercicio se resume en: qué eje mirar, y el rango de
// ángulos (mínimo/máximo) que define un movimiento completo.
// minAngulo y maxAngulo NO se escriben a mano: se calculan
// automáticamente con calibrarEjercicio().
// ============================================================
// DEFINICIÓN DE UN EJERCICIO
// ============================================================
struct Ejercicio {
  const char* nombre;
  uint8_t ejePrincipal;   // eje que define el movimiento (ej: Y en curl)
  float minAngulo;        // se llena con la calibración
  float maxAngulo;        // se llena con la calibración

  uint8_t ejeSecundario;  // eje que NO debería moverse mucho (ej: X)
  float centroSecundario; // valor "normal" del eje secundario, se calibra
  float toleranciaSecundario; // cuánto se le permite desviarse
};

Ejercicio ejercicioActual = { "Curl de biceps", 1, 0, 0,  /*principal: Y*/
                                                  0, 0, 0 /*secundario: se define abajo*/ };
// Ejercicio que está activo ahora mismo. Para agregar uno
// nuevo, solo hace falta declarar otro Ejercicio con su
// nombre y su eje (los ángulos se calibran solos):


// ============================================================
// CALIBRACIÓN POR EJERCICIO (repetición de muestra)
// ============================================================
// La persona hace UNA repetición completa y lenta mientras
// esta función está corriendo. Durante ese tiempo se guarda
// el ángulo mínimo y máximo que alcanzó, y eso queda como el
// "rango oficial" de ese ejercicio para esa persona.
void calibrarEjercicio(Ejercicio &ej, unsigned long duracionMs) {
  Serial.println("Calibrando ejercicio...");
  Serial.println("Hace UNA repeticion completa, lenta y CORRECTA ahora.");

  float minV = 999, maxV = -999;

  // Para el eje secundario, en vez de min/max, sumamos para
  // sacar un promedio (el "centro" natural de ese eje cuando
  // el movimiento se hace bien).
  float sumaSecundario = 0;
  int cantidadLecturas = 0;

  // También guardamos cuánto se desvió como máximo del promedio,
  // para saber qué tolerancia darle después.
  float minSecundario = 999, maxSecundario = -999;

  unsigned long inicio = millis();

  while (millis() - inicio < duracionMs) {
    leerInclinacion();

    float v  = valorEje(ej.ejePrincipal);
    float vs = valorEje(ej.ejeSecundario); // lectura del eje de control

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

  // El centro es el promedio de por dónde estuvo el eje secundario
  // durante TODA la repetición bien hecha.
  ej.centroSecundario = sumaSecundario / cantidadLecturas;

  // La tolerancia es cuánto se movió el eje secundario incluso
  // en una repetición correcta (le agregamos un colchón extra).
  float variacionVista = (maxSecundario - minSecundario) / 2.0;
  ej.toleranciaSecundario = variacionVista + 5.0; // +5° de margen extra

  Serial.print("Rango principal: "); Serial.print(minV); Serial.print(" a "); Serial.println(maxV);
  Serial.print("Centro secundario: "); Serial.println(ej.centroSecundario);
  Serial.print("Tolerancia secundario: "); Serial.println(ej.toleranciaSecundario);
}

// ============================================================
// MÁQUINA DE ESTADOS: evalúa si la repetición está bien hecha
// ============================================================
// Fases de una repetición:
//   REPOSO -> arranca cuando se acerca al mínimo del rango
//   MEDIO  -> confirma que pasó por el centro (evita "trampas")
//   FIN    -> confirma que llegó al máximo del rango
enum Fase { REPOSO, MEDIO, FIN };
Fase fase = REPOSO; 
unsigned long tInicio = 0; // marca de tiempo de cuándo arrancó la repetición

// Tiempos válidos para que una repetición cuente como real
// (evita que un golpe/vibración cuente como repetición, y
// evita que se quede "colgado" esperando para siempre).
const unsigned long T_MIN = 200;   // ms mínimos que debe durar una rep
const unsigned long T_MAX = 5000;  // ms máximos antes de descartarla

// Margen de tolerancia alrededor de cada umbral (15% del rango).
// Se usa para no ser demasiado estricto con el punto exacto.
const float MARGEN = 0.15;

void evaluarRepeticion(Ejercicio &ej) {
  float v  = valorEje(ej.ejePrincipal);
  float vs = valorEje(ej.ejeSecundario);

  // ¿Se desvió demasiado del comportamiento normal del eje secundario?
  bool posturaOK = abs(vs - ej.centroSecundario) <= ej.toleranciaSecundario;

  float rango  = ej.maxAngulo - ej.minAngulo;
  float inicio = ej.minAngulo + rango * MARGEN;
  float centro = ej.minAngulo + rango * 0.50;
  float final_ = ej.maxAngulo - rango * MARGEN;

  switch (fase) {

    case REPOSO:
      if (v <= inicio) {
        fase = MEDIO;
        tInicio = millis();
      }
      break;

    case MEDIO:
      // Si se desvía del eje secundario, es "zigzag" -> mal, sin importar Y
      if (!posturaOK) {
        Serial.println("Mal (postura, se desvio del eje secundario)");
        fase = REPOSO;
        delay(250);
        break;
      }
      if (abs(v - centro) <= rango * MARGEN) {
        fase = FIN;
      } else if (millis() - tInicio > T_MAX) {
        Serial.println("Mal");
        fase = REPOSO;
      }
      break;

    case FIN: {
      if (!posturaOK) {
        Serial.println("Mal (postura, se desvio del eje secundario)");
        fase = REPOSO;
        delay(250);
        break;
      }

      unsigned long duracion = millis() - tInicio;
      if (v >= final_) {
        Serial.println(duracion >= T_MIN ? "Bien" : "Mal (muy rapido)");
        fase = REPOSO;
        delay(250);
      } else if (duracion > T_MAX) {
        Serial.println("Mal");
        fase = REPOSO;
      }
      break;
    }
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(6, 7);
  sensor.initialize();

  Serial.println(sensor.testConnection() ? "Sensor listo." : "Error al iniciar sensor.");

  // 1) Offset del sensor: una sola vez, sirve para todos los ejercicios.
  calibrarOffsetGlobal();

  // 2) Calibración del ejercicio actual: una repetición de muestra.
  calibrarEjercicio(ejercicioActual, 4000); // 4 segundos para la rep de muestra
}

void loop() { 
  leerInclinacion();
  evaluarRepeticion(ejercicioActual);
  delay(100);
}