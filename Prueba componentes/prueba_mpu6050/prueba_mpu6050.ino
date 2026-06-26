//--------------------------------------------------MPU6050----------------------
#include "Wire.h"
#include "MPU6050.h"

MPU6050 sensor;

// CORRECCIÓN: Las lecturas crudas deben ser enteros de 16 bits
int16_t ax, ay, az;
int16_t gx, gy, gz;

// Las conversiones e inclinaciones sí llevan decimales (float)
float ax_ms2, ay_ms2, az_ms2;
float inclX, inclY, inclZ;

void setup() {
  Serial.begin(115200);
  Wire.begin(6, 7); // SDA en GPIO 6, SCL en GPIO 7 (Asegurate de que coincida con tus cables)
  Serial.println("hola");
  sensor.initialize();

  if (sensor.testConnection()) {
    Serial.println("MPU6050 conectado correctamente ");
  } else {
    Serial.println("Error al conectar el MPU6050 ");
  }
}

void loop() {
  // Ahora la librería va a aceptar las variables sin protestar
  sensor.getAcceleration(&ax, &ay, &az);
  sensor.getRotation(&gx, &gy, &gz);

  Serial.print("RAW Accel: ");
  Serial.print(ax); Serial.print(", ");
  Serial.print(ay); Serial.print(", ");
  Serial.println(az);

  Serial.print("RAW Gyro: ");
  Serial.print(gx); Serial.print(", ");
  Serial.print(gy); Serial.print(", ");
  Serial.println(gz);

  // Conversión a m/s² (Al dividir por 16384.0, Arduino convierte el resultado automáticamente a float)
  ax_ms2 = (ax / 16384.0) * 9.81;
  ay_ms2 = (ay / 16384.0) * 9.81;
  az_ms2 = (az / 16384.0) * 9.81;

  // Inclinaciones
  inclX = atan2(ax_ms2, sqrt(ay_ms2*ay_ms2 + az_ms2*az_ms2)) * 180.0 / PI;
  inclY = atan2(ay_ms2, sqrt(ax_ms2*ax_ms2 + az_ms2*az_ms2)) * 180.0 / PI;
  inclZ = atan2(az_ms2, sqrt(ax_ms2*ax_ms2 + ay_ms2*ay_ms2)) * 180.0 / PI;

  Serial.print("Accel (m/s²): ");
  Serial.print(ax_ms2); Serial.print(", ");
  Serial.print(ay_ms2); Serial.print(", ");
  Serial.println(az_ms2);

  Serial.print("Inclinaciones (°): ");
  Serial.print(inclX); Serial.print(", ");
  Serial.print(inclY); Serial.print(", ");
  Serial.println(inclZ);

  Serial.println("-----------------------------");
  delay(1000);
}