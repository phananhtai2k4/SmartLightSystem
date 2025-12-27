import 'dart:async';
import 'dart:convert';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:cloud_firestore/cloud_firestore.dart';
import '../../constants.dart';
import '../../services/udp_service.dart';
import '../../providers/user_provider.dart';

/// Màn hình xử lý quá trình thêm Gateway
class AddGatewayProgressScreen extends StatefulWidget {
  final String homeId;
  final String wifiSsid;
  final String wifiPassword;

  const AddGatewayProgressScreen({
    super.key,
    required this.homeId,
    required this.wifiSsid,
    required this.wifiPassword,
  });

  @override
  State<AddGatewayProgressScreen> createState() =>
      _AddGatewayProgressScreenState();
}

class _AddGatewayProgressScreenState extends State<AddGatewayProgressScreen> {
  final UdpService _udpService = UdpService();
  final FirebaseFirestore _firestore = FirebaseFirestore.instance;

  // Trạng thái
  _AddGatewayState _currentState = _AddGatewayState.initializing;
  String _statusMessage = 'Đang khởi tạo...';
  String? _errorMessage;

  // Thông tin thiết bị nhận được
  int? _devT;
  String? _devExtAddr;
  String? _topicSub;
  String? _topicPub;
  String? _topicAlert;

  // Timers & Subscriptions
  Timer? _broadcastTimer;
  Timer? _timeoutTimer;
  StreamSubscription? _udpSubscription;
  StreamSubscription? _mqttSubscription;

  @override
  void initState() {
    super.initState();
    _startProcess();
  }

  @override
  void dispose() {
    _broadcastTimer?.cancel();
    _timeoutTimer?.cancel();
    _udpSubscription?.cancel();
    _mqttSubscription?.cancel();
    _udpService.dispose();
    super.dispose();
  }

  /// Bắt đầu quá trình thêm Gateway
  Future<void> _startProcess() async {
    setState(() {
      _currentState = _AddGatewayState.initializing;
      _statusMessage = 'Đang khởi tạo kết nối UDP...';
    });

    // Khởi tạo UDP
    final success = await _udpService.initialize();
    if (!success) {
      _setError('Không thể khởi tạo kết nối UDP');
      return;
    }

    // Lắng nghe messages từ Gateway
    _udpSubscription = _udpService.messageStream.listen(_handleUdpMessage);

    // Bắt đầu broadcast để tìm Gateway
    _startBroadcasting();
  }

  /// Broadcast liên tục để tìm Gateway
  void _startBroadcasting() {
    setState(() {
      _currentState = _AddGatewayState.searchingDevice;
      _statusMessage = 'Đang tìm kiếm Gateway...';
    });

    // Timeout sau 60 giây
    _timeoutTimer = Timer(const Duration(seconds: 60), () {
      if (_currentState == _AddGatewayState.searchingDevice) {
        _setError(
          'Không tìm thấy Gateway. Vui lòng kiểm tra kết nối WiFi và thử lại.',
        );
      }
    });

    // Broadcast mỗi 2 giây
    _udpService.sendGetDeviceId();
    _broadcastTimer = Timer.periodic(const Duration(seconds: 2), (timer) {
      if (_currentState == _AddGatewayState.searchingDevice) {
        _udpService.sendGetDeviceId();
      } else {
        timer.cancel();
      }
    });
  }

  /// Xử lý message UDP nhận được từ Gateway
  void _handleUdpMessage(Map<String, dynamic> message) {
    final name = message['name'] as String?;
    debugPrint('UDP Message received: $message');

    switch (name) {
      case 'CmdGetDeviceID':
        // Kiểm tra đây là response từ Gateway (có devT và devExtAddr)
        // chứ không phải message broadcast của chính app
        if (message.containsKey('devT') && message.containsKey('devExtAddr')) {
          if (_currentState == _AddGatewayState.searchingDevice) {
            _handleDeviceFound(message);
          }
        } else {
          debugPrint('UDP: Ignoring own broadcast message');
        }
        break;
      case 'CmdGetData':
        // Nhận được xác nhận config
        if (_currentState == _AddGatewayState.sendingConfig) {
          _handleConfigConfirmation(message);
        }
        break;
    }
  }

