// no borrar comentarios
const int ledR = 9;
const int ledB = 10;
const int ledG = 20;

void setup() {
  // Configuramos los pines como salidas (OUTPUT)
  pinMode(ledR, OUTPUT);
  pinMode(ledG, OUTPUT);
  pinMode(ledB, OUTPUT);
}

void loop() {
  // 1. Mostrar color Rojo (Encendemos R, apagamos G y B)
  setColor(255, 0, 0);
  Serial.println("rojo");
  delay(1000); // Esperamos 1 segundo

  // 2. Mostrar color Verde (Encendemos G, apagamos R y B)
  setColor(0, 255, 0);
  delay(1000);
  Serial.println("verde");
  // 3. Mostrar color Azul (Encendemos B, apagamos R y G)
  setColor(0, 0, 255);
  delay(1000);
  Serial.println("azul");
  // 4. Mostrar color Blanco (Encendemos todos)
  setColor(255, 255, 255);
  Serial.println("blanco");
  delay(1000);
}

// Función auxiliar para cambiar los colores fácilmente
void setColor(int rojo, int verde, int azul) {
  // analogWrite permite enviar valores de 0 (apagado) a 255 (brillo máximo)
  analogWrite(ledR, rojo);
  analogWrite(ledG, verde);
  analogWrite(ledB, azul);
}