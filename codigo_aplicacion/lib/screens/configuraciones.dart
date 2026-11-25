import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';
import 'package:codigo_aplicacion/config_tolerancias.dart';
import 'package:codigo_aplicacion/screens/pantalla_bluetooth.dart';

class Configuraciones extends StatefulWidget {
  const Configuraciones({super.key});

  @override
  State<Configuraciones> createState() => _ConfiguracionesState();
}

class _ConfiguracionesState extends State<Configuraciones> {
  bool enviando = false;

  Future<void> _guardarYSincronizar() async {
    if (!yaEstaConectado || caracteristicaGlobal == null) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: const Text('Debes conectarte por Bluetooth primero para enviar las tolerancias.'),
          action: SnackBarAction(
            label: 'Conectar',
            textColor: const Color(0xFFD4AF37),
            onPressed: () async {
              await context.push('/pantallabluetooth');
              if (mounted) setState(() {});
            },
          ),
        ),
      );
      return;
    }

    setState(() => enviando = true);
    bool exito = await ConfigTolerancias.sincronizarConESP32();
    setState(() => enviando = false);

    if (!mounted) return;
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Text(
          exito
              ? 'Configuraciones guardadas en Techeck_V2'
              : 'Error al enviar configuraciones por Bluetooth',
        ),
        backgroundColor: exito ? Colors.green.shade700 : Colors.red.shade700,
      ),
    );
  }

  Future<void> _editarValorManual({
    required String titulo,
    required String unidad,
    required double valorActual,
    required ValueChanged<double> onGuardar,
  }) async {
    final controller = TextEditingController(text: valorActual.toString());
    await showDialog(
      context: context,
      builder: (context) => AlertDialog(
        backgroundColor: const Color(0xFF1E2A38),
        title: Text(titulo, style: const TextStyle(color: Color(0xFFD4AF37), fontSize: 17)),
        content: TextField(
          controller: controller,
          keyboardType: const TextInputType.numberWithOptions(decimal: true),
          autofocus: true,
          decoration: InputDecoration(
            labelText: 'Ingresa valor ($unidad)',
            suffixText: unidad,
          ),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('Cancelar'),
          ),
          ElevatedButton(
            onPressed: () {
              final parsed = double.tryParse(controller.text.replaceAll(',', '.'));
              if (parsed != null && parsed >= 0) {
                onGuardar(parsed);
              }
              Navigator.pop(context);
            },
            child: const Text('Aceptar'),
          ),
        ],
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Configuraciones'),
        actions: [
          IconButton(
            icon: const Icon(Icons.restore),
            tooltip: 'Restaurar valores por defecto',
            onPressed: () {
              setState(() => ConfigTolerancias.restaurarPorDefecto());
              ScaffoldMessenger.of(context).showSnackBar(
                const SnackBar(content: Text('Valores restaurados por defecto')),
              );
            },
          ),
        ],
      ),
      drawer: Drawer(
        child: ListView(
          padding: const EdgeInsets.all(5),
          children: [
            DrawerHeader(
              decoration: BoxDecoration(color: Theme.of(context).colorScheme.primary),
              child: const Text(
                'Menú de navegación',
                style: TextStyle(color: Colors.white, fontSize: 20),
              ),
            ),
            ListTile(
              leading: const Icon(Icons.home),
              title: const Text('Inicio'),
              onTap: () => context.push('/homescreen'),
            ),
            ListTile(
              leading: const Icon(Icons.fitness_center),
              title: const Text('Rutinas'),
              onTap: () => context.push('/rutinas'),
            ),
            ListTile(
              leading: const Icon(Icons.tune),
              title: const Text('Configuraciones'),
              onTap: () => Navigator.pop(context),
            ),
            ListTile(
              leading: const Icon(Icons.bar_chart),
              title: const Text('Estadísticas'),
              onTap: () => context.push('/estadisticas'),
            ),
          ],
        ),
      ),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          Card(
            color: const Color(0xFF1E2A38),
            child: ListTile(
              leading: Icon(
                yaEstaConectado ? Icons.bluetooth_connected : Icons.bluetooth_disabled,
                color: yaEstaConectado ? Colors.greenAccent : Colors.redAccent,
              ),
              title: Text(yaEstaConectado ? 'Conectado a Techeck_V2' : 'Bluetooth desconectado'),
              subtitle: Text(
                yaEstaConectado
                    ? 'Toca cualquier número dorado para escribir un valor exacto'
                    : 'Conecta el sensor para aplicar los cambios',
              ),
              trailing: yaEstaConectado
                  ? null
                  : ElevatedButton(
                      onPressed: () async {
                        await context.push('/pantallabluetooth');
                        if (mounted) setState(() {});
                      },
                      child: const Text('Conectar'),
                    ),
            ),
          ),
          const SizedBox(height: 14),

          _buildSeccion('Sensores'),
          Card(
            margin: const EdgeInsets.only(bottom: 12),
            child: SwitchListTile(
              activeColor: const Color(0xFFD4AF37),
              title: const Text(
                'Exigir sensores auxiliares',
                style: TextStyle(fontSize: 15, fontWeight: FontWeight.bold),
              ),
              subtitle: const Text(
                'Desactívalo si un módulo auxiliar se quedó sin batería y quieres entrenar solo con el sensor principal.',
                style: TextStyle(fontSize: 13, color: Colors.white70),
              ),
              value: ConfigTolerancias.requerirAuxiliares,
              onChanged: (val) => setState(() => ConfigTolerancias.requerirAuxiliares = val),
            ),
          ),

          _buildSeccion('Ángulos y Calibración'),
          _buildControlCard(
            titulo: 'Tolerancia de Desviación (Postura)',
            subtitulo: 'Ángulo máximo de desvío en espalda o codos antes de marcar error (desde 1°).',
            unidad: '°',
            valor: ConfigTolerancias.desviacionPostura,
            sliderMin: 1.0,
            sliderMax: 45.0,
            divisiones: 44,
            decimales: 0,
            onChanged: (val) => setState(() => ConfigTolerancias.desviacionPostura = val),
          ),
          _buildControlCard(
            titulo: 'Margen en Posición Inicial (Reposo)',
            subtitulo: 'Grados de movimiento permitidos en descanso sin disparar una repetición.',
            unidad: '°',
            valor: ConfigTolerancias.holguraReposo,
            sliderMin: 1.0,
            sliderMax: 30.0,
            divisiones: 29,
            decimales: 0,
            onChanged: (val) => setState(() => ConfigTolerancias.holguraReposo = val),
          ),
          _buildControlCard(
            titulo: 'Exceso de Recorrido Permitido',
            subtitulo: 'Grados extra permitidos por encima del límite calibrado antes de marcar "Te pasaste".',
            unidad: '°',
            valor: ConfigTolerancias.excesoRecorrido,
            sliderMin: 1.0,
            sliderMax: 45.0,
            divisiones: 44,
            decimales: 0,
            onChanged: (val) => setState(() => ConfigTolerancias.excesoRecorrido = val),
          ),
          _buildControlCard(
            titulo: 'Amplitud Mínima para Calibración',
            subtitulo: 'Ángulo mínimo que debe tener la repetición de muestra (bájalo a 3°-5° para ejercicios cortos).',
            unidad: '°',
            valor: ConfigTolerancias.amplitudMinCalib,
            sliderMin: 2.0,
            sliderMax: 40.0,
            divisiones: 38,
            decimales: 0,
            onChanged: (val) => setState(() => ConfigTolerancias.amplitudMinCalib = val),
          ),

          const SizedBox(height: 6),
          _buildSeccion('Umbrales de Movimiento'),
          _buildControlCard(
            titulo: 'Sensibilidad de Inicio de Repetición',
            subtitulo: 'Porcentaje de recorrido necesario para detectar que arrancaste (evita falsos disparos).',
            unidad: '%',
            valor: ConfigTolerancias.sensibilidadInicioPct,
            sliderMin: 5.0,
            sliderMax: 80.0,
            divisiones: 75,
            decimales: 0,
            onChanged: (val) => setState(() => ConfigTolerancias.sensibilidadInicioPct = val),
          ),
          _buildControlCard(
            titulo: 'Sensibilidad de Retorno al Inicio',
            subtitulo: 'Qué tan cerca de la posición inicial debes volver para cerrar la repetición.',
            unidad: '%',
            valor: ConfigTolerancias.sensibilidadRetornoPct,
            sliderMin: 5.0,
            sliderMax: 80.0,
            divisiones: 75,
            decimales: 0,
            onChanged: (val) => setState(() => ConfigTolerancias.sensibilidadRetornoPct = val),
          ),
          _buildControlCard(
            titulo: 'Recorrido Mínimo Exigido',
            subtitulo: 'Porcentaje de la amplitud calibrada que debes alcanzar para que cuente como completa.',
            unidad: '%',
            valor: ConfigTolerancias.rangoMinimoPct,
            sliderMin: 10.0,
            sliderMax: 100.0,
            divisiones: 90,
            decimales: 0,
            onChanged: (val) => setState(() => ConfigTolerancias.rangoMinimoPct = val),
          ),

          const SizedBox(height: 6),
          _buildSeccion('Tiempos y Velocidad'),
          _buildControlCard(
            titulo: 'Control de Velocidad (Anti-impulso)',
            subtitulo: 'Exigencia relativa contra repeticiones hechas demasiado rápido.',
            unidad: '%',
            valor: ConfigTolerancias.controlImpulsoPct,
            sliderMin: 5.0,
            sliderMax: 95.0,
            divisiones: 90,
            decimales: 0,
            onChanged: (val) => setState(() => ConfigTolerancias.controlImpulsoPct = val),
          ),
          _buildControlCard(
            titulo: 'Duración Mínima Absoluta de Repetición',
            subtitulo: 'Tiempo mínimo estricto (en milisegundos) por debajo del cual siempre se marca "Muy rápido".',
            unidad: 'ms',
            valor: ConfigTolerancias.duracionMinAbsolutaMs.toDouble(),
            sliderMin: 50.0,
            sliderMax: 2000.0,
            divisiones: 39,
            decimales: 0,
            onChanged: (val) => setState(() => ConfigTolerancias.duracionMinAbsolutaMs = val.round()),
          ),
          _buildControlCard(
            titulo: 'Tiempo Máximo por Repetición',
            subtitulo: 'Multiplicador de tiempo antes de marcar el movimiento como muy lento.',
            unidad: 'x',
            valor: ConfigTolerancias.tiempoMaxRep,
            sliderMin: 1.0,
            sliderMax: 10.0,
            divisiones: 90,
            decimales: 1,
            onChanged: (val) => setState(() => ConfigTolerancias.tiempoMaxRep = val),
          ),
          _buildControlCard(
            titulo: 'Descanso Mínimo entre Repeticiones',
            subtitulo: 'Pausa obligatoria en milisegundos entre una repetición y la siguiente.',
            unidad: 'ms',
            valor: ConfigTolerancias.tiempoMinEntreRepsMs.toDouble(),
            sliderMin: 100.0,
            sliderMax: 3000.0,
            divisiones: 29,
            decimales: 0,
            onChanged: (val) => setState(() => ConfigTolerancias.tiempoMinEntreRepsMs = val.round()),
          ),
          _buildControlCard(
            titulo: 'Tiempo Máximo de Inactividad',
            subtitulo: 'Segundos que puedes quedarte quieto en medio de una repetición.',
            unidad: 's',
            valor: ConfigTolerancias.tiempoInactividad,
            sliderMin: 1.0,
            sliderMax: 15.0,
            divisiones: 28,
            decimales: 1,
            onChanged: (val) => setState(() => ConfigTolerancias.tiempoInactividad = val),
          ),

          const SizedBox(height: 6),
          _buildSeccion('Estabilidad y Confirmación de Errores'),
          _buildControlCard(
            titulo: 'Tolerancia a Temblor / Brusquedad',
            subtitulo: 'Permisividad ante vibraciones o zigzagueos durante la ejecución.',
            unidad: '',
            valor: ConfigTolerancias.toleranciaTemblor,
            sliderMin: 1.0,
            sliderMax: 6.0,
            divisiones: 50,
            decimales: 1,
            onChanged: (val) => setState(() => ConfigTolerancias.toleranciaTemblor = val),
          ),
          _buildControlCard(
            titulo: 'Tolerancia a Retroceso / Tirones',
            subtitulo: 'Velocidad en sentido contrario permitida antes de detectar error de dirección.',
            unidad: '°/s',
            valor: ConfigTolerancias.toleranciaRetroceso,
            sliderMin: 1.0,
            sliderMax: 60.0,
            divisiones: 59,
            decimales: 0,
            onChanged: (val) => setState(() => ConfigTolerancias.toleranciaRetroceso = val),
          ),
          _buildControlCard(
            titulo: 'Confirmación Error de Postura',
            subtitulo: 'Cantidad de muestras seguidas (1 muestra = 0.1s) fuera de postura para marcar error.',
            unidad: ' muestras',
            valor: ConfigTolerancias.muestrasConfirmPostura.toDouble(),
            sliderMin: 1.0,
            sliderMax: 15.0,
            divisiones: 14,
            decimales: 0,
            onChanged: (val) => setState(() => ConfigTolerancias.muestrasConfirmPostura = val.round()),
          ),
          _buildControlCard(
            titulo: 'Confirmación Error de Velocidad/Tirones',
            subtitulo: 'Muestras seguidas (1 muestra = 0.1s) en sentido inverso para marcar error.',
            unidad: ' muestras',
            valor: ConfigTolerancias.muestrasConfirmVel.toDouble(),
            sliderMin: 1.0,
            sliderMax: 15.0,
            divisiones: 14,
            decimales: 0,
            onChanged: (val) => setState(() => ConfigTolerancias.muestrasConfirmVel = val.round()),
          ),
          _buildControlCard(
            titulo: 'Confirmación Exceso de Recorrido',
            subtitulo: 'Muestras seguidas (1 muestra = 0.1s) fuera del límite calibrado para marcar error.',
            unidad: ' muestras',
            valor: ConfigTolerancias.muestrasConfirmRango.toDouble(),
            sliderMin: 1.0,
            sliderMax: 15.0,
            divisiones: 14,
            decimales: 0,
            onChanged: (val) => setState(() => ConfigTolerancias.muestrasConfirmRango = val.round()),
          ),

          const SizedBox(height: 16),
          ElevatedButton.icon(
            onPressed: enviando ? null : _guardarYSincronizar,
            icon: const Icon(Icons.send_to_mobile),
            label: Text(enviando ? 'Enviando...' : 'Aplicar tolerancias por Bluetooth'),
            style: ElevatedButton.styleFrom(
              padding: const EdgeInsets.symmetric(vertical: 15),
            ),
          ),
          const SizedBox(height: 20),
        ],
      ),
    );
  }

  Widget _buildSeccion(String texto) {
    return Padding(
      padding: const EdgeInsets.only(bottom: 8, top: 4, left: 4),
      child: Text(
        texto,
        style: const TextStyle(
          fontSize: 17,
          fontWeight: FontWeight.bold,
          color: Color(0xFFD4AF37),
        ),
      ),
    );
  }

  Widget _buildControlCard({
    required String titulo,
    required String subtitulo,
    required String unidad,
    required double valor,
    required double sliderMin,
    required double sliderMax,
    required int divisiones,
    required int decimales,
    required ValueChanged<double> onChanged,
  }) {
    double valorSlider = valor.clamp(sliderMin, sliderMax);

    return Card(
      margin: const EdgeInsets.only(bottom: 12),
      child: Padding(
        padding: const EdgeInsets.all(14),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                Expanded(
                  child: Text(
                    titulo,
                    style: const TextStyle(fontSize: 15, fontWeight: FontWeight.bold),
                  ),
                ),
                InkWell(
                  onTap: () => _editarValorManual(
                    titulo: titulo,
                    unidad: unidad,
                    valorActual: valor,
                    onGuardar: onChanged,
                  ),
                  child: Container(
                    padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 4),
                    decoration: BoxDecoration(
                      color: const Color(0xFF1E2A38),
                      borderRadius: BorderRadius.circular(8),
                      border: Border.all(color: const Color(0xFFD4AF37)),
                    ),
                    child: Row(
                      mainAxisSize: MainAxisSize.min,
                      children: [
                        Text(
                          '${valor.toStringAsFixed(decimales)}$unidad',
                          style: const TextStyle(
                            fontSize: 15,
                            fontWeight: FontWeight.bold,
                            color: Color(0xFFD4AF37),
                          ),
                        ),
                        const SizedBox(width: 4),
                        const Icon(Icons.edit, size: 14, color: Color(0xFFD4AF37)),
                      ],
                    ),
                  ),
                ),
              ],
            ),
            const SizedBox(height: 6),
            Text(subtitulo, style: const TextStyle(fontSize: 13, color: Colors.white70)),
            Slider(
              value: valorSlider,
              min: sliderMin,
              max: sliderMax,
              divisions: divisiones,
              activeColor: const Color(0xFFD4AF37),
              onChanged: onChanged,
            ),
          ],
        ),
      ),
    );
  }
}
