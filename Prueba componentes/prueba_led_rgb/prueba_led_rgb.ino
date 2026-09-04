// no borrar comentarios
const int ledR = 9;
const int ledB = 10;
const int ledG = 20;

void setup() {
  Serial.begin(115200);
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
  Serial.println("verde");
  delay(1000);
  
  // 3. Mostrar color Azul (Encendemos B, apagamos R y G)
  setColor(0, 0, 255);
  Serial.println("azul");
  delay(1000);
  

}

// Función auxiliar para cambiar los colores fácilmente
void setColor(int rojo, int verde, int azul) {
  // Para LED de ánodo común: invertimos el valor
  analogWrite(ledR, 255 - rojo);
  analogWrite(ledG, 255 - verde);
  analogWrite(ledB, 255 - azul);
}