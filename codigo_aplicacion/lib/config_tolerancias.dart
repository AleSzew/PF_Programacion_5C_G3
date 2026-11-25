import 'dart:convert';
import 'package:codigo_aplicacion/screens/pantalla_bluetooth.dart';

class ConfigTolerancias {
  // 1. Ángulos de tolerancia
  static double desviacionPostura = 11.0; // Desde 1° sin límite superior
  static double holguraReposo = 7.0;      // Margen en posición inicial (°)
  static double excesoRecorrido = 10.0;   // Grados extra permitidos sobre el máximo (°)

  // 2. Recorrido y ritmo
  static double rangoMinimoPct = 45.0;    // % mínimo del recorrido calibrado (ej: 45%)
  static double controlImpulsoPct = 60.0; // Exigencia contra repetición muy rápida (%)
  static double tiempoMaxRep = 3.5;       // Multiplicador tiempo máximo (muy lento)

  // 3. Estabilidad e inactividad
  static double toleranciaTemblor = 2.0;  // Ratio de suavidad / brusquedad
  static double tiempoInactividad = 4.5;  // Segundos quieto antes de marcar error
  static double toleranciaRetroceso = 20.0; // Grados/seg permitidos en sentido contrario

  static void restaurarPorDefecto() {
    desviacionPostura = 11.0;
    holguraReposo = 7.0;
    excesoRecorrido = 10.0;
    rangoMinimoPct = 45.0;
    controlImpulsoPct = 60.0;
    tiempoMaxRep = 3.5;
    toleranciaTemblor = 2.0;
    tiempoInactividad = 4.5;
    toleranciaRetroceso = 20.0;
  }

  static Future<bool> sincronizarConESP32() async {
    if (!yaEstaConectado || caracteristicaGlobal == null) {
      return false;
    }

    // Conversiones a las unidades internas del ESP32
    double umbralPico = (rangoMinimoPct / 100.0).clamp(0.05, 1.0);
    double factorMin = ((100.0 - controlImpulsoPct) / 100.0).clamp(0.05, 0.95);
    int inactividadMs = (tiempoInactividad * 1000).round();

    // Formato: "TOL:postura,reposo,exceso,umbralPico,facMin,facMax,suavidad,inactMs,velDps"
    String comando = "TOL:"
        "${desviacionPostura.toStringAsFixed(1)},"
        "${holguraReposo.toStringAsFixed(1)},"
        "${excesoRecorrido.toStringAsFixed(1)},"
        "${umbralPico.toStringAsFixed(2)},"
        "${factorMin.toStringAsFixed(2)},"
        "${tiempoMaxRep.toStringAsFixed(1)},"
        "${toleranciaTemblor.toStringAsFixed(1)},"
        "$inactividadMs,"
        "${toleranciaRetroceso.toStringAsFixed(1)}";

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
}import 'dart:convert';
import 'package:codigo_aplicacion/screens/pantalla_bluetooth.dart';

class ConfigTolerancias {
  // 1. Ángulos de tolerancia
  static double desviacionPostura = 11.0; // Desde 1° sin límite superior
  static double holguraReposo = 7.0;      // Margen en posición inicial (°)
  static double excesoRecorrido = 10.0;   // Grados extra permitidos sobre el máximo (°)

  // 2. Recorrido y ritmo
  static double rangoMinimoPct = 45.0;    // % mínimo del recorrido calibrado (ej: 45%)
  static double controlImpulsoPct = 60.0; // Exigencia contra repetición muy rápida (%)
  static double tiempoMaxRep = 3.5;       // Multiplicador tiempo máximo (muy lento)

  // 3. Estabilidad e inactividad
  static double toleranciaTemblor = 2.0;  // Ratio de suavidad / brusquedad
  static double tiempoInactividad = 4.5;  // Segundos quieto antes de marcar error
  static double toleranciaRetroceso = 20.0; // Grados/seg permitidos en sentido contrario

  static void restaurarPorDefecto() {
    desviacionPostura = 11.0;
    holguraReposo = 7.0;
    excesoRecorrido = 10.0;
    rangoMinimoPct = 45.0;
    controlImpulsoPct = 60.0;
    tiempoMaxRep = 3.5;
    toleranciaTemblor = 2.0;
    tiempoInactividad = 4.5;
    toleranciaRetroceso = 20.0;
  }

  static Future<bool> sincronizarConESP32() async {
    if (!yaEstaConectado || caracteristicaGlobal == null) {
      return false;
    }

    // Conversiones a las unidades internas del ESP32
    double umbralPico = (rangoMinimoPct / 100.0).clamp(0.05, 1.0);
    double factorMin = ((100.0 - controlImpulsoPct) / 100.0).clamp(0.05, 0.95);
    int inactividadMs = (tiempoInactividad * 1000).round();

    // Formato: "TOL:postura,reposo,exceso,umbralPico,facMin,facMax,suavidad,inactMs,velDps"
    String comando = "TOL:"
        "${desviacionPostura.toStringAsFixed(1)},"
        "${holguraReposo.toStringAsFixed(1)},"
        "${excesoRecorrido.toStringAsFixed(1)},"
        "${umbralPico.toStringAsFixed(2)},"
        "${factorMin.toStringAsFixed(2)},"
        "${tiempoMaxRep.toStringAsFixed(1)},"
        "${toleranciaTemblor.toStringAsFixed(1)},"
        "$inactividadMs,"
        "${toleranciaRetroceso.toStringAsFixed(1)}";

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
