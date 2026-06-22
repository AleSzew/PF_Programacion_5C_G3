//NO BORRAR COMENTARIOS NUNCA
import 'package:codigo_aplicacion/carrito_ejercicios.dart';
import 'dart:async';
import 'package:flutter/material.dart';
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';

class PantallaBluetooth extends StatefulWidget {

  const PantallaBluetooth({Key? key}) : super(key: key);

  @override
  _PantallaBluetoothState createState() => _PantallaBluetoothState();
}

class _PantallaBluetoothState extends State<PantallaBluetooth> {
  QualifiedCharacteristic? writeCharacteristic;
  final flutterReactiveBle = FlutterReactiveBle();
  String status = "Esperando";
  String feedback = "Pulsa Escanear para buscar el ESP32";
  List<DiscoveredDevice> devices = [];
  StreamSubscription<DiscoveredDevice>? scanSub;
  StreamSubscription<ConnectionStateUpdate>? connSub;
  StreamSubscription<List<int>>? notifySub;

  final Uuid serviceUuid = Uuid.parse("11111111-1111-1111-1111-111111111111");
  final Uuid charUuid = Uuid.parse("22222222-2222-2222-2222-222222222222");

  @override
  void dispose() {
    scanSub?.cancel();
    connSub?.cancel();
    notifySub?.cancel();
    super.dispose();
  }

  void _startScan() {
    scanSub?.cancel();
    devices = [];
    setState(() {
      status = "Escaneando BLE";
      feedback = "Buscando dispositivos...";
    });

    scanSub = flutterReactiveBle.scanForDevices(withServices: []).listen( //escanea todos los dispositivos
      (device) { //cuando haya un dispotivio hace lo sigiuiente, anade a una lista
        if (!devices.any((d) => d.id == device.id)) {
          devices.add(device);
        }
        setState(() {});
      },
      onError: (error) {
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

    connSub = flutterReactiveBle.connectToDevice(id: device.id).listen(
      (update) async { 
        setState(() {
          status = "Estado: ${update.connectionState}";
        });

        if (update.connectionState == DeviceConnectionState.connected) { //si se conecta hace lo siguiente
          writeCharacteristic = QualifiedCharacteristic(
            deviceId: device.id,
            serviceId: serviceUuid,
            characteristicId: charUuid,
          );
          final characteristic = QualifiedCharacteristic(
            deviceId: device.id,
            serviceId: serviceUuid,
            characteristicId: charUuid,
          );
          if (ejercicioSeleccionadoId.isNotEmpty) {
            _sendCommand(ejercicioSeleccionadoId);
          }
          notifySub = flutterReactiveBle
              .subscribeToCharacteristic(characteristic) //avisame si hay cambios en la caracteristica
              .listen((data) {
            final message = String.fromCharCodes(data);
            setState(() {
              feedback = message;
              status = "Conectado";
            });
          }, onError: (error) {
            setState(() {
              status = "Error notificación";
              feedback = "No se pudo recibir datos";
            });
          });
        }
      },
      onError: (error) {
        setState(() {
          status = "Error conexión";
          feedback = "No se pudo conectar";
        });
      },
    );
  }
    Future<void> _sendCommand(String command) async {
    if (writeCharacteristic == null) {
      setState(() {
        feedback = "No hay conexión BLE activa";
      });
      return;
    }
    try {
      await flutterReactiveBle.writeCharacteristicWithResponse(
        writeCharacteristic!,
        value: command.codeUnits,
      );
      setState(() {
        feedback = "Enviado comando: $command";
      });
    } catch (error) {
      setState(() {
        feedback = "Error al enviar comando";
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
              child: Padding(
                padding: const EdgeInsets.all(12),
                child: Text(feedback),
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