  /// Xử lý khi tìm thấy Gateway
  void _handleDeviceFound(Map<String, dynamic> message) {
    _broadcastTimer?.cancel();
    _timeoutTimer?.cancel();

    // devT có thể là int hoặc string, cần parse linh hoạt
    final devTValue = message['devT'];
    if (devTValue is int) {
      _devT = devTValue;
    } else if (devTValue is String) {
      _devT = int.tryParse(devTValue);
    }

    _devExtAddr = message['devExtAddr'] as String?;

    if (_devT == null || _devExtAddr == null) {
      _setError('Thông tin thiết bị không hợp lệ');
      return;
    }

    debugPrint('Gateway found: devT=$_devT, devExtAddr=$_devExtAddr');

    // Gửi config cho Gateway
    _sendConfig();
  }

  /// Gửi config WiFi và MQTT cho Gateway
  void _sendConfig() {
    setState(() {
      _currentState = _AddGatewayState.sendingConfig;
      _statusMessage = 'Đang gửi cấu hình cho Gateway...';
    });

    // Tạo topics
    _topicSub = 'smartLight/sub/$_devExtAddr/';
    _topicPub = 'smartLight/pub/$_devExtAddr/';
    _topicAlert = 'smartLight/alert/$_devExtAddr/';

    // Gửi config
    _udpService.sendDeviceConfig(
      devT: _devT!,
      devExtAddr: _devExtAddr!,
      wifiSsid: widget.wifiSsid,
      wifiPass: widget.wifiPassword,
      mqttUser: AppConstants.mqttUser,
      mqttPass: AppConstants.mqttPass,
      mqttUrl: 'mqtts://${AppConstants.mqttHost}:${AppConstants.mqttPort}',
    );

    // Timeout cho config
    _timeoutTimer = Timer(const Duration(seconds: 30), () {
      if (_currentState == _AddGatewayState.sendingConfig) {
        _setError('Gateway không phản hồi. Vui lòng thử lại.');
      }
    });
  }

  /// Xử lý xác nhận config từ Gateway
  void _handleConfigConfirmation(Map<String, dynamic> message) {
    _timeoutTimer?.cancel();

    // Kiểm tra thông tin config có đúng không
    final receivedSsid = message['wifi_ssid'] as String?;
    final receivedPass = message['wifi_pass'] as String?;
    final receivedDevExtAddr = message['devExtAddr'] as String?;

    // Lấy mqtt_pub từ response của Gateway
    final receivedMqttPub = message['mqtt_pub'] as String?;
    if (receivedMqttPub != null) {
      _topicPub = receivedMqttPub;
    }

    if (receivedSsid != widget.wifiSsid ||
        receivedPass != widget.wifiPassword ||
        receivedDevExtAddr != _devExtAddr) {
      _setError('Xác nhận cấu hình không khớp. Vui lòng thử lại.');
      return;
    }

    print('✅ Config confirmed! mqtt_pub: $_topicPub');

    // Chờ Gateway kết nối MQTT
    _waitForMqttConnection();
  }

  /// Chờ Gateway kết nối MQTT và gửi message
  void _waitForMqttConnection() {
    setState(() {
      _currentState = _AddGatewayState.waitingMqtt;
      _statusMessage = 'Đang chờ Gateway kết nối đám mây...';
    });

    final userProvider = Provider.of<UserProvider>(context, listen: false);

    // Subscribe topic mqtt_pub để nhận bản tin từ Gateway
    // Gateway sẽ publish lên mqtt_pub, app cần subscribe mqtt_pub để nghe
    if (userProvider.isMqttConnected) {
      _subscribeAndWaitForGateway(userProvider);
    } else {
      print('⚠️ App chưa kết nối MQTT, đợi kết nối...');
      // Lắng nghe khi MQTT kết nối thì subscribe
      _mqttSubscription = userProvider.mqttService.connectionStatusStream
          .listen((state) {
            if (userProvider.isMqttConnected &&
                _currentState == _AddGatewayState.waitingMqtt) {
              print('✅ App đã kết nối MQTT, subscribe topic...');
              _subscribeAndWaitForGateway(userProvider);
            }
          });
    }

    // Timeout 90 giây cho MQTT (tăng thời gian vì gateway cần kết nối WiFi trước)
    _timeoutTimer = Timer(const Duration(seconds: 90), () {
      if (_currentState == _AddGatewayState.waitingMqtt) {
        _setError(
          'Gateway không kết nối được đám mây. Vui lòng kiểm tra mạng WiFi và thử lại.',
        );
      }
    });
  }

