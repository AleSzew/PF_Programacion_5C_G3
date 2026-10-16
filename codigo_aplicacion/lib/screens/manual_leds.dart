import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';

class ManualLeds extends StatelessWidget {
  const ManualLeds({super.key});

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Guía de LEDs ESP32'),
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
              leading: const Icon(Icons.bar_chart),
              title: const Text('Estadísticas'),
              onTap: () => context.push('/estadisticas'),
            ),
            ListTile(
              leading: const Icon(Icons.lightbulb),
              title: const Text('Guía de LEDs'),
              onTap: () => context.push('/manual_leds'),
            ),
          ],
        ),
      ),
      body: ListView(
        padding: const EdgeInsets.all(16.0),
        children: [
          const Text(
            '¿Qué significan los LEDs en el dispositivo?',
            style: TextStyle(
              fontSize: 20,
              fontWeight: FontWeight.bold,
              color: Color(0xFFD4AF37),
            ),
            textAlign: TextAlign.center,
          ),
          const SizedBox(height: 10),
          const Text(
            'El módulo principal de Techeck cuenta con luces LED que te indican en tiempo real el estado de tu entrenamiento:',
            style: TextStyle(fontSize: 15, color: Colors.white70),
            textAlign: TextAlign.center,
          ),
          const SizedBox(height: 20),

          // Tarjeta VERDE
          _buildLedCard(
            color: Colors.greenAccent.shade400,
            titulo: 'VERDE',
            subtitulo: 'Repetición Correcta',
            descripcion:
                'Indica que la repetición que hiciste estuvo bien ejecutada, respetando la postura y el tiempo adecuado. También se ilumina cuando el sistema está listo para comenzar.',
            icono: Icons.check_circle_outline,
          ),

          const SizedBox(height: 16),

          // Tarjeta ROJA
          _buildLedCard(
            color: Colors.redAccent.shade400,
            titulo: 'ROJO',
            subtitulo: 'Repetición con Error',
            descripcion:
                'Indica que la repetición tuvo un error: te desviaste de la postura correcta, hiciste el movimiento demasiado rápido o excediste el tiempo límite.',
            icono: Icons.cancel_outlined,
          ),

          const SizedBox(height: 16),

          // Tarjeta AMARILLA
          _buildLedCard(
            color: Colors.amberAccent.shade400,
            titulo: 'AMARILLO',
            subtitulo: 'Calibrando o En Movimiento',
            descripcion:
                'Indica que el sensor se está calibrando al inicio, o que estás en medio de una repetición / en pausa entre repeticiones preparando la siguiente.',
            icono: Icons.hourglass_top,
          ),
        ],
      ),
    );
  }

  Widget _buildLedCard({
    required Color color,
    required String titulo,
    required String subtitulo,
    required String descripcion,
    required IconData icono,
  }) {
    return Card(
      color: const Color(0xFF1E2A38),
      elevation: 4,
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(14),
        side: BorderSide(color: color.withOpacity(0.6), width: 1.5),
      ),
      child: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Row(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Container(
              padding: const EdgeInsets.all(10),
              decoration: BoxDecoration(
                shape: BoxShape.circle,
                color: color.withOpacity(0.15),
                border: Border.all(color: color, width: 2),
              ),
              child: Icon(icono, color: color, size: 30),
            ),
            const SizedBox(width: 14),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    titulo,
                    style: TextStyle(
                      fontSize: 18,
                      fontWeight: FontWeight.bold,
                      color: color,
                    ),
                  ),
                  Text(
                    subtitulo,
                    style: const TextStyle(
                      fontSize: 15,
                      fontWeight: FontWeight.w600,
                      color: Colors.white,
                    ),
                  ),
                  const SizedBox(height: 6),
                  Text(
                    descripcion,
                    style: const TextStyle(
                      fontSize: 13,
                      color: Colors.white70,
                      height: 1.3,
                    ),
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }
}