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
              ? 'Tolerancias enviadas y aplicadas en Techeck_V2'
              : 'Error al enviar tolerancias por Bluetooth',
        ),
        backgroundColor: exito ? Colors.green.shade700 : Colors.red.shade700,
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Configuraciones'),
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
                    ? 'Listo para actualizar tolerancias en el sensor'
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
          _buildSliderCard(
            titulo: 'Tolerancia de Postura (Espalda / Codos)',
            subtitulo: 'Ángulo máximo que puedes desviarte antes de que el sensor marque error de postura.',
            etiquetaValor: '${ConfigTolerancias.desviacionPostura.toStringAsFixed(0)}°',
            valor: ConfigTolerancias.desviacionPostura,
            min: 5.0,
            max: 25.0,
            divisiones: 20,
            onChanged: (val) => setState(() => ConfigTolerancias.desviacionPostura = val),
          ),
          _buildSliderCard(
            titulo: 'Control de Velocidad (Anti-impulso)',
            subtitulo: 'Qué tan estricto es el sistema si levantas el peso demasiado rápido.',
            etiquetaValor: '${ConfigTolerancias.controlImpulso.toStringAsFixed(0)}%',
            valor: ConfigTolerancias.controlImpulso,
            min: 20.0,
            max: 80.0,
            divisiones: 12,
            onChanged: (val) => setState(() => ConfigTolerancias.controlImpulso = val),
          ),
          _buildSliderCard(
            titulo: 'Tiempo Máximo por Repetición',
            subtitulo: 'Cuánto tiempo puedes tardar en una repetición antes de marcarla como muy lenta.',
            etiquetaValor: '${ConfigTolerancias.tiempoMaxRep.toStringAsFixed(1)}x',
            valor: ConfigTolerancias.tiempoMaxRep,
            min: 2.0,
            max: 5.0,
            divisiones: 6,
            onChanged: (val) => setState(() => ConfigTolerancias.tiempoMaxRep = val),
          ),
          _buildSliderCard(
            titulo: 'Margen en Posición Inicial',
            subtitulo: 'Movimiento permitido mientras descansas entre repeticiones sin activar una nueva.',
            etiquetaValor: '${ConfigTolerancias.holguraReposo.toStringAsFixed(0)}°',
            valor: ConfigTolerancias.holguraReposo,
            min: 3.0,
            max: 15.0,
            divisiones: 12,
            onChanged: (val) => setState(() => ConfigTolerancias.holguraReposo = val),
          ),
          const SizedBox(height: 12),
          ElevatedButton.icon(
            onPressed: enviando ? null : _guardarYSincronizar,
            icon: const Icon(Icons.send_to_mobile),
            label: Text(enviando ? 'Enviando...' : 'Aplicar tolerancias por Bluetooth'),
            style: ElevatedButton.styleFrom(
              padding: const EdgeInsets.symmetric(vertical: 14),
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildSliderCard({
    required String titulo,
    required String subtitulo,
    required String etiquetaValor,
    required double valor,
    required double min,
    required double max,
    required int divisiones,
    required ValueChanged<double> onChanged,
  }) {
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
                    style: const TextStyle(fontSize: 16, fontWeight: FontWeight.bold),
                  ),
                ),
                Text(
                  etiquetaValor,
                  style: const TextStyle(
                    fontSize: 18,
                    fontWeight: FontWeight.bold,
                    color: Color(0xFFD4AF37),
                  ),
                ),
              ],
            ),
            const SizedBox(height: 4),
            Text(subtitulo, style: const TextStyle(fontSize: 13, color: Colors.white70)),
            Slider(
              value: valor,
              min: min,
              max: max,
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
