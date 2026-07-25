//NO BORRAR COMENTARIOS NUNCA
import 'package:codigo_aplicacion/carrito_ejercicios.dart';
import 'dart:async';
import 'package:flutter/material.dart';
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';

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
      feedback = "Conexión mantenida con el ESP32";
      
      // CRUCIAL: Volvemos a escuchar los mensajes del ESP32 al entrar a la pantalla
      _listenToCharacteristic();

      // Mandamos el comando del ejercicio si hay uno listo
      if (ejercicioSeleccionadoId.isNotEmpty) {
        _sendCommand(ejercicioSeleccionadoId);
      }
    }
  }

  @override
  void dispose() {
    scanSub?.cancel();
    notifySub?.cancel(); // Dejamos de actualizar la pantalla para no causar crasheos
    // IMPORTANTE: NO cancelamos 'connSubGlobal' para que el iPhone no corte el Bluetooth
    super.dispose();
  }

  // Separé la escucha en una función para poder llamarla desde initState y connectToDevice
  void _listenToCharacteristic() {
    notifySub?.cancel();
    notifySub = flutterReactiveBleGlobal
        .subscribeToCharacteristic(caracteristicaGlobal!)
        .listen((data) {
      if (!mounted) return; // SEGURO ANTI-CRASHEOS: Si la pantalla ya se cerró, ignora el mensaje
      
      final message = String.fromCharCodes(data);
      setState(() {
        feedback = message;
        status = "Conectado";
      });
    }, onError: (error) {
      if (!mounted) return;
      setState(() {
        feedback = "Error al recibir datos";
      });
    });
  }

  void _startScan() {
    scanSub?.cancel();
    devices = [];
    setState(() {
      status = "Escaneando BLE";
      feedback = "Buscando dispositivos...";
    });

    scanSub = flutterReactiveBleGlobal.scanForDevices(withServices: []).listen(
      (device) {
        if (!devices.any((d) => d.id == device.id)) {
          if (!mounted) return;
          setState(() {
            devices.add(device);
          });
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
      feedback = "Intentando conectar...";
    });

    connSubGlobal?.cancel(); // Corta cualquier intento de conexión anterior
    connSubGlobal = flutterReactiveBleGlobal.connectToDevice(id: device.id).listen(
      (update) async { 
        if (!mounted) return; // Seguro anti-crasheos
        
        setState(() {
          status = "Estado: ${update.connectionState}";
        });

        if (update.connectionState == DeviceConnectionState.connected) {
          yaEstaConectado = true;
          
          caracteristicaGlobal = QualifiedCharacteristic(
            deviceId: device.id,
            serviceId: serviceUuid,
            characteristicId: charUuid,
          );

          // Iniciamos la escucha de los mensajes del ESP32
          _listenToCharacteristic();

          // Si hay un ejercicio, lo mandamos
          if (ejercicioSeleccionadoId.isNotEmpty) {
            _sendCommand(ejercicioSeleccionadoId);
          }

        } else if (update.connectionState == DeviceConnectionState.disconnected) {
          yaEstaConectado = false;
          if (!mounted) return;
          setState(() {
            status = "Desconectado";
            feedback = "Se perdió la conexión con el ESP32";
          });
        }
      },
      onError: (error) {
        if (!mounted) return;
        setState(() {
          status = "Error conexión";
          feedback = "No se pudo conectar";
        });
      },
    );
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
        value: command.codeUnits,
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
      appBar: AppBar(title: const Text('Techeck BLE')),
      body: Padding(
        padding: const EdgeInsets.all(12),
        child: Column(
          children: [
            Text("Estado: $status"),
            const SizedBox(height: 10),
            Card(
              color: yaEstaConectado ? Colors.green.shade100 : Colors.white,
              child: Padding(
                padding: const EdgeInsets.all(12),
                child: Text(
                  feedback, 
                  style: TextStyle(
                    fontSize: 16, 
                    fontWeight: yaEstaConectado ? FontWeight.bold : FontWeight.normal
                  ),
                ),
              ),
            ),
            const SizedBox(height: 10),
            Row(
              children: [
                ElevatedButton(onPressed: _startScan, child: const Text("Escanear")),
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
                  ? const Center(child: Text("No hay dispositivos BLE"))
                  : ListView.builder(
                      itemCount: devices.length,
                      itemBuilder: (context, index) {
                        final device = devices[index];
                        final name = device.name.isEmpty ? "Sin nombre" : device.name;
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
        ),
      ),
    );
  }
}