  /// Subscribe mqtt_pub và lắng nghe message từ Gateway
  void _subscribeAndWaitForGateway(UserProvider userProvider) {
    // Subscribe topic mqtt_pub (topic mà gateway publish lên)
    final topicToSubscribe = _topicPub ?? 'smartLight/pub/$_devExtAddr/';
    print('📡 Subscribing to: $topicToSubscribe');
    userProvider.mqttService.subscribe(topicToSubscribe);

    // Hủy subscription cũ nếu có
    _mqttSubscription?.cancel();

    // Lắng nghe message từ Gateway qua MQTT
    _mqttSubscription = userProvider.mqttService.messageStream.listen((
      mqttMessage,
    ) {
      final topic = mqttMessage.topic;
      print('📩 MQTT message on topic: $topic');

      // Kiểm tra message từ gateway (topic mqtt_pub)
      if (topic == topicToSubscribe ||
          topic.startsWith('smartLight/pub/$_devExtAddr')) {
        // Parse payload
        try {
          final payload = _getMqttPayload(mqttMessage);
          print('📩 Payload: $payload');
          final json = _parseJson(payload);

          if (json != null && json['name'] == 'CmdGetDeviceID') {
            final receivedDevExtAddr = json['devExtAddr'] as String?;
            if (receivedDevExtAddr == _devExtAddr) {
              print('🎉 Gateway connected to MQTT! Device: $_devExtAddr');
              _mqttSubscription?.cancel();
              _timeoutTimer?.cancel();
              _saveGatewayToFirestore();
            }
          }
        } catch (e) {
          print('❌ Error parsing MQTT message: $e');
        }
      }
    });
  }

  /// Lấy payload từ MQTT message
  String _getMqttPayload(dynamic mqttMessage) {
    try {
      final publishMessage = mqttMessage.payload;
      final payload = publishMessage.payload.message;
      return String.fromCharCodes(payload);
    } catch (e) {
      return '';
    }
  }

  /// Parse JSON an toàn
  Map<String, dynamic>? _parseJson(String text) {
    try {
      return Map<String, dynamic>.from(
        (text.isNotEmpty) ? _decodeJson(text) : {},
      );
    } catch (e) {
      return null;
    }
  }

  dynamic _decodeJson(String text) {
    try {
      return const JsonDecoder().convert(text);
    } catch (e) {
      return {};
    }
  }

  /// Lưu Gateway vào Firestore
  Future<void> _saveGatewayToFirestore() async {
    setState(() {
      _currentState = _AddGatewayState.saving;
      _statusMessage = 'Đang lưu thiết bị...';
    });

    try {
      // Tạo document với ID là MAC address
      await _firestore
          .collection(AppConstants.colGateways)
          .doc(_devExtAddr)
          .set({
            'gatewayMac': _devExtAddr,
            'homeId': widget.homeId,
            'name': 'GATEWAY_$_devExtAddr',
            'online': true,
            'topicSub': _topicSub,
            'topicPub': _topicPub,
            'topicAlert': _topicAlert,
          });

      setState(() {
        _currentState = _AddGatewayState.success;
        _statusMessage = 'Thêm Gateway thành công!';
      });
    } catch (e) {
      _setError('Không thể lưu thiết bị: ${e.toString()}');
    }
  }

  /// Set error state
  void _setError(String message) {
    _broadcastTimer?.cancel();
    _timeoutTimer?.cancel();
    setState(() {
      _currentState = _AddGatewayState.error;
      _errorMessage = message;
      _statusMessage = 'Đã xảy ra lỗi';
    });
  }

