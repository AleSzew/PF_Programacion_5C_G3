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
              ? 'Todas las tolerancias guardadas en Techeck_V2'
              : 'Error al enviar tolerancias por Bluetooth',
        ),
        backgroundColor: exito ? Colors.green.shade700 : Colors.red.shade700,
      ),
    );
  }

  // Permite escribir cualquier valor numérico exacto sin tope mínimo ni máximo
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
              if (parsed != null && parsed > 0) {
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
                    ? 'Toca un número dorado para escribir cualquier valor exacto'
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
          const SizedBox(height: 16),

          _buildSeccion('Ángulos y Postura'),
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
            subtitulo: 'Grados de movimiento permitidos en descanso sin iniciar una repetición.',
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
            subtitulo: 'Grados extra que puedes pasarte del límite grabado antes de marcar "Te pasaste".',
            unidad: '°',
            valor: ConfigTolerancias.excesoRecorrido,
            sliderMin: 1.0,
            sliderMax: 45.0,
            divisiones: 44,
            decimales: 0,
            onChanged: (val) => setState(() => ConfigTolerancias.excesoRecorrido = val),
          ),

          const SizedBox(height: 8),
          _buildSeccion('Recorrido y Velocidad'),
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
          _buildControlCard(
            titulo: 'Control de Velocidad (Anti-impulso)',
            subtitulo: 'Exigencia contra repeticiones hechas demasiado rápido.',
            unidad: '%',
            valor: ConfigTolerancias.controlImpulsoPct,
            sliderMin: 5.0,
            sliderMax: 95.0,
            divisiones: 90,
            decimales: 0,
            onChanged: (val) => setState(() => ConfigTolerancias.controlImpulsoPct = val),
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

          const SizedBox(height: 8),
          _buildSeccion('Estabilidad e Inactividad'),
          _buildControlCard(
            titulo: 'Tolerancia a Temblor / Brusquedad',
            subtitulo: 'Permisividad ante vibraciones o zigzagueos durante la ejecución (mayor = más permisivo).',
            unidad: '',
            valor: ConfigTolerancias.toleranciaTemblor,
            sliderMin: 1.0,
            sliderMax: 6.0,
            divisiones: 50,
            decimales: 1,
            onChanged: (val) => setState(() => ConfigTolerancias.toleranciaTemblor = val),
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
          _buildControlCard(
            titulo: 'Tolerancia a Retroceso / Tirones',
            subtitulo: 'Velocidad en sentido contrario permitida antes de marcar error de dirección.',
            unidad: '°/s',
            valor: ConfigTolerancias.toleranciaRetroceso,
            sliderMin: 1.0,
            sliderMax: 60.0,
            divisiones: 59,
            decimales: 0,
            onChanged: (val) => setState(() => ConfigTolerancias.toleranciaRetroceso = val),
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
    // Asegura que el slider no falle si el usuario escribió un número fuera de su rango visual
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
                            fontSize: 16,
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
