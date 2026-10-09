// Escáner de pines GPIO para ESP32-C3

// Lista de GPIOs accesibles en el ESP32-C3
// (Se excluyen GPIO20 y GPIO21 para no perder la comunicación Serial RX/TX)
const int pinesESP32C3[] = {20, 21, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 18, 19};
const int totalPines = sizeof(pinesESP32C3) / sizeof(pinesESP32C3[0]);

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=== ESCÁNER DE PINES GPIO ===");
  Serial.println("Mira el LED y observa en el Monitor Serie qué número de GPIO se enciende.\n");

  // Configurar todos los pines de la lista como salida en LOW
  for (int i = 0; i < totalPines; i++) {
    pinMode(pinesESP32C3[i], OUTPUT);
    digitalWrite(pinesESP32C3[i], LOW);
  }
}

void loop() {
  for (int i = 0; i < totalPines; i++) {
    int pinActual = pinesESP32C3[i];

    Serial.print("Probando GPIO: ");
    Serial.println(pinActual);

    // Encender pin
    digitalWrite(pinActual, HIGH);
    delay(1000); // Permanece encendido 1 segundo

    // Apagar pin
    digitalWrite(pinActual, LOW);
    delay(200);  // Pausa entre pines
  }

  Serial.println("\n--- Ciclo completo. Reiniciando escaneo... ---\n");
  delay(1000);
}