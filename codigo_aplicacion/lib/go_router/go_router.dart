import 'package:codigo_aplicacion/screens/pantalla_bluetooth.dart';
import 'package:codigo_aplicacion/screens/infousers.dart';
import 'package:go_router/go_router.dart';
import 'package:codigo_aplicacion/screens/login.dart';
import 'package:codigo_aplicacion/screens/homescreen.dart';
import 'package:codigo_aplicacion/screens/rutinas.dart';
import 'package:codigo_aplicacion/screens/estadisticas.dart';
import 'package:codigo_aplicacion/screens/ejercicios.dart';



final appRouter = GoRouter(
  initialLocation: '/homescreen',
  routes: [
    GoRoute(
      path: '/login',
      builder: (context, state) => const Login(),
    ),
    GoRoute(
      path: '/homescreen',
      builder: (context, state) => const HomeScreen(),
    ),
    GoRoute(
      path: '/rutinas',
      builder: (context, state) => const Rutinas(),
    ),
   
    GoRoute(
      path: '/ejercicios',
      builder: (context, state) => const Ejercicios(),
    ),
    GoRoute(
      path: '/infousers',
      builder: (context, state) => const Infousers(),
    ),
     GoRoute(
      path: '/pantallabluetooth',
      builder: (context, state) =>  PantallaBluetooth(),
    ),
  ],
);
