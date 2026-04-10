import 'dart:async';
import 'package:flutter/material.dart';
import 'package:cloud_firestore/cloud_firestore.dart';
import '../../constants.dart';
import '../../services/mesh_service.dart';

/// Màn hình xử lý quá trình thêm đèn thông minh
class AddLightProgressScreen extends StatefulWidget {
  final String homeId;
  final UnprovisionedMeshDevice device;

  const AddLightProgressScreen({
    super.key,
    required this.homeId,
    required this.device,
  });

  @override
  State<AddLightProgressScreen> createState() => _AddLightProgressScreenState();
}

class _AddLightProgressScreenState extends State<AddLightProgressScreen> {
  final MeshService _meshService = MeshService();
  final FirebaseFirestore _firestore = FirebaseFirestore.instance;

  // Trạng thái
  _AddLightState _currentState = _AddLightState.initializing;
  String _statusMessage = 'Đang khởi tạo...';
  String? _errorMessage;

  // BLE Mesh provisioning info
  int? _meshUnicastAddress;
  String? _meshDeviceKey;

  // Subscriptions
  StreamSubscription? _provisionSubscription;

  @override
  void initState() {
    super.initState();
    _meshService.initialize();
    _startProvisioning();
  }

  @override
  void dispose() {
    _provisionSubscription?.cancel();
    super.dispose();
  }

  /// Bắt đầu quá trình provisioning
  Future<void> _startProvisioning() async {
    setState(() {
      _currentState = _AddLightState.initializing;
      _statusMessage = 'Đang chuẩn bị...';
    });

    // Kiểm tra quyền BLE
    final hasPermission = await _meshService.requestPermissions();
    if (!hasPermission) {
      _setError(
        'Cần cấp quyền Bluetooth để tiếp tục. Vui lòng cấp quyền và thử lại.',
      );
      return;
    }

    // Kiểm tra Bluetooth có bật không
    final isEnabled = await _meshService.isBluetoothEnabled();
    if (!isEnabled) {
      _setError('Vui lòng bật Bluetooth để tiếp tục.');
      return;
    }

    // Bắt đầu provisioning
    _provisionLight();
  }

  /// Provision đèn vào mesh network
  Future<void> _provisionLight() async {
    setState(() {
      _currentState = _AddLightState.provisioning;
      _statusMessage = 'Đang thêm đèn vào mạng mesh...';
    });

    print(
      '🔗 Starting provisioning for ${widget.device.name} (${widget.device.address})',
    );

    // Lắng nghe provisioning events để cập nhật UI
    _provisionSubscription?.cancel();
    _provisionSubscription = _meshService.provisioningEvents.listen((event) {
      print('📡 Provisioning event: ${event.status}');

      String message = _statusMessage;
      switch (event.status) {
        case ProvisioningStatus.connecting:
          message = 'Đang kết nối Bluetooth...';
          break;
        case ProvisioningStatus.connected:
          message = 'Đã kết nối, đang chuẩn bị...';
          break;
        case ProvisioningStatus.serviceDiscovered:
          message = 'Đang trao đổi khóa bảo mật...';
          break;
        case ProvisioningStatus.provisioning:
          message = 'Đang provision thiết bị...';
          break;
        case ProvisioningStatus.sendingNetworkKey:
          message = 'Đang gửi Network Key...';
          break;
        case ProvisioningStatus.sendingAppKey:
          message = 'Đang gửi App Key...';
          break;
        default:
          break;
      }

      if (_currentState == _AddLightState.provisioning) {
        setState(() {
          _statusMessage = message;
        });
      }
    });

    // Thực hiện provisioning và đợi kết quả
    final result = await _meshService.provisionGatewayAndWait(
      widget.device,
      timeoutSeconds: 60,
    );

    _provisionSubscription?.cancel();

    if (result.status == ProvisioningStatus.success) {
      _meshUnicastAddress = result.unicastAddress;
      _meshDeviceKey = result.deviceKey;

      print(
        '🎉 Provisioning successful! Address: 0x${_meshUnicastAddress?.toRadixString(16)}',
      );

      // Lưu vào Firestore
      _saveLightToFirestore();
    } else {
      _setError(
        'Không thể thêm đèn vào mạng mesh: ${result.error ?? "Lỗi không xác định"}',
      );
    }
  }

