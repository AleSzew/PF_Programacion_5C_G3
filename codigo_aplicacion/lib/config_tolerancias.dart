import 'dart:convert';
import 'package:codigo_aplicacion/screens/pantalla_bluetooth.dart';

class ConfigTolerancias {
  // 1. Ángulos de tolerancia
  static double desviacionPostura = 11.0;    // Grados máximos de desvío (°)
  static double holguraReposo = 7.0;         // Margen en posición inicial (°)
  static double excesoRecorrido = 10.0;      // Grados extra permitidos sobre el máximo (°)
  static double amplitudMinCalib = 10.0;     // Amplitud mínima para calibrar (°)

  // 2. Umbrales de recorrido
  static double sensibilidadInicioPct = 30.0;  // UMBRAL_SALIDA (%)
  static double sensibilidadRetornoPct = 35.0; // UMBRAL_RETORNO (%)
  static double rangoMinimoPct = 45.0;         // UMBRAL_PICO (%)

  // 3. Tiempos y velocidad
  static double controlImpulsoPct = 60.0;      // Factor tiempo mínimo (%)
  static double tiempoMaxRep = 3.5;            // Multiplicador tiempo máximo (x)
  static int duracionMinAbsolutaMs = 200;      // T_MIN (ms)
  static int tiempoMinEntreRepsMs = 800;       // T_MIN_ENTRE_REPS (ms)
  static double tiempoInactividad = 4.5;       // TIEMPO_MAX_INACTIVIDAD (s)

  // 4. Estabilidad y confirmaciones (en muestras de 100 ms)
  static double toleranciaTemblor = 2.0;       // RATIO_SUAVIDAD_MAX
  static double toleranciaRetroceso = 20.0;    // TOLERANCIA_VEL_DPS (°/s)
  static int muestrasConfirmPostura = 2;       // MUESTRAS_CONFIRM_POSTURA (x100ms)
  static int muestrasConfirmVel = 4;           // MUESTRAS_CONFIRM_VEL (x100ms)
  static int muestrasConfirmRango = 5;         // MUESTRAS_CONFIRM_RANGO (x100ms)

  // 5. Hardware / Auxiliares
  static bool requerirAuxiliares = true;       // REQUERIR_AUXILIARES

  static void restaurarPorDefecto() {
    desviacionPostura = 11.0;
    holguraReposo = 7.0;
    excesoRecorrido = 10.0;
    amplitudMinCalib = 10.0;
    sensibilidadInicioPct = 30.0;
    sensibilidadRetornoPct = 35.0;
    rangoMinimoPct = 45.0;
    controlImpulsoPct = 60.0;
    tiempoMaxRep = 3.5;
    duracionMinAbsolutaMs = 200;
    tiempoMinEntreRepsMs = 800;
    tiempoInactividad = 4.5;
    toleranciaTemblor = 2.0;
    toleranciaRetroceso = 20.0;
    muestrasConfirmPostura = 2;
    muestrasConfirmVel = 4;
    muestrasConfirmRango = 5;
    requerirAuxiliares = true;
  }

  static Future<bool> sincronizarConESP32() async {
    if (!yaEstaConectado || caracteristicaGlobal == null) {
      return false;
    }

    double umbralPico = (rangoMinimoPct / 100.0).clamp(0.01, 1.0);
    double umbralSalida = (sensibilidadInicioPct / 100.0).clamp(0.01, 0.95);
    double umbralRetorno = (sensibilidadRetornoPct / 100.0).clamp(0.01, 0.95);
    double factorMin = ((100.0 - controlImpulsoPct) / 100.0).clamp(0.01, 0.99);
    int inactividadMs = (tiempoInactividad * 1000).round();
    int auxFlag = requerirAuxiliares ? 1 : 0;

    // 18 valores separados por coma
    String comando = "TOL:"
        "${desviacionPostura.toStringAsFixed(1)},"
        "${holguraReposo.toStringAsFixed(1)},"
        "${excesoRecorrido.toStringAsFixed(1)},"
        "${umbralPico.toStringAsFixed(2)},"
        "${factorMin.toStringAsFixed(2)},"
        "${tiempoMaxRep.toStringAsFixed(1)},"
        "${toleranciaTemblor.toStringAsFixed(1)},"
        "$inactividadMs,"
        "${toleranciaRetroceso.toStringAsFixed(1)},"
        "${umbralSalida.toStringAsFixed(2)},"
        "${umbralRetorno.toStringAsFixed(2)},"
        "$tiempoMinEntreRepsMs,"
        "${amplitudMinCalib.toStringAsFixed(1)},"
        "$duracionMinAbsolutaMs,"
        "$muestrasConfirmPostura,"
        "$muestrasConfirmVel,"
        "$muestrasConfirmRango,"
        "$auxFlag";

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
