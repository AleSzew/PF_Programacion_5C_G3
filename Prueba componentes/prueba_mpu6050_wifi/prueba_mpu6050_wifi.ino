#include "I2Cdev.h"
#include "MPU6050.h"
#include "Wire.h"

MPU6050 mpu;

// Variables para almacenar las lecturas brutas
int16_t ax, ay, az;
int16_t gx, gy, gz;

void setup() {
  Serial.begin(9600);
  Wire.begin(44,1);

  // Inicializar sensor
  mpu.initialize();

  // Verificar conexión
  if (mpu.testConnection()) {
    Serial.println("MPU6050 conectado correctamente");
  } else {
    Serial.println("Error al conectar MPU6050");
  }
}

void loop() {
  // Leer acelerómetro y giroscopio de una sola vez
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

  // Mostrar datos en el Monitor Serie
  Serial.print("Aceleracion X Y Z: ");
  Serial.print(ax); Serial.print("\t");
  Serial.print(ay); Serial.print("\t");
  Serial.print(az); Serial.print("\t | ");

  Serial.print("Giroscopio X Y Z: ");
  Serial.print(gx); Serial.print("\t");
  Serial.print(gy); Serial.print("\t");
  Serial.println(gz);

  delay(200);
}