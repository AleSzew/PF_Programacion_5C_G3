// Librerías I2C para controlar el MPU6050
// La librería MPU6050.h necesita I2Cdev.h, y esta necesita Wire.h
//configurar usb cdc on boot en enabled
#include "I2Cdev.h"
#include "MPU6050.h"
#include "Wire.h"

// La dirección del MPU6050 puede ser 0x68 o 0x69, dependiendo
// del estado de AD0. Si no se especifica, 0x68 estará implícito.
MPU6050 sensor;

// CORRECCIÓN CLAVE PARA ESP32: Usar int16_t en lugar de int
// Valores RAW (sin procesar) del acelerómetro y giroscopio
int16_t ax, ay, az;
int16_t gx, gy, gz;
float ax_ms2, ay_ms2, az_ms2;
float inclX, inclY, inclZ;

void setup() {
  // Iniciando puerto serial a 115200 (Mejor velocidad para el ESP32)
  Serial.begin(115200);

  // Iniciando I2C con los pines libres del ESP32-C3 Super Mini (SDA=6, SCL=7)
  Wire.begin(6, 7);

  // Iniciando el sensor
  Serial.println("Iniciando MPU6050...");
  sensor.initialize();

  // Comprobando conexión
  if (sensor.testConnection()) {
    Serial.println("Sensor iniciado correctamente");
  } else {
    Serial.println("Error al iniciar el sensor. Revisa los cables.");
  }
}

void loop() {
  // Ahora la librería va a aceptar las variables sin protestar
  sensor.getAcceleration(&ax, &ay, &az);
  sensor.getRotation(&gx, &gy, &gz);

  Serial.print("RAW Accel: ");
  Serial.print(ax);
  Serial.print(", ");
  Serial.print(ay);
  Serial.print(", ");
  Serial.println(az);

  Serial.print("RAW Gyro: ");
  Serial.print(gx);
  Serial.print(", ");
  Serial.print(gy);
  Serial.print(", ");
  Serial.println(gz);

  // Conversión a m/s² (Al dividir por 16384.0, Arduino convierte el resultado automáticamente a float)
  ax_ms2 = (ax / 16384.0) * 9.81;
  ay_ms2 = (ay / 16384.0) * 9.81;
  az_ms2 = (az / 16384.0) * 9.81;

  // Inclinaciones
  inclX = atan2(ax_ms2, sqrt(ay_ms2 * ay_ms2 + az_ms2 * az_ms2)) * 180.0 / PI;
  inclY = atan2(ay_ms2, sqrt(ax_ms2 * ax_ms2 + az_ms2 * az_ms2)) * 180.0 / PI;
  inclZ = atan2(az_ms2, sqrt(ax_ms2 * ax_ms2 + ay_ms2 * ay_ms2)) * 180.0 / PI;

  Serial.print("Accel (m/s²): ");
  Serial.print(ax_ms2);
  Serial.print(", ");
  Serial.print(ay_ms2);
  Serial.print(", ");
  Serial.println(az_ms2);

  Serial.print("Inclinaciones (°): ");
  Serial.print(inclX);
  Serial.print(", ");
  Serial.print(inclY);
  Serial.print(", ");
  Serial.println(inclZ);

  Serial.println("-----------------------------");
  delay(1000);
}