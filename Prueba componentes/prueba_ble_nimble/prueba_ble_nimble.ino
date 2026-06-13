#include <NimBLEDevice.h>

NimBLEServer* pServer;
NimBLECharacteristic* pCharacteristic;

void setup() {
  Serial.begin(115200);

  // Nombre visible del dispositivo BLE (lo que ves al escanear)
  NimBLEDevice::init("Techeck_ESP32");

  // Crear servidor BLE
  pServer = NimBLEDevice::createServer();

  // Servicio con UUID simple
  NimBLEService* pService = pServer->createService("11111111-1111-1111-1111-111111111111"); // TecheckService

  // Característica con UUID simple
  pCharacteristic = pService->createCharacteristic(
    "22222222-2222-2222-2222-222222222222", // TecheckFeedbackCharacteristic
    NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ
  );

  // Iniciar servicio y publicidad
  pService->start();
  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(pService->getUUID());
  NimBLEDevice::startAdvertising();

  Serial.println("Techeck BLE listo, conecta tu app Flutter.");
}

void loop() {
  // Enviar mensaje fijo cada 2 segundos
  pCharacteristic->setValue("Hola desde Techeck");
  pCharacteristic->notify();
  Serial.println("Notificación enviada: Hola desde Techeck");
  delay(2000);
}
