import 'dart:async';
import 'package:flutter/material.dart';
import '../../constants.dart';
import '../../services/mesh_service.dart';
import 'add_light_progress_screen.dart';

/// Màn hình quét và hiển thị danh sách các đèn thông minh
class ScanLightDeviceScreen extends StatefulWidget {
  final String homeId;

  const ScanLightDeviceScreen({super.key, required this.homeId});

  @override
  State<ScanLightDeviceScreen> createState() => _ScanLightDeviceScreenState();
}

class _ScanLightDeviceScreenState extends State<ScanLightDeviceScreen> {
  final MeshService _meshService = MeshService();
  final List<UnprovisionedMeshDevice> _devices = [];
  StreamSubscription? _scanSubscription;
  bool _isScanning = false;
  String? _errorMessage;

  @override
  void initState() {
    super.initState();
    _meshService.initialize(); // Khởi tạo MeshService để lắng nghe events
    _startScan();
  }

  @override
  void dispose() {
    _stopScan();
    super.dispose();
  }

  Future<void> _startScan() async {
    setState(() {
      _isScanning = true;
      _errorMessage = null;
      _devices.clear();
    });

    // Đăng ký listener TRƯỚC khi bắt đầu scan để không bỏ lỡ events
    _scanSubscription?.cancel();
    _scanSubscription = _meshService.scanEvents.listen((event) {
      debugPrint('ScanLightScreen: Received event: ${event.type}');
      if (event.type == 'deviceFound' && event.device != null) {
        final device = event.device!;
        debugPrint(
          'ScanLightScreen: Device found: ${device.name} (${device.address})',
        );
        // Chỉ hiển thị các thiết bị đèn (tên bắt đầu bằng DEVICE_)
        if (device.name?.startsWith('DEVICE_') == true) {
          debugPrint('ScanLightScreen: Adding DEVICE_ to list: ${device.name}');
          // Kiểm tra xem thiết bị đã có trong danh sách chưa
          final existingIndex = _devices.indexWhere(
            (d) => d.address == device.address,
          );
          setState(() {
            if (existingIndex >= 0) {
              // Cập nhật RSSI nếu thiết bị đã tồn tại
              _devices[existingIndex] = device;
            } else {
              // Thêm thiết bị mới
              _devices.add(device);
            }
            // Sắp xếp theo RSSI (tín hiệu mạnh nhất lên trước)
            _devices.sort((a, b) => b.rssi.compareTo(a.rssi));
          });
        }
      } else if (event.type == 'scanFailed') {
        setState(() {
          _isScanning = false;
          _errorMessage = 'Quét thất bại. Mã lỗi: ${event.errorCode}';
        });
      }
    });

    // Bắt đầu scan SAU KHI đã đăng ký listener
    final started = await _meshService.startScan();
    if (!started) {
      setState(() {
        _isScanning = false;
        _errorMessage = 'Không thể bắt đầu quét. Hãy kiểm tra quyền Bluetooth.';
      });
      return;
    }

    // Tự động dừng sau 30 giây
    Future.delayed(const Duration(seconds: 30), () {
      if (_isScanning && mounted) {
        _stopScan();
      }
    });
  }

  Future<void> _stopScan() async {
    _scanSubscription?.cancel();
    _scanSubscription = null;
    await _meshService.stopScan();
    if (mounted) {
      setState(() {
        _isScanning = false;
      });
    }
  }

  void _onDeviceSelected(UnprovisionedMeshDevice device) {
    _stopScan();
    Navigator.push(
      context,
      MaterialPageRoute(
        builder: (_) =>
            AddLightProgressScreen(homeId: widget.homeId, device: device),
      ),
    );
  }

  String _getSignalStrength(int rssi) {
    if (rssi >= -50) return 'Rất mạnh';
    if (rssi >= -60) return 'Mạnh';
    if (rssi >= -70) return 'Trung bình';
    if (rssi >= -80) return 'Yếu';
    return 'Rất yếu';
  }

  Color _getSignalColor(int rssi) {
    if (rssi >= -50) return Colors.green;
    if (rssi >= -60) return Colors.lightGreen;
    if (rssi >= -70) return Colors.orange;
    if (rssi >= -80) return Colors.deepOrange;
    return Colors.red;
  }

