//NO BORRAR COMENTARIOS NUNCA
import 'package:codigo_aplicacion/carrito_ejercicios.dart';
import 'package:codigo_aplicacion/config_tolerancias.dart';
import 'dart:async';
import 'dart:convert';
import 'package:flutter/material.dart';
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';
import 'package:go_router/go_router.dart';
import 'dart:io';
import 'package:permission_handler/permission_handler.dart';

// ---------------------------------------------------------
// VARIABLES GLOBALES (Mantienen la conexión viva en iOS/Android)
// ---------------------------------------------------------
final flutterReactiveBleGlobal = FlutterReactiveBle();
StreamSubscription<ConnectionStateUpdate>? connSubGlobal;
QualifiedCharacteristic? caracteristicaGlobal;
bool yaEstaConectado = false;

final Uuid serviceUuid = Uuid.parse("11111111-1111-1111-1111-111111111111");
final Uuid charUuid = Uuid.parse("22222222-2222-2222-2222-222222222222");
// ---------------------------------------------------------

class PantallaBluetooth extends StatefulWidget {
  const PantallaBluetooth({Key? key}) : super(key: key);

  @override
  _PantallaBluetoothState createState() => _PantallaBluetoothState();
}

class _PantallaBluetoothState extends State<PantallaBluetooth> {
  String status = "Esperando";
  String feedback = "Pulsa Escanear para buscar el ESP32";
  List<DiscoveredDevice> devices = [];
  StreamSubscription<DiscoveredDevice>? scanSub;
  StreamSubscription<List<int>>? notifySub; // Escucha las respuestas del ESP32

  @override
  void initState() {
    super.initState();

    // Si volvemos a esta pantalla y ya estábamos conectados...
    if (yaEstaConectado && caracteristicaGlobal != null) {
      status = "Conectado";
      feedback = "Conexión activa con el ESP32";

      _listenToCharacteristic();

      // Mandamos el comando del ejercicio si hay uno listo con breve espera
      if (ejercicioSeleccionadoId.isNotEmpty) {
        Future.delayed(const Duration(milliseconds: 300), () {
          if (mounted) _sendCommand(ejercicioSeleccionadoId);
        });
      }
    } else {
      // Inicia el escaneo automáticamente al entrar a la pantalla
      _startScan();
    }
  }

  @override
  void dispose() {
    scanSub?.cancel();
    notifySub?.cancel(); // Dejamos de actualizar la pantalla para no causar crasheos
    super.dispose();
  }

