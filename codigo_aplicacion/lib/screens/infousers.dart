import 'package:codigo_aplicacion/entities/users.dart';
import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';

class Infousers extends StatefulWidget {
  const Infousers({super.key});

  @override
  State<Infousers> createState() => _InfousersState();
}

class _InfousersState extends State<Infousers> {
  String title = "Datos del usuario";
  final TextEditingController ageController = TextEditingController();
  final TextEditingController nameController = TextEditingController();
  final TextEditingController weightController = TextEditingController();

  @override
  void initState() {
    super.initState();
    nameController.text = miUsuario.name;
    ageController.text = miUsuario.age;
    weightController.text = miUsuario.weight;
  }

  void _mostrarError(String mensaje) {
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(content: Text(mensaje)),
    );
  }

  void _guardar() {
    final nombre = nameController.text;
    if (nombre.isEmpty) {
      _mostrarError("El nombre debe ser un texto (no puede estar vacío)");
      return;
    }

    final edad = int.tryParse(ageController.text);
    if (edad == null) {
      _mostrarError("La edad debe ser un número entero");
      return;
    }

    final pesoTexto = weightController.text.replaceAll(',', '.');
    final peso = double.tryParse(pesoTexto);
    if (peso == null) {
      _mostrarError("El peso debe ser un número");
      return;
    }

    miUsuario.name = nombre;
    miUsuario.age = edad.toString();
    miUsuario.weight = peso.toString();

    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Text(
          "Guardado: ${miUsuario.name}, edad ${miUsuario.age}, peso ${miUsuario.weight} kg",
        ),
      ),
    );

    context.pop();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: Text(title)),
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
      body: Padding(
        padding: const EdgeInsets.all(20),
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            TextField(
              controller: nameController,
              decoration: const InputDecoration(labelText: "Nombre"),
            ),
            TextField(
              controller: ageController,
              keyboardType: TextInputType.number,
              decoration: const InputDecoration(labelText: "Edad"),
            ),
            TextField(
              controller: weightController,
              keyboardType: const TextInputType.numberWithOptions(decimal: true),
              decoration: const InputDecoration(labelText: "Peso (kg)"),
            ),
            const SizedBox(height: 20),
            ElevatedButton(
              onPressed: _guardar,
              child: const Text("Guardar"),
            ),
          ],
        ),
      ),
    );
  }
}
