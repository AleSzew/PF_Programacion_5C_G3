import 'dart:convert';
import 'package:codigo_aplicacion/screens/pantalla_bluetooth.dart';

class ConfigTolerancias {
  static double desviacionPostura = 11.0; // Grados máximos de inclinación permitidos
  static double controlImpulso = 60.0;    // % de exigencia contra movimientos bruscos/rápidos
  static double tiempoMaxRep = 3.5;       // Multiplicador de tiempo antes de marcar "Muy lento"
  static double holguraReposo = 7.0;      // Grados de margen en posición inicial

  static Future<bool> sincronizarConESP32() async {
    if (!yaEstaConectado || caracteristicaGlobal == null) {
      return false;
    }

    // Convertir % de control de impulso a factorDuracionMin del ESP32 (ej: 60% -> 0.40)
    double factorMin = (100.0 - controlImpulso) / 100.0;
    if (factorMin < 0.2) factorMin = 0.2;

    String comando =
        "TOL:${desviacionPostura.toStringAsFixed(1)},${factorMin.toStringAsFixed(2)},${tiempoMaxRep.toStringAsFixed(1)},${holguraReposo.toStringAsFixed(1)}";

    try {
      await flutterReactiveBleGlobal.writeCharacteristicWithResponse(
        caracteristicaGlobal!,
        value: utf8.encode(comando),
      );
      return true;
    } catch (e) {
      return false;
    }
  }
}
