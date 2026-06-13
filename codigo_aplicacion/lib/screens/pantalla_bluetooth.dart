=import 'dart:async';
import 'package:flutter/material.dart';
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';

class PantallaBluetooth extends StatefulWidget {
  final String? ejercicioId;
  const PantallaBluetooth({Key? key, this.ejercicioId}) : super(key: key);

  @override
  _PantallaBluetoothState createState() => _PantallaBluetoothState();
}

class _PantallaBluetoothState extends State<PantallaBluetooth> {
  final flutterReactiveBle = FlutterReactiveBle();
  String feedback = "Esperando datos...";
  String status = "Idle";
  String lastLog = "";
  DiscoveredDevice? esp32Device;
  late QualifiedCharacteristic characteristic;
  StreamSubscription<DiscoveredDevice>? scanSub;
  StreamSubscription<ConnectionStateUpdate>? connSub;
  StreamSubscription<List<int>>? notifySub;

  final Uuid serviceUuid = Uuid.parse("11111111-1111-1111-1111-111111111111");
  final Uuid charUuid = Uuid.parse("22222222-2222-2222-2222-222222222222");

  @override
  void initState() {
    super.initState();
    _startScan();
  }

  @override
  void dispose() {
    scanSub?.cancel();
    connSub?.cancel();
    notifySub?.cancel();
    super.dispose();
  }

  void _log(String m) {
    setState(() {
      lastLog = "${DateTime.now().toIso8601String()} - $m\n" + lastLog;
      if (lastLog.length > 4000) lastLog = lastLog.substring(0, 4000);
    });
  }

  void _startScan() {
    setState(() { status = "Escaneando..."; feedback = "Escaneando BLE por servicio"; });
    _log("Iniciando scan por servicio $serviceUuid");
    scanSub = flutterReactiveBle.scanForDevices(withServices: [serviceUuid]).listen((device) {
      _log("Encontrado device: ${device.name} (${device.id}) - rssi ${device.rssi}");
      if (device.name == "Techeck_ESP32" || device.serviceUuids.contains(serviceUuid.toString())) {
        esp32Device = device;
        _log("Seleccionado dispositivo ${device.name}");
        scanSub?.cancel();
        setState(() { status = "Conectando a ${device.name}"; });
        _connectToDevice(device);
      }
    }, onError: (e) {
      _log("Error scan: $e");
      setState(() { status = "Error en scan"; });
    });
  }

  void _connectToDevice(DiscoveredDevice device) {
    connSub = flutterReactiveBle.connectToDevice(id: device.id).listen((update) async {
      _log("Connection state: ${update.connectionState}");
      setState(() { status = "Estado: ${update.connectionState}"; });
      if (update.connectionState == DeviceConnectionState.connected) {
        characteristic = QualifiedCharacteristic(
          deviceId: device.id,
          serviceId: serviceUuid,
          characteristicId: charUuid,
        );
        _log("Conectado: suscribiendo a característica $charUuid");
        try {
          notifySub = flutterReactiveBle.subscribeToCharacteristic(characteristic).listen((data) {
            String s = String.fromCharCodes(data);
            _log("Notificación recibida raw: ${data.toString()}");
            _log("Notificación recibida str: $s");
            setState(() {
              feedback = s;
            });
          }, onError: (e) {
            _log("Error en subscribe: $e");
            setState(() { status = "Error subscribe"; });
          });

          // Intentar leer valor inicial (si existe)
          try {
            final bytes = await flutterReactiveBle.readCharacteristic(characteristic);
            String initial = String.fromCharCodes(bytes);
            _log("ReadCharacteristic: $initial");
            setState(() { feedback = initial; });
          } catch (e) {
            _log("readCharacteristic falló: $e");
          }

          // Si querés enviar el ID del ejercicio al dispositivo (si la característica acepta WRITE),
          // descomenta y usa writeCharacteristicWithResponse o WithoutResponse según necesidad.
          // await flutterReactiveBle.writeCharacteristicWithResponse(characteristic, value: widget.ejercicioId?.codeUnits ?? []);

        } catch (e) {
          _log("Excepción al suscribir: $e");
          setState(() { status = "Error al suscribir"; });
        }
      }
    }, onError: (e) {
      _log("Error conexión: $e");
      setState(() { status = "Error conexión"; });
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Techeck BLE Setup')),
      body: Padding(
        padding: const EdgeInsets.all(12),
        child: Column(
          children: [
            Text("Estado: $status"),
            const SizedBox(height: 8),
            Card(child: Padding(padding: EdgeInsets.all(12), child: Text(feedback))),
            const SizedBox(height: 8),
            ElevatedButton(
              onPressed: () {
                scanSub?.cancel();
                connSub?.cancel();
                notifySub?.cancel();
                esp32Device = null;
                setState(() {
                  status = "Reiniciando scan";
                  feedback = "Reiniciando...";
                });
                _startScan();
              },
              child: const Text("Reintentar escaneo"),
            ),
            const SizedBox(height: 8),
            Expanded(child: SingleChildScrollView(child: Text(lastLog))),
          ],
        ),
      ),
    );
  }
}