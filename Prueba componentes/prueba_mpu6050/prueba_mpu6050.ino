#include "I2Cdev.h"
#include "MPU6050.h"
#include "Wire.h"

MPU6050 sensor;

int16_t ax, ay, az;
int16_t gx, gy, gz;
float ax_ms2, ay_ms2, az_ms2;
float inclX, inclY, inclZ;



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

  float inclX = atan2(ax_ms2, sqrt(ay_ms2 * ay_ms2 + az_ms2 * az_ms2)) * 180.0 / PI;
  float inclY = atan2(ay_ms2, sqrt(ax_ms2 * ax_ms2 + az_ms2 * az_ms2)) * 180.0 / PI;
  float inclZ = atan2(az_ms2, sqrt(ax_ms2 * ax_ms2 + ay_ms2 * ay_ms2)) * 180.0 / PI;

  
  Serial.println(inclX);
  Serial.println(inclY);
  Serial.println(inclZ);
  Serial.println("--------------------------------------------");

  delay(500);
}