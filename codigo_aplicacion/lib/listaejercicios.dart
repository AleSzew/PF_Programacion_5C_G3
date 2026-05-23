import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';
import 'package:url_launcher/url_launcher.dart';
import 'package:codigo_aplicacion/carrito_ejercicios.dart';

class ListaEjercicios extends StatefulWidget {
  const ListaEjercicios({super.key});

  @override
  State<ListaEjercicios> createState() => _ListaEjerciciosState();
}

class _ListaEjerciciosState extends State<ListaEjercicios> {
  @override
  Widget build(BuildContext context) {
    return ListView.builder(
      padding: const EdgeInsets.all(12),
      itemCount: listaEjercicios.length,
      itemBuilder: (BuildContext context, int indice) {
        Map<String, String> ejercicio = listaEjercicios[indice];
        String nombre = ejercicio['nombre']!;
        String video = ejercicio['video']!;
        String id = ejercicio['id']!;

        bool enCarrito = false;
        for (int i = 0; i < carrito.length; i++) {
          if (carrito[i]['id'] == id) {
            enCarrito = true;
          }
        }

        return Card(
          margin: const EdgeInsets.only(bottom: 12),
          child: Padding(
            padding: const EdgeInsets.all(12),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  nombre,
                  style: const TextStyle(
                    fontSize: 18,
                    fontWeight: FontWeight.bold,
                  ),
                ),
                if (enCarrito)
                  const Padding(
                    padding: EdgeInsets.only(top: 4),
                    child: Text('En tu rutina'),
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
                    const SizedBox(width: 6),
                    Expanded(
                      child: ElevatedButton(
                        onPressed: () {
                          for (int i = 0; i < carrito.length; i++) {
                            if (carrito[i]['id'] == id) {
                              return;
                            }
                          }
                          carrito.add(ejercicio);
                          setState(() {});
                        },
                        child: const Text('+'),
                      ),
                    ),
                    const SizedBox(width: 6),
                    Expanded(
                      child: ElevatedButton(
                        onPressed: () {
                          for (int i = carrito.length - 1; i >= 0; i--) {
                            if (carrito[i]['id'] == id) {
                              carrito.removeAt(i);
                            }
                          }
                          setState(() {});
                        },
                        child: const Text('-'),
                      ),
                    ),
                    const SizedBox(width: 6),
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
    );
  }
}
