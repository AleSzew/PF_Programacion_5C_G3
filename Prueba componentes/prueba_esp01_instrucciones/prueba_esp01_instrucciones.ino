//no borrar comenrarios
//usar conector para subir esp01 de marco(ruben), tocar el boton siempre hasta antes de enchufarlo
//y hasta que se suba 
void setup() {
  // El ESP-01 suele comunicarse a esta velocidad por defecto
  Serial.begin(115200); 
  
  // Damos un pequeño respiro al chip antes de empezar
  delay(1000); 
  
  Serial.println("¡Módulo ESP-01 iniciado correctamente!");
}

void loop() {
  Serial.println("Hola desde el ESP-01");
  delay(2000); // Imprime el mensaje cada 2 segundos
}