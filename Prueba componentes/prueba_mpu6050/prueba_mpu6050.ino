#include "I2Cdev.h"
#include "MPU6050.h"
#include "Wire.h"

MPU6050 sensor;

int16_t ax, ay, az;
int16_t gx, gy, gz;
float ax_ms2, ay_ms2, az_ms2;
float inclX, inclY, inclZ;

// Tu cero personalizado
float offset_X = -81.42;
float offset_Y = -2.11;
float offset_Z = -8.32;

// Tolerancias
float MIN_X_PERMITIDO = 40.0; 
float MAX_X_PERMITIDO = 100.0; 

// ==================================================
// PROTECCIÓN CONTRA VIBRACIONES RÁPIDAS
// ==================================================
unsigned long TIEMPO_MINIMO = 200; // Mínimo 200ms para que sea un gesto real
unsigned long TIEMPO_MAXIMO = 2000; // Máximo 2 segundos

int estadoMovimiento = 0; 
unsigned long tiempoInicio = 0; 

void setup() {
  Serial.begin(115200);
  Wire.begin(6, 7);
  sensor.initialize();

  if (sensor.testConnection()) {
    Serial.println("Sensor listo.");
  } else {
    Serial.println("Error al iniciar el sensor.");
  }
}

void loop() {
  sensor.getAcceleration(&ax, &ay, &az);

  ax_ms2 = (ax / 16384.0) * 9.81;
  ay_ms2 = (ay / 16384.0) * 9.81;
  az_ms2 = (az / 16384.0) * 9.81;

  float brutaX = atan2(ax_ms2, sqrt(ay_ms2 * ay_ms2 + az_ms2 * az_ms2)) * 180.0 / PI;
  float brutaY = atan2(ay_ms2, sqrt(ax_ms2 * ax_ms2 + az_ms2 * az_ms2)) * 180.0 / PI;
  float brutaZ = atan2(az_ms2, sqrt(ax_ms2 * ax_ms2 + ay_ms2 * ay_ms2)) * 180.0 / PI;

  inclX = brutaX - offset_X;
  inclY = brutaY - offset_Y;
  inclZ = brutaZ - offset_Z;
  
  Serial.println(inclX);
  Serial.println(inclY);
  Serial.println(inclZ);
  Serial.println("--------------------------------------------");

  // ==================================================
  // EVALUADOR CON FILTRO ANTI-VIBRACIÓN
  // ==================================================
  
  if (estadoMovimiento == 0) {
    // Paso 1: Detección en la izquierda
    if (inclY <= -35.0 && inclX >= MIN_X_PERMITIDO && inclX <= MAX_X_PERMITIDO) {
      estadoMovimiento = 1; 
      tiempoInicio = millis(); 
    }
  } 
  else if (estadoMovimiento == 1) {
    // Paso 2: Obligatorio pasar cerca del centro para confirmar que es un movimiento continuo
    if (abs(inclY) <= 15.0 && inclX >= MIN_X_PERMITIDO && inclX <= MAX_X_PERMITIDO) {
      estadoMovimiento = 2; // Ya pasó por el centro, va camino a la derecha
    }
    // Si se sale de los límites de X o tarda mucho
    else if (inclX < MIN_X_PERMITIDO || inclX > MAX_X_PERMITIDO || (millis() - tiempoInicio > TIEMPO_MAXIMO)) {
      Serial.println("¡Está mal!");
      estadoMovimiento = 0; 
      delay(250);
    }
  }
  else if (estadoMovimiento == 2) {
    unsigned long duracion = millis() - tiempoInicio;

    // Paso 3: Llegada a la derecha
    if (inclY >= 45.0) {
      // Si pasó por el centro Y ADEMÁS duró más del tiempo mínimo, es válido
      if (duracion >= TIEMPO_MINIMO) {
        Serial.println("¡Está bien!");
      } else {
        Serial.println("¡Está mal! (Demasiado rápido / Vibración)");
      }
      estadoMovimiento = 0; 
      delay(250); 
    }
    
    // Verificaciones de error habituales (límites de X o tiempo agotado)
    else if (inclX < MIN_X_PERMITIDO || inclX > MAX_X_PERMITIDO || (duracion > TIEMPO_MAXIMO)) {
      Serial.println("¡Está mal!");
      estadoMovimiento = 0; 
      delay(250); 
    }
  }
  
  delay(100);
}