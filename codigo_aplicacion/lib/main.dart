import 'package:flutter/material.dart';
import 'package:codigo_aplicacion/go_router/go_router.dart';

void main() {
  runApp(const MainApp());
}

class MainApp extends StatelessWidget {
  const MainApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp.router(
      routerConfig: appRouter,
      theme: ThemeData(
        brightness: Brightness.dark, // base oscura
        scaffoldBackgroundColor: const Color(0xFF000000), // negro puro
        primaryColor: const Color(0xFFD4AF37), // dorado clásico
        colorScheme: const ColorScheme.dark(
          primary: Color(0xFFD4AF37), // dorado
          secondary: Color(0xFF1E2A38), // negro azulado para contraste
          surface: Color(0xFF121212), // gris oscuro para tarjetas
          onPrimary: Colors.black, // texto sobre dorado
          onSurface: Colors.white, // texto sobre fondo oscuro
        ),
        appBarTheme: const AppBarTheme(
          backgroundColor: Color(0xFF000000), // negro
          foregroundColor: Color(0xFFD4AF37), // dorado en títulos
          elevation: 2,
        ),
        textTheme: const TextTheme(
          headlineLarge: TextStyle(
            fontSize: 28,
            fontWeight: FontWeight.bold,
            color: Color(0xFFD4AF37), // dorado en títulos grandes
          ),
          bodyLarge: TextStyle(
            fontSize: 16,
            color: Colors.white, // texto normal blanco
          ),
          labelLarge: TextStyle(
            fontSize: 20,
            fontWeight: FontWeight.w600,
            color: Color(0xFFD4AF37), // dorado en labels/botones
          ),
        ),
        elevatedButtonTheme: ElevatedButtonThemeData(
          style: ElevatedButton.styleFrom(
            backgroundColor: const Color(0xFFD4AF37), // dorado
            foregroundColor: Colors.black, // texto negro
            shape: RoundedRectangleBorder(
              borderRadius: BorderRadius.circular(8),
            ),
          ),
        ),
      ),
    );
  }
}
