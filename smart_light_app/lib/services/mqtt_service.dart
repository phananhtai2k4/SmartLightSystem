import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'package:flutter/foundation.dart';
import 'package:mqtt_client/mqtt_client.dart';
import 'package:mqtt_client/mqtt_server_client.dart';
import '../constants.dart';

/// Singleton service để quản lý kết nối MQTT với HiveMQ Cloud
class MqttService {
  static final MqttService _instance = MqttService._internal();
  factory MqttService() => _instance;
  MqttService._internal();

  MqttServerClient? _client;
  String? _currentClientId;
  bool _isConnecting = false;

  // Stream controllers để broadcast trạng thái và messages
  final _connectionStatusController =
      StreamController<MqttConnectionState>.broadcast();
  final _messageController =
      StreamController<MqttReceivedMessage<MqttMessage>>.broadcast();

  Stream<MqttConnectionState> get connectionStatusStream =>
      _connectionStatusController.stream;
  Stream<MqttReceivedMessage<MqttMessage>> get messageStream =>
      _messageController.stream;

  bool get isConnected =>
      _client?.connectionStatus?.state == MqttConnectionState.connected;
  MqttConnectionState? get connectionState => _client?.connectionStatus?.state;

  /// Kết nối tới HiveMQ Cloud broker
  Future<bool> connect(String clientId) async {
    print('🔌 MQTT: connect() called with clientId: $clientId');

    // Guard: Đang connecting thì skip
    if (_isConnecting) {
      print('⚠️ MQTT: Already connecting, skipping...');
      return false;
    }

    // Đã connected thì skip
    if (isConnected) {
      print('✅ MQTT: Already connected');
      return true;
    }

    // Nếu đã có client với cùng clientId, chỉ cần reconnect
    if (_client != null && _currentClientId == clientId) {
      print('🔄 MQTT: Reusing existing client, waiting for auto-reconnect...');
      return isConnected;
    }

    _isConnecting = true;
    _currentClientId = clientId;

    // Disconnect client cũ nếu có
    if (_client != null) {
      print('🔌 MQTT: Disconnecting old client...');
      _client!.autoReconnect = false;
      _client!.disconnect();
      _client = null;
    }

    _client = MqttServerClient.withPort(
      AppConstants.mqttHost,
      clientId,
      AppConstants.mqttPort,
    );

    // Cấu hình client
    _client!.logging(on: kDebugMode); // Log trong debug mode
    _client!.keepAlivePeriod = 30;
    _client!.autoReconnect = true;
    _client!.resubscribeOnAutoReconnect = true;
    _client!.secure = true; // Sử dụng TLS/SSL
    _client!.securityContext = SecurityContext.defaultContext;

    // Callbacks
    _client!.onConnected = _onConnected;
    _client!.onDisconnected = _onDisconnected;
    _client!.onAutoReconnect = _onAutoReconnect;
    _client!.onAutoReconnected = _onAutoReconnected;
    _client!.onSubscribed = _onSubscribed;
    _client!.onUnsubscribed = _onUnsubscribed;

    // Message về Last Will (khi mất kết nối đột ngột)
    final connMessage = MqttConnectMessage()
        .withClientIdentifier(clientId)
        .authenticateAs(AppConstants.mqttUser, AppConstants.mqttPass)
        .withWillTopic('clients/$clientId/status')
        .withWillMessage('offline')
        .withWillQos(MqttQos.atLeastOnce)
        .withWillRetain()
        .startClean();

    _client!.connectionMessage = connMessage;

    try {
      print(
        '🔌 MQTT: Connecting to ${AppConstants.mqttHost}:${AppConstants.mqttPort}...',
      );
      await _client!.connect();

      _isConnecting = false;

      if (isConnected) {
        print('✅ MQTT: Connected successfully!');

        // Lắng nghe messages từ tất cả topics đã subscribe
        _client!.updates?.listen((
          List<MqttReceivedMessage<MqttMessage>> messages,
        ) {
          for (var message in messages) {
            _messageController.add(message);

            // Log message nhận được
            final payload = message.payload as MqttPublishMessage;
            final payloadString = MqttPublishPayload.bytesToStringAsString(
              payload.payload.message,
            );
            print('📩 MQTT: Received on ${message.topic}: $payloadString');
          }
        });

        // Publish trạng thái online
        publish('clients/$clientId/status', 'online', retain: true);

        return true;
      }
    } on NoConnectionException catch (e) {
      print('❌ MQTT: Connection exception - $e');
      _isConnecting = false;
      _client?.disconnect();
    } on SocketException catch (e) {
      print('❌ MQTT: Socket exception - $e');
      _isConnecting = false;
      _client?.disconnect();
    } catch (e) {
      print('❌ MQTT: Unknown exception - $e');
      _isConnecting = false;
      _client?.disconnect();
    }

    return false;
  }

  /// Ngắt kết nối
  void disconnect() {
    if (_client != null) {
      debugPrint('MQTT: Disconnecting...');
      _client!.disconnect();
    }
  }

  /// Subscribe một topic
  void subscribe(String topic, {MqttQos qos = MqttQos.atLeastOnce}) {
    if (!isConnected) {
      debugPrint('MQTT: Cannot subscribe - not connected');
      return;
    }
    debugPrint('MQTT: Subscribing to $topic');
    _client!.subscribe(topic, qos);
  }

  /// Unsubscribe một topic
  void unsubscribe(String topic) {
    if (!isConnected) return;
    debugPrint('MQTT: Unsubscribing from $topic');
    _client!.unsubscribe(topic);
  }

  /// Publish message tới topic
  void publish(
    String topic,
    String message, {
    bool retain = false,
    MqttQos qos = MqttQos.atLeastOnce,
  }) {
    if (!isConnected) {
      debugPrint('MQTT: Cannot publish - not connected');
      return;
    }

    final builder = MqttClientPayloadBuilder();
    builder.addString(message);

    debugPrint('MQTT: Publishing to $topic: $message');
    _client!.publishMessage(topic, qos, builder.payload!, retain: retain);
  }

  /// Publish JSON message
  void publishJson(
    String topic,
    Map<String, dynamic> json, {
    bool retain = false,
  }) {
    publish(topic, jsonEncode(json), retain: retain);
  }

  // ==================== CALLBACKS ====================

  void _onConnected() {
    print('✅ MQTT: Connected callback');
    _connectionStatusController.add(MqttConnectionState.connected);
  }

  void _onDisconnected() {
    print('❌ MQTT: Disconnected callback');
    _connectionStatusController.add(MqttConnectionState.disconnected);
  }

  void _onAutoReconnect() {
    print('🔄 MQTT: Auto reconnecting...');
    _connectionStatusController.add(MqttConnectionState.connecting);
  }

  void _onAutoReconnected() {
    print('✅ MQTT: Auto reconnected');
    _connectionStatusController.add(MqttConnectionState.connected);
  }

  void _onSubscribed(String topic) {
    print('📥 MQTT: Subscribed to $topic');
  }

  void _onUnsubscribed(String? topic) {
    print('📤 MQTT: Unsubscribed from $topic');
  }

  /// Dispose resources
  void dispose() {
    disconnect();
    _connectionStatusController.close();
    _messageController.close();
  }
}
