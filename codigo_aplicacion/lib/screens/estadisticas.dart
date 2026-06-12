import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';

class Estadisticas extends StatefulWidget {
  const Estadisticas({super.key});

  @override
  State<Estadisticas> createState() => _EstadisticasState();
}

class _EstadisticasState extends State<Estadisticas> {
  // Datos simulados
  int ejerciciosCompletados = 12;
  double progresoRutina = 0.65; // 65%
  List<int> caloriasSemana = [300, 450, 500, 200, 600, 700, 400];

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text("Estadísticas")),
      drawer: Drawer(
        child: ListView(
          padding: const EdgeInsets.all(5),
          children: [
            DrawerHeader(
              decoration: BoxDecoration(color: Theme.of(context).colorScheme.primary),
              child: const Text("Menú de navegación",
                  style: TextStyle(color: Colors.white, fontSize: 20)),
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
      body: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          children: [
            // Contador simple
            Card(
              child: ListTile(
                leading: const Icon(Icons.check_circle, color: Colors.green),
                title: const Text("Ejercicios completados"),
                trailing: Text("$ejerciciosCompletados"),
              ),
            ),
            const SizedBox(height: 10),

            // Barra de progreso
            Card(
              child: ListTile(
                leading: const Icon(Icons.fitness_center, color: Colors.blue),
                title: const Text("Progreso de rutina"),
                subtitle: LinearProgressIndicator(value: progresoRutina),
                trailing: Text("${(progresoRutina * 100).toInt()}%"),
              ),
            ),
            const SizedBox(height: 10),

            // Lista de calorías por día
            Expanded(
              child: Card(
                child: Column(
                  children: [
                    const ListTile(
                      leading: Icon(Icons.local_fire_department, color: Colors.red),
                      title: Text("Calorías quemadas esta semana"),
                    ),
                    Expanded(
                      child: ListView.builder(
                        itemCount: caloriasSemana.length,
                        itemBuilder: (context, index) {
                          return ListTile(
                            leading: Text("Día ${index + 1}"),
                            trailing: Text("${caloriasSemana[index]} kcal"),
                          );
                        },
                      ),
                    ),
                  ],
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}
