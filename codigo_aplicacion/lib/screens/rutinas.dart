import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';
import 'package:url_launcher/url_launcher.dart';
import 'package:codigo_aplicacion/carrito_ejercicios.dart';

class Rutinas extends StatefulWidget {
  const Rutinas({super.key});

  @override
  State<Rutinas> createState() => _RutinasState();
}

class _RutinasState extends State<Rutinas> {
  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Mi rutina'),
        actions: [
          Padding(
            padding: const EdgeInsets.only(right: 12),
            child: ElevatedButton(
              onPressed: () async {
                await context.push('/ejercicios');
                setState(() {});
              },
              style: ElevatedButton.styleFrom(
                backgroundColor: Colors.white,
                foregroundColor: const Color(0xFF1E2A38),
              ),
              child: const Text('+'),
            ),
          ),
        ],
      ),
      drawer: Drawer(
        child: ListView(
          padding: EdgeInsets.all(5),
          children: [
            const DrawerHeader(
              decoration: BoxDecoration(color: Colors.blue),
              child: Text(
                'Menú de navegación',
                style: TextStyle(color: Colors.white, fontSize: 20),
              ),
            ),
            ListTile(
              leading: const Icon(Icons.home),
              title: const Text('Inicio'),
              onTap: () {
                context.push('/homescreen');
              },
            ),
            ListTile(
              leading: const Icon(Icons.fitness_center),
              title: const Text('Rutinas'),
              onTap: () {
                context.push('/rutinas');
              },
            ),
            ListTile(
              leading: const Icon(Icons.bar_chart),
              title: const Text('Estadísticas'),
              onTap: () {
                context.push('/estadisticas');
              },
            ),
            ListTile(
              leading: const Icon(Icons.settings),
              title: const Text('Configuración'),
              onTap: () {
                context.push('/configuracion');
              },
            ),
          ],
        ),
      ),
      body: carrito.length == 0
          ? const Center(
              child: Text('No tienes ejercicios en tu rutina'),
            )
          : ListView.builder(
              padding: const EdgeInsets.all(12),
              itemCount: carrito.length,
              itemBuilder: (BuildContext context, int indice) {
                Map<String, String> ejercicio = carrito[indice];
                String nombre = ejercicio['nombre']!;
                String video = ejercicio['video']!;

                return Card(
                  margin: const EdgeInsets.only(bottom: 10),
                  child: Padding(
                    padding: const EdgeInsets.all(12),
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text(
                          nombre,
                          style: const TextStyle(
                            fontSize: 17,
                            fontWeight: FontWeight.bold,
                          ),
                        ),
                        const SizedBox(height: 10),
                        Row(
                          children: [
                            Expanded(
                              child: ElevatedButton(
                                onPressed: () async {
                                  Uri link = Uri.parse(video);
                                  if (await canLaunchUrl(link)) {
                                    await launchUrl(
                                      link,
                                      mode: LaunchMode.externalApplication,
                                    );
                                  }
                                },
                                child: const Text('Video'),
                              ),
                            ),
                            const SizedBox(width: 8),
                            Expanded(
                              child: ElevatedButton(
                                onPressed: () {
                                  carrito.removeAt(indice);
                                  setState(() {});
                                },
                                child: const Text('-'),
                              ),
                            ),
                            const SizedBox(width: 8),
                            Expanded(
                              child: ElevatedButton(
                                onPressed: () {
                                  context.push('/pantallabluetooth');
                                },
                                child: const Text('Hacer'),
                              ),
                            ),
                          ],
                        ),
                      ],
                    ),
                  ),
                );
              },
            ),
    );
  }
}