  /// Lưu đèn vào Firestore
  Future<void> _saveLightToFirestore() async {
    setState(() {
      _currentState = _AddLightState.saving;
      _statusMessage = 'Đang lưu thiết bị...';
    });

    try {
      // Lấy MAC address từ BLE address
      // Format: CC:BA:97:0D:E2:3A -> CCBA970DE23A
      final nodeMac = widget.device.address.replaceAll(':', '');

      // Lấy gatewayMac của gateway thuộc home này
      String? gatewayMac;
      final gatewaySnapshot = await _firestore
          .collection(AppConstants.colGateways)
          .where('homeId', isEqualTo: widget.homeId)
          .limit(1)
          .get();

      if (gatewaySnapshot.docs.isNotEmpty) {
        gatewayMac = gatewaySnapshot.docs.first.data()['gatewayMac'] as String?;
        print('📡 Found gateway for home: $gatewayMac');
      } else {
        print('⚠️ No gateway found for home ${widget.homeId}');
      }

      // Tạo document với ID là MAC address
      await _firestore.collection(AppConstants.colNodes).doc(nodeMac).set({
        'nodeMac': nodeMac,
        'homeId': widget.homeId,
        'roomId': null, // Chưa assign vào phòng
        'gatewayMac': gatewayMac, // Gán gateway của home
        'name': widget.device.name ?? 'Đèn thông minh',
        'type': 'light',
        'meshAddress': _meshUnicastAddress,
        'meshDeviceKey': _meshDeviceKey,
        'online': true,
        'state_on': false,
        'createdAt': FieldValue.serverTimestamp(),
      });

      setState(() {
        _currentState = _AddLightState.success;
        _statusMessage = 'Thêm đèn thành công!';
      });
    } catch (e) {
      _setError('Không thể lưu thiết bị: ${e.toString()}');
    }
  }

  /// Set error state
  void _setError(String message) {
    setState(() {
      _currentState = _AddLightState.error;
      _errorMessage = message;
      _statusMessage = 'Đã xảy ra lỗi';
    });
  }

  /// Thử lại
  void _retry() {
    setState(() {
      _currentState = _AddLightState.initializing;
      _errorMessage = null;
      _meshUnicastAddress = null;
      _meshDeviceKey = null;
    });
    _startProvisioning();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Thêm đèn'),
        backgroundColor: AppConstants.primaryColor,
        foregroundColor: Colors.white,
        automaticallyImplyLeading: _currentState != _AddLightState.success,
      ),
      body: PopScope(
        canPop:
            _currentState == _AddLightState.success ||
            _currentState == _AddLightState.error,
        onPopInvokedWithResult: (didPop, result) async {
          if (didPop) return;

          // Hiện dialog xác nhận hủy
          final shouldPop = await showDialog<bool>(
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

          if (shouldPop == true && context.mounted) {
            Navigator.of(context).pop();
          }
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

                // Progress indicator hoặc error/success message
                if (_currentState == _AddLightState.error) ...[
                  Text(
                    _errorMessage ?? 'Đã xảy ra lỗi',
                    style: const TextStyle(fontSize: 14, color: Colors.red),
                    textAlign: TextAlign.center,
                  ),
                  const SizedBox(height: 24),
                  Row(
                    mainAxisAlignment: MainAxisAlignment.center,
                    children: [
                      OutlinedButton(
                        onPressed: () => Navigator.pop(context),
                        child: const Text('Quay lại'),
                      ),
                      const SizedBox(width: 16),
                      ElevatedButton.icon(
                        onPressed: _retry,
                        icon: const Icon(Icons.refresh),
                        label: const Text('Thử lại'),
                        style: ElevatedButton.styleFrom(
                          backgroundColor: AppConstants.primaryColor,
                          foregroundColor: Colors.white,
                        ),
                      ),
                    ],
                  ),
                ] else if (_currentState == _AddLightState.success) ...[
                  Text(
                    'Đèn ${widget.device.name} đã được thêm vào nhà của bạn.',
                    style: TextStyle(fontSize: 14, color: Colors.grey.shade600),
                    textAlign: TextAlign.center,
                  ),
                  const SizedBox(height: 8),
                  Text(
                    'Mesh Address: 0x${_meshUnicastAddress?.toRadixString(16).toUpperCase().padLeft(4, '0') ?? "N/A"}',
                    style: TextStyle(fontSize: 12, color: Colors.grey.shade500),
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
      case _AddLightState.error:
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
      case _AddLightState.success:
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
            color: AppConstants.primaryColor.withValues(alpha: 0.1),
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
      case _AddLightState.initializing:
        detail = 'Chuẩn bị kết nối Bluetooth...';
        break;
      case _AddLightState.provisioning:
        detail = 'Thiết bị: ${widget.device.name ?? "Unknown"}';
        break;
      case _AddLightState.saving:
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

enum _AddLightState { initializing, provisioning, saving, success, error }
