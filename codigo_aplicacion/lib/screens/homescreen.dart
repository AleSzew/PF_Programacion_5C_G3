import 'package:codigo_aplicacion/entities/users.dart';
import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';

class HomeScreen extends StatefulWidget {
  const HomeScreen({super.key});

  @override
  State<HomeScreen> createState() => _HomeScreenState();
}

class _HomeScreenState extends State<HomeScreen> {
  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        backgroundColor: Theme.of(context).colorScheme.primary,
        title: const Text("Bienvenido a Techeck"),
      ),
      drawer: Drawer(
        child: ListView(
          padding: const EdgeInsets.all(5),
          children: [
             DrawerHeader(
              decoration: BoxDecoration(color: Theme.of(context).colorScheme.primary),
              child: Text(
                "Menú de navegación",
                style: TextStyle(color: Colors.white, fontSize: 20),
              ),
            ),
            ListTile(
              leading: const Icon(Icons.home),
              title: const Text("Inicio"),
              onTap: () => context.push('/homescreen'),
            ),
            ListTile(
              leading: const Icon(Icons.fitness_center),
              title: const Text("Rutinas"),
              onTap: () => context.push('/rutinas'),
            ),
            ListTile(
              leading: const Icon(Icons.bar_chart),
              title: const Text("Estadísticas"),
              onTap: () => context.push('/estadisticas'),
            ),
            ListTile(
              leading: const Icon(Icons.settings),
              title: const Text("Configuración"),
              onTap: () => context.push('/configuracion'),
            ),
          ],
        ),
      ),
      body: Column(
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          Card(
            child: ListTile(
              title: Text(
                "Nombre: ${miUsuario.name},   Edad: ${miUsuario.age}",
              ),
              subtitle: Text("Peso: ${miUsuario.weight} kg"),
            ),
          ),
          Center(
            child: ElevatedButton(
              onPressed: () => context.push('/rutinas'),
              child: const Text(
                "Nueva Rutina",
              ),
            ),
          ),
          ElevatedButton(
            onPressed: () async {
              await context.push('/infousers');
              setState(() {});
            },
            child: const Text(
              "Por favor complete su peso y valores aqui",
            ),
          ),
        ],
      ),
    );
  }
}
