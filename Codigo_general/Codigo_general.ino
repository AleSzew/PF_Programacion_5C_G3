//NO BORRAR COMENTARIOS NUNCA C3 2026
#include <MPU6050.h>
#include "Wire.h"
#include "WiFi.h"
#include <HTTPClient.h>
#include <WebServer.h>
#include <Ticker.h>
#include <NimBLEDevice.h>

MPU6050 sensor;
NimBLEServer* pServer;
NimBLECharacteristic* pCharacteristic;

// Tickers 
Ticker timerBoton;
Ticker timerAntirrebote; 
Ticker timerMedicion;

int contadorErrores = 0;

// Cronómetro manejado por Ticker para no saturar el WiFi
bool flagMedicion = false;
int INTERVALO_MEDICION = 100; // 1000 ms 

// --- Máquinas de estados ---
typedef enum {
  INICIALIZACION,
  MEDICIONES,
  ANALISIS_SERIE,
  C_APLICACION
} estadoMaq_General_t;

estadoMaq_General_t estadoMaq_General = INICIALIZACION;

// --- Variables y Máquina de Antirrebote ---
typedef enum {
  ESPERA, 
  CONFIRMACION, 
  LIBERACION
} estadoAntirrebote_t;

estadoAntirrebote_t estadoBoton = ESPERA;
 int msBoton = 0; // Se incrementa cada 1 milisegundo gracias al Ticker
bool flagBoton = false;
#define T_REBOTE 10 // 10 milisegundos es el estandar para eliminar el ruido mecánico

// Sensor local 
int16_t ax_local, ay_local, az_local;
int16_t gx_local, gy_local, gz_local;

// Los valores calculados
float ax_ms2_local, ay_ms2_local, az_ms2_local;
float inclX_local, inclY_local, inclZ_local;

// Sensor auxiliar 
float ax_aux, ay_aux, az_aux;
float gx_aux, gy_aux, gz_aux;
float ax_ms2_aux, ay_ms2_aux, az_ms2_aux;
float inclX_aux, inclY_aux, inclZ_aux;

// Valores estándar
float std_ax, std_ay, std_az;
float std_inclX, std_inclY, std_inclZ;
bool estandarCalibrado = false;
String resultadoValidacion = "";
int segundosBoton = 0;

#define PIN_BOTON 1
#define PIN_LED_R 9
#define PIN_LED_G 20
#define PIN_LED_B 10

// Direcciones del servidor HTTP del dispositivo auxiliar
const char* serverNameAx = "http://192.168.4.2/Ax";
const char* serverNameAy = "http://192.168.4.2/Ay";
const char* serverNameAz = "http://192.168.4.2/Az";

// Credenciales de la red WiFi que este ESP32 va a crear
const char* ssid = "ESP32_C3_Server";
const char* password = "GRUPO3";

// Clase para manejar los eventos de recepcion de Bluetooth Low Energy (BLE)
class MiCharacteristicCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pCharacteristic) { 
    // Esta funcion se ejecuta de forma asincrona cada vez que el cliente BLE envia un dato
    std::string valor = pCharacteristic->getValue();
    Serial.print("Recibido BLE: ");
    Serial.println(valor.c_str());

    int codigo = atoi(valor.c_str());
    Serial.print("ID recibido: ");
    Serial.println(codigo);
  }
};

void setup() {
  Serial.begin(115200);
  Wire.begin(6, 7);
  sensor.initialize();
  pinMode(PIN_BOTON, INPUT_PULLUP);
  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_B, OUTPUT);

  // --- CONFIGURACIÓN SEGURA DE WIFI ---
  Serial.println("Iniciando AP WiFi...");
  
  // Se configura el ESP32 como Access Point (Punto de Acceso)
  // Esto significa que crea su propia red WiFi a la que otros se pueden conectar, en lugar de conectarse a un router.
  WiFi.mode(WIFI_AP); 
  WiFi.softAP(ssid, password);
  delay(500); 
  
  // Por defecto, la IP del Access Point en el ESP32 suele ser 192.168.4.1
  Serial.print("AP WiFi iniciado correctamente. IP: ");
  Serial.println(WiFi.softAPIP());

  inicializarBLE();
  
  // timerBoton cuenta de a 1 segundo para la pulsacion larga
  timerBoton.attach(1, funcionTimerBoton); 
  
  // timerAntirrebote cuenta de a 1 milisegundo (usamos attach_ms) para el debounce
  timerAntirrebote.attach_ms(1, funcionTimerAntirrebote);
  // timerMedicion se ejecuta cada 100ms para habilitar la lectura de sensores
  timerMedicion.attach_ms(INTERVALO_MEDICION, funcionTimerMedicion);
}

void loop() {
  // Ejecutamos la lectura limpia del boton antes de la maquina general
  maquinaAntirrebote();
  Maq_General();
  
}