  IconData _getSignalIcon(int rssi) {
    if (rssi >= -60) return Icons.signal_cellular_4_bar;
    if (rssi >= -70) return Icons.signal_cellular_alt;
    if (rssi >= -80) return Icons.signal_cellular_alt_2_bar;
    return Icons.signal_cellular_alt_1_bar;
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Quét thiết bị'),
        backgroundColor: AppConstants.primaryColor,
        foregroundColor: Colors.white,
        actions: [
          if (_isScanning)
            const Padding(
              padding: EdgeInsets.all(16.0),
              child: SizedBox(
                width: 20,
                height: 20,
                child: CircularProgressIndicator(
                  strokeWidth: 2,
                  valueColor: AlwaysStoppedAnimation<Color>(Colors.white),
                ),
              ),
            )
          else
            IconButton(
              icon: const Icon(Icons.refresh),
              onPressed: _startScan,
              tooltip: 'Quét lại',
            ),
        ],
      ),
      body: Column(
        children: [
          // Header info
          Container(
            width: double.infinity,
            padding: const EdgeInsets.all(16),
            color: AppConstants.primaryColor.withValues(alpha: 0.1),
            child: Row(
              children: [
                Icon(
                  _isScanning ? Icons.bluetooth_searching : Icons.bluetooth,
                  color: AppConstants.primaryColor,
                ),
                const SizedBox(width: 12),
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        _isScanning ? 'Đang quét thiết bị...' : 'Quét hoàn tất',
                        style: const TextStyle(
                          fontWeight: FontWeight.bold,
                          fontSize: 14,
                        ),
                      ),
                      Text(
                        'Tìm thấy ${_devices.length} đèn',
                        style: TextStyle(
                          fontSize: 12,
                          color: Colors.grey.shade600,
                        ),
                      ),
                    ],
                  ),
                ),
                if (!_isScanning)
                  TextButton.icon(
                    onPressed: _startScan,
                    icon: const Icon(Icons.refresh, size: 18),
                    label: const Text('Quét lại'),
                  ),
              ],
            ),
          ),

          // Error message
          if (_errorMessage != null)
            Container(
              width: double.infinity,
              padding: const EdgeInsets.all(12),
              margin: const EdgeInsets.all(16),
              decoration: BoxDecoration(
                color: Colors.red.shade50,
                borderRadius: BorderRadius.circular(8),
                border: Border.all(color: Colors.red.shade200),
              ),
              child: Row(
                children: [
                  Icon(Icons.error_outline, color: Colors.red.shade700),
                  const SizedBox(width: 12),
                  Expanded(
                    child: Text(
                      _errorMessage!,
                      style: TextStyle(
                        fontSize: 13,
                        color: Colors.red.shade900,
                      ),
                    ),
                  ),
                ],
              ),
            ),

          // Device list
          Expanded(
            child: _devices.isEmpty
                ? Center(
                    child: Column(
                      mainAxisAlignment: MainAxisAlignment.center,
                      children: [
                        Icon(
                          _isScanning
                              ? Icons.bluetooth_searching
                              : Icons.lightbulb_outline,
                          size: 64,
                          color: Colors.grey.shade400,
                        ),
                        const SizedBox(height: 16),
                        Text(
                          _isScanning
                              ? 'Đang tìm kiếm đèn...'
                              : 'Không tìm thấy đèn nào',
                          style: TextStyle(
                            fontSize: 16,
                            color: Colors.grey.shade600,
                          ),
                        ),
                        if (!_isScanning) ...[
                          const SizedBox(height: 8),
                          Text(
                            'Hãy đảm bảo đèn đang ở chế độ cấu hình',
                            style: TextStyle(
                              fontSize: 13,
                              color: Colors.grey.shade500,
                            ),
                          ),
                          const SizedBox(height: 16),
                          ElevatedButton.icon(
                            onPressed: _startScan,
                            icon: const Icon(Icons.refresh),
                            label: const Text('Quét lại'),
                            style: ElevatedButton.styleFrom(
                              backgroundColor: AppConstants.primaryColor,
                              foregroundColor: Colors.white,
                            ),
                          ),
                        ],
                      ],
                    ),
                  )
                : ListView.builder(
                    padding: const EdgeInsets.all(16),
                    itemCount: _devices.length,
                    itemBuilder: (context, index) {
                      final device = _devices[index];
                      return _DeviceCard(
                        device: device,
                        signalStrength: _getSignalStrength(device.rssi),
                        signalColor: _getSignalColor(device.rssi),
                        signalIcon: _getSignalIcon(device.rssi),
                        onTap: () => _onDeviceSelected(device),
                      );
                    },
                  ),
          ),
        ],
      ),
    );
  }
}

class _DeviceCard extends StatelessWidget {
  final UnprovisionedMeshDevice device;
  final String signalStrength;
  final Color signalColor;
  final IconData signalIcon;
  final VoidCallback onTap;

  const _DeviceCard({
    required this.device,
    required this.signalStrength,
    required this.signalColor,
    required this.signalIcon,
    required this.onTap,
  });

  @override
  Widget build(BuildContext context) {
    // Lấy MAC suffix từ tên thiết bị (DEVICE_XXXX -> XXXX)
    final macSuffix = device.name?.replaceFirst('DEVICE_', '') ?? '';

    return Card(
      margin: const EdgeInsets.only(bottom: 12),
      elevation: 2,
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(12),
        side: BorderSide(
          color: AppConstants.primaryColor.withValues(alpha: 0.2),
        ),
      ),
      child: InkWell(
        onTap: onTap,
        borderRadius: BorderRadius.circular(12),
        child: Padding(
          padding: const EdgeInsets.all(16),
          child: Row(
            children: [
              // Light icon
              Container(
                padding: const EdgeInsets.all(12),
                decoration: BoxDecoration(
                  color: AppConstants.primaryColor.withValues(alpha: 0.1),
                  borderRadius: BorderRadius.circular(12),
                ),
                child: const Icon(
                  Icons.lightbulb,
                  size: 32,
                  color: AppConstants.primaryColor,
                ),
              ),
              const SizedBox(width: 16),

              // Device info
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(
                      device.name ?? 'Unknown Device',
                      style: const TextStyle(
                        fontSize: 16,
                        fontWeight: FontWeight.bold,
                      ),
                    ),
                    const SizedBox(height: 4),
                    Text(
                      'MAC: ...$macSuffix',
                      style: TextStyle(
                        fontSize: 12,
                        color: Colors.grey.shade600,
                      ),
                    ),
                    const SizedBox(height: 4),
                    Row(
                      children: [
                        Icon(signalIcon, size: 14, color: signalColor),
                        const SizedBox(width: 4),
                        Text(
                          '$signalStrength (${device.rssi} dBm)',
                          style: TextStyle(
                            fontSize: 11,
                            color: signalColor,
                            fontWeight: FontWeight.w500,
                          ),
                        ),
                      ],
                    ),
                  ],
                ),
              ),

              // Arrow
              Icon(Icons.chevron_right, color: Colors.grey.shade400),
            ],
          ),
        ),
      ),
    );
  }
}
