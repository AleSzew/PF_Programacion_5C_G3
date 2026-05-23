import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';
import 'package:codigo_aplicacion/listaejercicios.dart';

class Videoejercicios extends StatelessWidget {
  const Videoejercicios({super.key});

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Ejercicios'),
      ),
      body: const ListaEjercicios(),
    );
  }
}