// === MÁQUINA DE ESTADOS DEL ANTIRREBOTE ===
void maquinaAntirrebote() {
  bool lecturaBoton = digitalRead(PIN_BOTON); // Con INPUT_PULLUP, LOW es presionado

  switch (estadoBoton) {
    case ESPERA:
      if (lecturaBoton == LOW) {  
        msBoton = 0; // Reiniciamos el contador de milisegundos
        estadoBoton = CONFIRMACION;
      }
      break;
      
    case CONFIRMACION:
      // Esperamos que el Ticker haya sumado T_REBOTE (10) milisegundos
      if (msBoton >= T_REBOTE) {
        if (lecturaBoton == LOW) {
          estadoBoton = LIBERACION;
        } else {
          estadoBoton = ESPERA; // Falsa alarma, volvemos a inicio
        }
      }
      break;
      
    case LIBERACION:
      if (lecturaBoton == HIGH) { // El usuario soltó el botón
        flagBoton = true; // Habilitamos la bandera para que la máquina general actúe
        estadoBoton = ESPERA;
      }
      break;
  }
}

void Maq_General() {
  switch (estadoMaq_General) {
    case INICIALIZACION:
      Serial.println("Estado inicializacion");
      digitalWrite(PIN_LED_G, HIGH);  
      if (flagBoton) {
        flagBoton = false; // Importante apagar el flag una vez usado
        Serial.println("se toco el boton ir a funciones");
        mediciones();  
        recibirValores();
        calibrarEstandar();  
        estandarCalibrado = true;
  
        estadoMaq_General = MEDICIONES;
      }
      break;

    case MEDICIONES:
      Serial.println("Estado mediciones");
      digitalWrite(PIN_LED_G, LOW);
      
      if (flagMedicion) {
        flagMedicion = false; //cada 100 ms se lee el sensor y se comparan los valores con el estandar
        recibirValores();
        mediciones();
        if (estandarCalibrado) {
          compararConEstandar();
        }
      }
      if (flagBoton) {
        flagBoton = false;
        estadoMaq_General = ANALISIS_SERIE;
      }
      break;

    case ANALISIS_SERIE:
      Serial.println("Estado analisis serie");
      // Evalúa pulsación mantenida de 5 segundos, gestionada por funcionTimerBoton.
      if (segundosBoton >= 5) {
        Serial.println("Botón presionado >5s, apagar sistema.");
        resultadoValidacion = "SERIE TERMINADA: " + String(contadorErrores) + " errores";
        Serial.println(resultadoValidacion);
        // Reiniciamos variables y  botón
        segundosBoton = 0; 
        flagBoton = false; 
        estadoMaq_General = C_APLICACION;
      }
      break;

    case C_APLICACION:
      Serial.println("Estado conexion aplicacion");
      enviarFeedbackBLE(resultadoValidacion);
      if (flagBoton) {
        flagBoton = false;
        estadoMaq_General = INICIALIZACION;
      }
      break;
  }
}

void mediciones() {
  // Ahora sí leemos los valores reales del acelerómetro y giroscopio
  sensor.getAcceleration(&ax_local, &ay_local, &az_local);
  sensor.getRotation(&gx_local, &gy_local, &gz_local);

  ax_ms2_local = (ax_local / 16384.0) * 9.81;
  ay_ms2_local = (ay_local / 16384.0) * 9.81;
  az_ms2_local = (az_local / 16384.0) * 9.81;

  inclX_local = atan2(ax_ms2_local, sqrt(ay_ms2_local * ay_ms2_local + az_ms2_local * az_ms2_local)) * 180.0 / PI;
  inclY_local = atan2(ay_ms2_local, sqrt(ax_ms2_local * ax_ms2_local + az_ms2_local * az_ms2_local)) * 180.0 / PI;
  inclZ_local = atan2(az_ms2_local, sqrt(ax_ms2_local * ax_ms2_local + ay_ms2_local * ay_ms2_local)) * 180.0 / PI;

  Serial.println("Lectura LOCAL REAL:");
  Serial.println(String(ax_ms2_local) + "," + String(ay_ms2_local) + "," + String(az_ms2_local) + "," + String(inclX_local) + "," + String(inclY_local) + "," + String(inclZ_local));
}

void calibrarEstandar() {
  std_ax = ax_ms2_local;
  std_ay = ay_ms2_local;
  std_az = az_ms2_local;
  std_inclX = inclX_local;
  std_inclY = inclY_local;
  std_inclZ = inclZ_local;
  Serial.println("Valores estándar guardados");
}

