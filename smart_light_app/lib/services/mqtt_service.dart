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
    if (isConnected) {
      debugPrint('MQTT: Already connected');
      return true;
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
      debugPrint(
        'MQTT: Connecting to ${AppConstants.mqttHost}:${AppConstants.mqttPort}...',
      );
      await _client!.connect();

      if (isConnected) {
        debugPrint('MQTT: Connected successfully!');

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
            debugPrint('MQTT: Received on ${message.topic}: $payloadString');
          }
        });

        // Publish trạng thái online
        publish('clients/$clientId/status', 'online', retain: true);

        return true;
      }
    } on NoConnectionException catch (e) {
      debugPrint('MQTT: Connection exception - $e');
      _client?.disconnect();
    } on SocketException catch (e) {
      debugPrint('MQTT: Socket exception - $e');
      _client?.disconnect();
    } catch (e) {
      debugPrint('MQTT: Unknown exception - $e');
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
    debugPrint('MQTT: ✅ Connected');
    _connectionStatusController.add(MqttConnectionState.connected);
  }

  void _onDisconnected() {
    debugPrint('MQTT: ❌ Disconnected');
    _connectionStatusController.add(MqttConnectionState.disconnected);
  }

  void _onAutoReconnect() {
    debugPrint('MQTT: 🔄 Auto reconnecting...');
    _connectionStatusController.add(MqttConnectionState.connecting);
  }

  void _onAutoReconnected() {
    debugPrint('MQTT: ✅ Auto reconnected');
    _connectionStatusController.add(MqttConnectionState.connected);
  }

  void _onSubscribed(String topic) {
    debugPrint('MQTT: 📥 Subscribed to $topic');
  }

  void _onUnsubscribed(String? topic) {
    debugPrint('MQTT: 📤 Unsubscribed from $topic');
  }

  /// Dispose resources
  void dispose() {
    disconnect();
    _connectionStatusController.close();
    _messageController.close();
  }
}
