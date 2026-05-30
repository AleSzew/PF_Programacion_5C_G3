import 'package:flutter/material.dart';
import 'package:flutter_bluetooth_serial/flutter_bluetooth_serial.dart';

class PantallaBluetooth extends StatefulWidget {
  const PantallaBluetooth({Key? key}) : super(key: key);

  @override
  _PantallaBluetoothState createState() => _PantallaBluetoothState();
}

class _PantallaBluetoothState extends State<PantallaBluetooth> {
  BluetoothConnection? _connection;
  String feedback = "Esperando datos...";

  @override
  void initState() {
    super.initState();
    _connectToDevice();
  }

  Future<void> _connectToDevice() async {
    List<BluetoothDevice> devices =
        await FlutterBluetoothSerial.instance.getBondedDevices();

    String deviceAddress = ' 28:05:A5:0B:2C:72 '; // MAC real de tu ESP32
    BluetoothDevice device =
        devices.firstWhere((d) => d.address == deviceAddress);

    try {
      _connection = await BluetoothConnection.toAddress(device.address);
      print('Conectado a ${device.name}');

      // Escuchar datos entrantes
      _connection!.input!.listen((data) {
        String recibido = String.fromCharCodes(data).trim();
        setState(() {
          feedback = recibido;
        });
        print("Dato recibido: $recibido");
      });
    } catch (error) {
      print('Error al conectar: $error');
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Bluetooth Setup')),
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