void compararConEstandar() {
  Serial.println("Comparacion con estandar");
  float tol_acc = 2.0;
  float tol_incl = 2.0;
  bool correcto_local = true;
  bool correcto_aux = true;

  if (abs(ax_ms2_local - std_ax) > tol_acc) correcto_local = false;
  if (abs(ay_ms2_local - std_ay) > tol_acc) correcto_local = false;
  if (abs(az_ms2_local - std_az) > tol_acc) correcto_local = false;
  if (abs(inclX_local - std_inclX) > tol_incl) correcto_local = false;
  if (abs(inclY_local - std_inclY) > tol_incl) correcto_local = false;
  if (abs(inclZ_local - std_inclZ) > tol_incl) correcto_local = false;

  if (abs(ax_ms2_aux - std_ax) > tol_acc) correcto_aux = false;
  if (abs(ay_ms2_aux - std_ay) > tol_acc) correcto_aux = false;
  if (abs(az_ms2_aux - std_az) > tol_acc) correcto_aux = false;
  if (abs(inclX_aux - std_inclX) > tol_incl) correcto_aux = false;
  if (abs(inclY_aux - std_inclY) > tol_incl) correcto_aux = false;
  if (abs(inclZ_aux - std_inclZ) > tol_incl) correcto_aux = false;

  if (correcto_local && correcto_aux) {
    resultadoValidacion = "CORRECTO";
    Serial.println("Movimiento CORRECTO");
  } else {
    resultadoValidacion = "INCORRECTO";
    Serial.println("Movimiento INCORRECTO");
    contadorErrores++;
  }
}

void recibirValores() {
  String ax = httpGETRequest(serverNameAx);
  String ay = httpGETRequest(serverNameAy);
  String az = httpGETRequest(serverNameAz);

  ax_ms2_aux = ax.toFloat();
  ay_ms2_aux = ay.toFloat();
  az_ms2_aux = az.toFloat();

  inclX_aux = atan2(ax_ms2_aux, sqrt(ay_ms2_aux * ay_ms2_aux + az_ms2_aux * az_ms2_aux)) * 180.0 / PI;
  inclY_aux = atan2(ay_ms2_aux, sqrt(ax_ms2_aux * ax_ms2_aux + az_ms2_aux * az_ms2_aux)) * 180.0 / PI;
  inclZ_aux = atan2(az_ms2_aux, sqrt(ax_ms2_aux * ax_ms2_aux + ay_ms2_aux * ay_ms2_aux)) * 180.0 / PI;
}

void inicializarBLE() {
  Serial.println("inicilizar ble");
  // Se inicializa el dispositivo con el nombre que vera el usuario al escanear
  NimBLEDevice::init("Techeck_ESP32");
  pServer = NimBLEDevice::createServer();
  
  // Se crea un servicio BLE. El UUID funciona como un identificador unico para que la app sepa que hace este servicio.
  NimBLEService* pService = pServer->createService("11111111-1111-1111-1111-111111111111");
  
  // Dentro del servicio se crea una caracteristica. Se configuran permisos:
  // READ (leer), WRITE (escribir hacia el ESP32) y NOTIFY (el ESP32 puede enviar datos a la app sin que esta los pida).
  pCharacteristic = pService->createCharacteristic(
    "22222222-2222-2222-2222-222222222222",
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);
    
  pCharacteristic->setCallbacks(new MiCharacteristicCallbacks());
  pCharacteristic->setValue("ESP32 listo");
  pService->start();
  
  // El Advertising permite que el dispositivo empiece a emitir su presencia para que los telefonos lo puedan encontrar.
  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(pService->getUUID());
  NimBLEDevice::startAdvertising();
}

void enviarFeedbackBLE(const String& mensaje) {
  Serial.println("Enviara feedback");
  // Verifica si el puntero de la caracteristica existe antes de intentar enviar datos, evitando reseteos por fallos de memoria.
  if (pCharacteristic != nullptr) {
    pCharacteristic->setValue(mensaje.c_str());
    // El metodo notify() empuja el mensaje actualizado a cualquier cliente que este suscrito.
    pCharacteristic->notify();
    Serial.println("-> Intentando notificar por BLE: " + mensaje);
  }
}

// Función ejecutada por el timerBoton cada 1 segundo (Pulsación larga)
void funcionTimerBoton() {
  if (digitalRead(PIN_BOTON) == LOW) {
    segundosBoton++;
  } else {
    segundosBoton = 0;
  }
}

// Función ejecutada por el timerAntirrebote cada 1 milisegundo (Debounce corto)
void funcionTimerAntirrebote() {
  msBoton++;
}
// Función ejecutada por el timerMedicion cada 100 milisegundos
void funcionTimerMedicion() {
  flagMedicion = true;
}

String httpGETRequest(const char* serverName) {
  // softAPgetStationNum() devuelve la cantidad de dispositivos conectados a la red del ESP32.
  // Sirve para no perder tiempo haciendo peticiones HTTP si sabemos que el dispositivo auxiliar ni siquiera esta conectado por WiFi.
  if (WiFi.softAPgetStationNum() == 0) {
    return "0.0";
  }
  HTTPClient http;
  http.begin(serverName);
  // Realiza la peticion GET de manera sincrona (bloquea el codigo hasta obtener respuesta o timeout).
  int httpResponseCode = http.GET();
  String payload = "0.0"; 
  
  // Un codigo mayor a 0 indica que el servidor respondio (ejemplo: 200 OK).
  if (httpResponseCode > 0) {
    payload = http.getString();
  }
  http.end();
  return payload;
}