  void _mostrarResumenSerie(
      String bien, String mal, String velMedia, String rangoMax, String detalle) {
    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        backgroundColor: const Color(0xFF1E2A38),
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(16),
          side: const BorderSide(color: Color(0xFFD4AF37), width: 2),
        ),
        title: const Row(
          children: [
            Icon(Icons.emoji_events, color: Color(0xFFD4AF37)),
            SizedBox(width: 8),
            Text('Resumen de la Serie', style: TextStyle(color: Color(0xFFD4AF37))),
          ],
        ),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text('✅ Repeticiones correctas: $bien', style: const TextStyle(fontSize: 16)),
            const SizedBox(height: 6),
            Text('❌ Repeticiones con error: $mal', style: const TextStyle(fontSize: 16)),
            const Divider(color: Colors.white24, height: 20),
            Text(
              '⚡ Velocidad media: $velMedia °/s',
              style: const TextStyle(fontSize: 17, fontWeight: FontWeight.bold, color: Colors.white),
            ),
            const SizedBox(height: 8),
            Text(
              '📐 Rango de movimiento máx.: $rangoMax°',
              style: const TextStyle(fontSize: 17, fontWeight: FontWeight.bold, color: Colors.white),
            ),
            const SizedBox(height: 12),
            Text(
              'Observación: $detalle',
              style: const TextStyle(fontSize: 14, color: Colors.white70),
            ),
          ],
        ),
        actions: [
          ElevatedButton(
            onPressed: () => Navigator.of(context).pop(),
            child: const Text('Cerrar'),
          ),
        ],
      ),
    );
  }

  void _listenToCharacteristic() {
    notifySub?.cancel();
    notifySub = flutterReactiveBleGlobal
        .subscribeToCharacteristic(caracteristicaGlobal!)
        .listen(
          (data) {
            if (!mounted) return;

            final message = utf8.decode(data, allowMalformed: true);

            if (message.startsWith("RESUMEN|")) {
              final partes = message.split("|");
              if (partes.length >= 6) {
                setState(() {
                  feedback = "Serie finalizada (BIEN: ${partes[1]} | MAL: ${partes[2]})";
                  status = "Conectado";
                });
                _mostrarResumenSerie(
                  partes[1],
                  partes[2],
                  partes[3],
                  partes[4],
                  partes[5],
                );
                return;
              }
            }

            setState(() {
              feedback = message;
              status = "Conectado";
            });
          },
          onError: (error) {
            if (!mounted) return;
            setState(() {
              feedback = "Error al recibir datos";
            });
          },
        );
  }

  Future<bool> _requestPermissions() async {
    if (Platform.isAndroid) {
      Map<Permission, PermissionStatus> statuses = await [
        Permission.bluetoothScan,
        Permission.bluetoothConnect,
        Permission.locationWhenInUse,
      ].request();

      bool allGranted = statuses.values.every((s) => s.isGranted);

      if (!allGranted && mounted) {
        setState(() {
          feedback =
              "Se necesitan permisos de Bluetooth y Ubicación para escanear";
        });
      }
      return allGranted;
    }
    return true;
  }

  void _startScan() async {
    bool permisosOk = await _requestPermissions();
    if (!permisosOk) return;
    scanSub?.cancel();
    devices = [];
    setState(() {
      status = "Escaneando BLE";
      feedback = "Buscando Techeck_V2...";
    });

    scanSub = flutterReactiveBleGlobal
        .scanForDevices(withServices: [])
        .listen(
          (device) {
            if (!devices.any((d) => d.id == device.id)) {
              if (!mounted) return;
              setState(() {
                devices.add(device);
              });

              // Conexión automática si detecta el dispositivo Techeck
              if (device.name == "Techeck_V2" ||
                  device.name.contains("Techeck")) {
                _connectToDevice(device);
              }
            }
          },
          onError: (error) {
            if (!mounted) return;
            setState(() {
              status = "Error escaneo";
              feedback = "Error al buscar dispositivos";
            });
          },
        );
  }

  void _connectToDevice(DiscoveredDevice device) {
    scanSub?.cancel();
    setState(() {
      status = "Conectando a ${device.name.isEmpty ? device.id : device.name}";
      feedback = "Estableciendo conexión BLE...";
    });

    connSubGlobal?.cancel();
    connSubGlobal = flutterReactiveBleGlobal
        .connectToDevice(id: device.id)
        .listen(
          (update) async {
            if (update.connectionState == DeviceConnectionState.connected) {
              yaEstaConectado = true;

              // Solicitar MTU mayor para evitar que Android corte mensajes a 20 caracteres
              try {
                await flutterReactiveBleGlobal.requestMtu(
                  deviceId: device.id,
                  mtu: 185,
                );
              } catch (e) {
                debugPrint("No se pudo negociar MTU: $e");
              }

              caracteristicaGlobal = QualifiedCharacteristic(
                deviceId: device.id,
                serviceId: serviceUuid,
                characteristicId: charUuid,
              );

              if (mounted) {
                setState(() {
                  status = "Conectado";
                });
                _listenToCharacteristic();
              }

              // Espera 500 ms para que el servicio GATT se estabilice antes de enviar
              await Future.delayed(const Duration(milliseconds: 500));

              // Sincroniza las tolerancias configuradas en la app
              await ConfigTolerancias.sincronizarConESP32();
              await Future.delayed(const Duration(milliseconds: 200));

              if (ejercicioSeleccionadoId.isNotEmpty && mounted) {
                _sendCommand(ejercicioSeleccionadoId);
              }
            } else if (update.connectionState ==
                DeviceConnectionState.disconnected) {
              yaEstaConectado = false;
              caracteristicaGlobal = null;
              if (mounted) {
                setState(() {
                  status = "Desconectado";
                  feedback = "Se perdió la conexión con el ESP32";
                });
              }
            } else {
              if (mounted) {
                setState(() {
                  status = "Estado: ${update.connectionState}";
                });
              }
            }
          },
          onError: (error) {
            yaEstaConectado = false;
            caracteristicaGlobal = null;
            if (mounted) {
              setState(() {
                status = "Error conexión";
                feedback = "No se pudo conectar";
              });
            }
          },
        );
  }

  void _disconnect() {
    connSubGlobal?.cancel();
    notifySub?.cancel();
    setState(() {
      yaEstaConectado = false;
      caracteristicaGlobal = null;
      status = "Desconectado";
      feedback = "Dispositivo desconectado";
    });
  }

  Future<void> _sendCommand(String command) async {
    if (caracteristicaGlobal == null || !yaEstaConectado) {
      if (!mounted) return;
      setState(() {
        feedback = "No hay conexión BLE activa";
      });
      return;
    }

    try {
      await flutterReactiveBleGlobal.writeCharacteristicWithResponse(
        caracteristicaGlobal!,
        value: utf8.encode(command),
      );
      if (!mounted) return;
      setState(() {
        feedback = "¡Haciendo ejercicio! Comando enviado: $command";
      });
    } catch (error) {
      if (!mounted) return;
      setState(() {
        feedback = "Error al enviar el comando al ESP32";
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Techeck BLE'),
        actions: [
          IconButton(
            icon: const Icon(Icons.info_outline),
            tooltip: 'Guía de LEDs',
            onPressed: () => context.push('/manual_leds'),
          ),
        ],
      ),
      body: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          children: [
            Text(
              "Estado: $status",
              style: const TextStyle(fontSize: 16, fontWeight: FontWeight.bold),
            ),
            const SizedBox(height: 12),

            // CUANDO ESTÁ CONECTADO: Se ocultan los dispositivos y se muestra el feedback en grande
            if (yaEstaConectado) ...[
              Expanded(
                child: Center(
                  child: Card(
                    color: const Color(0xFF1E2A38),
                    elevation: 6,
                    shape: RoundedRectangleBorder(
                      borderRadius: BorderRadius.circular(16),
                      side: const BorderSide(
                        color: Color(0xFFD4AF37),
                        width: 2,
                      ),
                    ),
                    child: Padding(
                      padding: const EdgeInsets.all(24),
                      child: Column(
                        mainAxisSize: MainAxisSize.min,
                        children: [
                          const Icon(
                            Icons.bluetooth_connected,
                            color: Color(0xFFD4AF37),
                            size: 50,
                          ),
                          const SizedBox(height: 16),
                          const Text(
                            "Feedback del ESP32",
                            style: TextStyle(
                              fontSize: 18,
                              fontWeight: FontWeight.bold,
                              color: Color(0xFFD4AF37),
                            ),
                          ),
                          const SizedBox(height: 16),
                          Text(
                            feedback,
                            textAlign: TextAlign.center,
                            style: const TextStyle(
                              fontSize: 22,
                              fontWeight: FontWeight.bold,
                              color: Colors.white,
                            ),
                          ),
                        ],
                      ),
                    ),
                  ),
                ),
              ),
              const SizedBox(height: 16),
              ElevatedButton.icon(
                onPressed: _disconnect,
                icon: const Icon(Icons.bluetooth_disabled),
                label: const Text("Desconectar"),
                style: ElevatedButton.styleFrom(
                  backgroundColor: Colors.red.shade700,
                  foregroundColor: Colors.white,
                  padding: const EdgeInsets.symmetric(
                    horizontal: 24,
                    vertical: 12,
                  ),
                ),
              ),
            ] else ...[
              // CUANDO NO ESTÁ CONECTADO: Muestra la búsqueda y lista de dispositivos
              Card(
                color: const Color(0xFF1E2A38),
                child: Padding(
                  padding: const EdgeInsets.all(12),
                  child: Text(
                    feedback,
                    style: const TextStyle(fontSize: 15, color: Colors.white),
                  ),
                ),
              ),
              const SizedBox(height: 10),
              Row(
                children: [
                  ElevatedButton(
                    onPressed: _startScan,
                    child: const Text("Escanear"),
                  ),
                  const SizedBox(width: 10),
                  ElevatedButton(
                    onPressed: () {
                      scanSub?.cancel();
                      setState(() {
                        status = "Escaneo detenido";
                        feedback = "Puedes volver a escanear";
                      });
                    },
                    child: const Text("Detener"),
                  ),
                ],
              ),
              const SizedBox(height: 10),
              Expanded(
                child: devices.isEmpty
                    ? const Center(child: Text("Buscando dispositivos BLE..."))
                    : ListView.builder(
                        itemCount: devices.length,
                        itemBuilder: (context, index) {
                          final device = devices[index];
                          final name = device.name.isEmpty
                              ? "Sin nombre"
                              : device.name;
                          return Card(
                            child: ListTile(
                              title: Text(name),
                              subtitle: Text(device.id),
                              trailing: ElevatedButton(
                                onPressed: () => _connectToDevice(device),
                                child: const Text("Conectar"),
                              ),
                            ),
                          );
                        },
                      ),
              ),
            ],
          ],
        ),
      ),
    );
  }
}
