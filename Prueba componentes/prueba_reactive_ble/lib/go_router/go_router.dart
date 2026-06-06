import 'package:go_router/go_router.dart';
import 'package:prueba_reactive_ble/pantalla_bluetooth.dart';

final appRouter = GoRouter(
  initialLocation: '/pantallabluetooth',
  routes: [
    GoRoute(
      path: '/pantallabluetooth',
      builder: (context, state) => const PantallaBluetooth(),
    ),
  ],
);