  /// Thử lại từ đầu
  void _retry() {
    setState(() {
      _currentState = _AddGatewayState.initializing;
      _errorMessage = null;
      _devT = null;
      _devExtAddr = null;
    });
    _startProcess();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Thêm Gateway'),
        backgroundColor: AppConstants.primaryColor,
        foregroundColor: Colors.white,
        automaticallyImplyLeading: _currentState != _AddGatewayState.success,
      ),
      body: WillPopScope(
        onWillPop: () async {
          if (_currentState == _AddGatewayState.success) {
            return true;
          }
          // Hiện dialog xác nhận hủy
          final result = await showDialog<bool>(
            context: context,
            builder: (context) => AlertDialog(
              title: const Text('Hủy thêm thiết bị?'),
              content: const Text(
                'Quá trình thêm thiết bị sẽ bị hủy. Bạn có chắc chắn?',
              ),
              actions: [
                TextButton(
                  onPressed: () => Navigator.pop(context, false),
                  child: const Text('Tiếp tục'),
                ),
                TextButton(
                  onPressed: () => Navigator.pop(context, true),
                  child: const Text('Hủy'),
                ),
              ],
            ),
          );
          return result ?? false;
        },
        child: Center(
          child: Padding(
            padding: const EdgeInsets.all(32.0),
            child: Column(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                // Icon/Animation based on state
                _buildStateIcon(),
                const SizedBox(height: 32),

                // Status message
                Text(
                  _statusMessage,
                  style: const TextStyle(
                    fontSize: 18,
                    fontWeight: FontWeight.bold,
                  ),
                  textAlign: TextAlign.center,
                ),
                const SizedBox(height: 12),

                // Progress indicator hoặc error message
                if (_currentState == _AddGatewayState.error) ...[
                  Text(
                    _errorMessage ?? 'Đã xảy ra lỗi',
                    style: const TextStyle(fontSize: 14, color: Colors.red),
                    textAlign: TextAlign.center,
                  ),
                  const SizedBox(height: 24),
                  ElevatedButton.icon(
                    onPressed: _retry,
                    icon: const Icon(Icons.refresh),
                    label: const Text('Thử lại'),
                    style: ElevatedButton.styleFrom(
                      backgroundColor: AppConstants.primaryColor,
                      foregroundColor: Colors.white,
                    ),
                  ),
                ] else if (_currentState == _AddGatewayState.success) ...[
                  Text(
                    'Gateway $_devExtAddr đã được thêm vào nhà của bạn.',
                    style: TextStyle(fontSize: 14, color: Colors.grey.shade600),
                    textAlign: TextAlign.center,
                  ),
                  const SizedBox(height: 24),
                  ElevatedButton(
                    onPressed: () {
                      // Pop về màn hình dashboard
                      Navigator.of(context).popUntil((route) => route.isFirst);
                    },
                    style: ElevatedButton.styleFrom(
                      backgroundColor: Colors.green,
                      foregroundColor: Colors.white,
                      padding: const EdgeInsets.symmetric(
                        horizontal: 32,
                        vertical: 12,
                      ),
                    ),
                    child: const Text('Hoàn tất'),
                  ),
                ] else ...[
                  const SizedBox(height: 16),
                  _buildProgressDetail(),
                ],
              ],
            ),
          ),
        ),
      ),
    );
  }

  Widget _buildStateIcon() {
    switch (_currentState) {
      case _AddGatewayState.error:
        return Container(
          padding: const EdgeInsets.all(24),
          decoration: BoxDecoration(
            color: Colors.red.shade50,
            shape: BoxShape.circle,
          ),
          child: Icon(
            Icons.error_outline,
            size: 80,
            color: Colors.red.shade400,
          ),
        );
      case _AddGatewayState.success:
        return Container(
          padding: const EdgeInsets.all(24),
          decoration: BoxDecoration(
            color: Colors.green.shade50,
            shape: BoxShape.circle,
          ),
          child: Icon(
            Icons.check_circle_outline,
            size: 80,
            color: Colors.green.shade400,
          ),
        );
      default:
        return Container(
          padding: const EdgeInsets.all(24),
          decoration: BoxDecoration(
            color: AppConstants.primaryColor.withOpacity(0.1),
            shape: BoxShape.circle,
          ),
          child: const SizedBox(
            width: 80,
            height: 80,
            child: CircularProgressIndicator(
              strokeWidth: 6,
              color: AppConstants.primaryColor,
            ),
          ),
        );
    }
  }

  Widget _buildProgressDetail() {
    String detail = '';
    switch (_currentState) {
      case _AddGatewayState.initializing:
        detail = 'Chuẩn bị kết nối...';
        break;
      case _AddGatewayState.searchingDevice:
        detail = 'Đang gửi tín hiệu tìm kiếm qua UDP...';
        break;
      case _AddGatewayState.sendingConfig:
        detail = 'WiFi: ${widget.wifiSsid}';
        break;
      case _AddGatewayState.waitingMqtt:
        detail = 'Gateway đang kết nối Internet...';
        break;
      case _AddGatewayState.saving:
        detail = 'Đang cập nhật cơ sở dữ liệu...';
        break;
      default:
        break;
    }

    return Text(
      detail,
      style: TextStyle(fontSize: 13, color: Colors.grey.shade600),
      textAlign: TextAlign.center,
    );
  }
}

enum _AddGatewayState {
  initializing,
  searchingDevice,
  sendingConfig,
  waitingMqtt,
  saving,
  success,
  error,
}
