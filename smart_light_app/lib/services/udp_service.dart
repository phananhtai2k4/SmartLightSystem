import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'package:flutter/foundation.dart';

/// Service để giao tiếp UDP với Gateway
class UdpService {
  RawDatagramSocket? _socket;
  StreamSubscription? _subscription;

  final _messageController = StreamController<Map<String, dynamic>>.broadcast();
  Stream<Map<String, dynamic>> get messageStream => _messageController.stream;

  bool _isListening = false;
  bool get isListening => _isListening;

  static const int _listenPort = 8888;

  /// Khởi tạo UDP socket để lắng nghe và gửi broadcast
  Future<bool> initialize() async {
    try {
      // Bind socket để lắng nghe trên port 8888 (cùng port với gateway broadcast)
      _socket = await RawDatagramSocket.bind(
        InternetAddress.anyIPv4,
        _listenPort,
      );
      _socket!.broadcastEnabled = true;

      _isListening = true;

      // Lắng nghe messages từ gateway
      _subscription = _socket!.listen((RawSocketEvent event) {
        if (event == RawSocketEvent.read) {
          final datagram = _socket!.receive();
          if (datagram != null) {
            try {
              final message = utf8.decode(datagram.data);
              debugPrint(
                'UDP: Received from ${datagram.address.address}:${datagram.port}: $message',
              );

              // Parse JSON và broadcast qua stream
              final json = jsonDecode(message) as Map<String, dynamic>;
              _messageController.add(json);
            } catch (e) {
              debugPrint('UDP: Error parsing message: $e');
            }
          }
        }
      });

      debugPrint('UDP: Socket initialized on port ${_socket!.port}');
      return true;
    } catch (e) {
      debugPrint('UDP: Error initializing socket: $e');
      return false;
    }
  }

  /// Gửi UDP broadcast đến tất cả thiết bị trong mạng
  void broadcast(Map<String, dynamic> message, {int port = 8888}) {
    if (_socket == null) {
      debugPrint('UDP: Socket not initialized');
      return;
    }

    final jsonString = jsonEncode(message);
    final data = utf8.encode(jsonString);

    // Broadcast đến địa chỉ 192.168.4.255
    _socket!.send(data, InternetAddress('192.168.4.255'), port);
    debugPrint('UDP: Broadcast to port $port: $jsonString');
  }

  /// Gửi UDP đến địa chỉ cụ thể
  void sendTo(Map<String, dynamic> message, String address, int port) {
    if (_socket == null) {
      debugPrint('UDP: Socket not initialized');
      return;
    }

    final jsonString = jsonEncode(message);
    final data = utf8.encode(jsonString);

    _socket!.send(data, InternetAddress(address), port);
    debugPrint('UDP: Sent to $address:$port: $jsonString');
  }

  /// Gửi command lấy Device ID (broadcast)
  void sendGetDeviceId({int port = 8888}) {
    broadcast({'name': 'CmdGetDeviceID'}, port: port);
  }

  /// Gửi config cho device
  void sendDeviceConfig({
    required int devT,
    required String devExtAddr,
    required String wifiSsid,
    required String wifiPass,
    required String mqttUser,
    required String mqttPass,
    required String mqttUrl,
    int port = 8888,
  }) {
    final message = {
      'name': 'CmdSetDeviceConfig',
      'value': {
        'devT': devT,
        'devExtAddr': devExtAddr,
        'wifi_ssid': wifiSsid,
        'wifi_pass': wifiPass,
        'mqtt_user': mqttUser,
        'mqtt_pass': mqttPass,
        'mqtt_url': mqttUrl,
        'mqtt_sub': 'smartLight/sub/$devExtAddr/',
        'mqtt_pub': 'smartLight/pub/$devExtAddr/',
        'mqtt_alert': 'smartLight/alert/$devExtAddr/',
      },
    };
    broadcast(message, port: port);
  }

  /// Đóng socket
  void dispose() {
    _subscription?.cancel();
    _socket?.close();
    _messageController.close();
    _isListening = false;
    debugPrint('UDP: Socket closed');
  }
}
