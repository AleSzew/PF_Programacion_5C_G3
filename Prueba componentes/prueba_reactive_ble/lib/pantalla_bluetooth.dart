import 'package:flutter/material.dart';
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';

class PantallaBluetooth extends StatefulWidget {
  const PantallaBluetooth({Key? key}) : super(key: key);

  @override
  _PantallaBluetoothState createState() => _PantallaBluetoothState();
}

class _PantallaBluetoothState extends State<PantallaBluetooth> {
  final flutterReactiveBle = FlutterReactiveBle();
  String feedback = "Esperando datos...";

  DiscoveredDevice? esp32Device;
  late QualifiedCharacteristic characteristic;

  @override
  void initState() {
    super.initState();
    _scanAndConnect();
  }

  void _scanAndConnect() {
    flutterReactiveBle.scanForDevices(withServices: []).listen((device) {
      if (device.name == "Techeck_ESP32") {
        esp32Device = device;

        flutterReactiveBle.connectToDevice(id: device.id).listen((update) {
          if (update.connectionState == DeviceConnectionState.connected) {
            characteristic = QualifiedCharacteristic(
              serviceId: Uuid.parse("11111111-1111-1111-1111-111111111111"), // TecheckService
              characteristicId: Uuid.parse("22222222-2222-2222-2222-222222222222"), // TecheckFeedbackCharacteristic
              deviceId: device.id,
            );

            flutterReactiveBle.subscribeToCharacteristic(characteristic).listen((data) {
              setState(() {
                feedback = String.fromCharCodes(data);
              });
            });
          }
        });
      }
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Techeck BLE Setup')),
      body: Center(
        child: Card(
          child: Padding(
            padding: const EdgeInsets.all(20),
            child: Text(
              feedback,
              style: const TextStyle(fontSize: 18),
            ),
          ),
        ),
      ),
    );
  }
}
