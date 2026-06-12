import 'package:flutter/material.dart';
import 'package:flutter_bluetooth_serial/flutter_bluetooth_serial.dart';
import 'package:permission_handler/permission_handler.dart';

class PantallaBluetooth extends StatefulWidget {
  const PantallaBluetooth({Key? key}) : super(key: key);

  @override
  _PantallaBluetoothState createState() => _PantallaBluetoothState();
}
// Hola 
class _PantallaBluetoothState extends State<PantallaBluetooth> {
  BluetoothConnection? _connection;
  String feedback = "Esperando datos...";

  @override
  void initState() {
    super.initState();
    _initBluetooth();
  }

  Future<void> _initBluetooth() async {
    // Pedir permisos en tiempo de ejecución
    await Permission.bluetoothScan.request();
    await Permission.bluetoothConnect.request();


    // Intentar conectar después de permisos
    _connectToDevice();
  }

  Future<void> _connectToDevice() async {
  try {
    List<BluetoothDevice> devices =
        await FlutterBluetoothSerial.instance.getBondedDevices();

    // Buscar el ESP32 en la lista de emparejados
    BluetoothDevice? device = devices.firstWhere(
      (d) => d.address == '28:05:A5:0B:2C:72',
      orElse: () => throw Exception('ESP32 no emparejado'),
    );

    _connection = await BluetoothConnection.toAddress(device.address);
    print('Conectado a ${device.name}');

    _connection!.input!.listen((data) {
      String recibido = String.fromCharCodes(data).trim();
      setState(() {
        feedback = recibido;
      });
      print("Dato recibido: $recibido");
    });
  } catch (error) {
    setState(() {
      feedback = "Error al conectar: $error";
    });
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
