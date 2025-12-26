import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../../constants.dart';
import '../../providers/home_provider.dart';
import '../../models/node_model.dart';

class DeviceDetailScreen extends StatefulWidget {
  final NodeModel node;

  const DeviceDetailScreen({super.key, required this.node});

  @override
  State<DeviceDetailScreen> createState() => _DeviceDetailScreenState();
}

class _DeviceDetailScreenState extends State<DeviceDetailScreen> {
  late NodeModel _node;

  @override
  void initState() {
    super.initState();
    _node = widget.node;
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text(_node.name),
        backgroundColor: AppConstants.primaryColor,
        foregroundColor: Colors.white,
        actions: [
          PopupMenuButton<String>(
            onSelected: (value) => _handleMenuAction(value, context),
            itemBuilder: (context) => [
              const PopupMenuItem(
                value: 'edit',
                child: Row(
                  children: [
                    Icon(Icons.edit, color: Colors.grey),
                    SizedBox(width: 8),
                    Text('Đổi tên thiết bị'),
                  ],
                ),
              ),
              const PopupMenuItem(
                value: 'delete',
                child: Row(
                  children: [
                    Icon(Icons.delete, color: Colors.red),
                    SizedBox(width: 8),
                    Text('Xóa thiết bị', style: TextStyle(color: Colors.red)),
                  ],
                ),
              ),
            ],
          ),
        ],
      ),
      body: Consumer<HomeProvider>(
        builder: (context, homeProvider, child) {
          // Cập nhật node từ provider để có state mới nhất
          final updatedNode = homeProvider.nodes.firstWhere(
            (n) => n.id == _node.id,
            orElse: () => _node,
          );

          return ListView(
            padding: const EdgeInsets.all(16),
            children: [
              // Card điều khiển chính
              _ControlCard(
                node: updatedNode,
                onToggle: (value) {
                  homeProvider.toggleNode(updatedNode.id, value);
                },
              ),

              const SizedBox(height: 24),

              // Thông tin chi tiết
              // _DetailCard(node: updatedNode),
              const SizedBox(height: 24),

              // Trạng thái kết nối
              _StatusCard(node: updatedNode),
            ],
          );
        },
      ),
    );
  }

  void _handleMenuAction(String action, BuildContext context) {
    switch (action) {
      case 'edit':
        _showEditDialog(context);
        break;
      case 'delete':
        _showDeleteConfirmation(context);
        break;
    }
  }

  void _showEditDialog(BuildContext context) {
    final controller = TextEditingController(text: _node.name);

    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Đổi tên thiết bị'),
        content: TextField(
          controller: controller,
          decoration: const InputDecoration(
            labelText: 'Tên thiết bị',
            border: OutlineInputBorder(),
          ),
          autofocus: true,
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('Hủy'),
          ),
          ElevatedButton(
            style: ElevatedButton.styleFrom(
              backgroundColor: AppConstants.primaryColor,
              foregroundColor: Colors.white,
            ),
            onPressed: () async {
              if (controller.text.trim().isNotEmpty) {
                final success = await context
                    .read<HomeProvider>()
                    .updateNodeName(_node.id, controller.text.trim());
                if (context.mounted) {
                  Navigator.pop(context);
                  if (success) {
                    setState(() {
                      _node = _node.copyWith(name: controller.text.trim());
                    });
                    ScaffoldMessenger.of(context).showSnackBar(
                      const SnackBar(content: Text('Đã cập nhật tên thiết bị')),
                    );
                  }
                }
              }
            },
            child: const Text('Lưu'),
          ),
        ],
      ),
    );
  }

  void _showDeleteConfirmation(BuildContext context) {
    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Xóa thiết bị'),
        content: Text(
          'Bạn có chắc muốn xóa thiết bị "${_node.name}"?\n\nThao tác này không thể hoàn tác.',
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('Hủy'),
          ),
          ElevatedButton(
            style: ElevatedButton.styleFrom(
              backgroundColor: Colors.red,
              foregroundColor: Colors.white,
            ),
            onPressed: () async {
              final success = await context.read<HomeProvider>().deleteNode(
                _node.id,
              );
              if (context.mounted) {
                Navigator.pop(context); // Đóng dialog
                if (success) {
                  Navigator.pop(context); // Quay lại màn hình trước
                  ScaffoldMessenger.of(context).showSnackBar(
                    const SnackBar(content: Text('Đã xóa thiết bị')),
                  );
                }
              }
            },
            child: const Text('Xóa'),
          ),
        ],
      ),
    );
  }
}

class _ControlCard extends StatelessWidget {
  final NodeModel node;
  final Function(bool) onToggle;

  const _ControlCard({required this.node, required this.onToggle});

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.all(32),
      decoration: BoxDecoration(
        gradient: LinearGradient(
          begin: Alignment.topLeft,
          end: Alignment.bottomRight,
          colors: node.stateOn
              ? [
                  AppConstants.accentColor,
                  AppConstants.accentColor.withOpacity(0.7),
                ]
              : [Colors.grey.shade400, Colors.grey.shade300],
        ),
        borderRadius: BorderRadius.circular(24),
        boxShadow: [
          BoxShadow(
            color: (node.stateOn ? AppConstants.accentColor : Colors.grey)
                .withOpacity(0.4),
            blurRadius: 20,
            offset: const Offset(0, 8),
          ),
        ],
      ),
      child: Column(
        children: [
          // Icon đèn lớn
          Container(
            padding: const EdgeInsets.all(24),
            decoration: BoxDecoration(
              color: Colors.white.withOpacity(0.2),
              shape: BoxShape.circle,
            ),
            child: Icon(
              node.stateOn ? Icons.lightbulb : Icons.lightbulb_outline,
              size: 80,
              color: Colors.white,
            ),
          ),

          const SizedBox(height: 24),

          // Tên thiết bị
          Text(
            node.name,
            style: const TextStyle(
              color: Colors.white,
              fontSize: 24,
              fontWeight: FontWeight.bold,
            ),
          ),

          const SizedBox(height: 8),

          // Trạng thái
          Text(
            node.stateOn ? 'ĐANG BẬT' : 'ĐÃ TẮT',
            style: TextStyle(
              color: Colors.white.withOpacity(0.8),
              fontSize: 16,
              fontWeight: FontWeight.w500,
              letterSpacing: 2,
            ),
          ),

          const SizedBox(height: 24),

          // Switch lớn
          Transform.scale(
            scale: 1.5,
            child: Switch(
              value: node.stateOn,
              activeColor: Colors.white,
              activeTrackColor: Colors.white38,
              inactiveThumbColor: Colors.white,
              inactiveTrackColor: Colors.white24,
              onChanged: node.online ? onToggle : null,
            ),
          ),

          if (!node.online) ...[
            const SizedBox(height: 16),
            Container(
              padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
              decoration: BoxDecoration(
                color: Colors.red.withOpacity(0.3),
                borderRadius: BorderRadius.circular(20),
              ),
              child: const Row(
                mainAxisSize: MainAxisSize.min,
                children: [
                  Icon(Icons.wifi_off, color: Colors.white, size: 16),
                  SizedBox(width: 8),
                  Text(
                    'Thiết bị đang offline',
                    style: TextStyle(color: Colors.white),
                  ),
                ],
              ),
            ),
          ],
        ],
      ),
    );
  }
}

class _DetailCard extends StatelessWidget {
  final NodeModel node;

  const _DetailCard({required this.node});

  @override
  Widget build(BuildContext context) {
    return Card(
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(16)),
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Row(
              children: [
                Icon(Icons.info_outline, color: AppConstants.primaryColor),
                SizedBox(width: 8),
                Text(
                  'Thông tin thiết bị',
                  style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
                ),
              ],
            ),
            const SizedBox(height: 16),
            _DetailRow(
              icon: Icons.bluetooth,
              label: 'MAC Address',
              value: node.id,
            ),
            const Divider(),
            _DetailRow(
              icon: Icons.category,
              label: 'Loại thiết bị',
              value: _getDeviceTypeName(node.type),
            ),
            const Divider(),
            _DetailRow(
              icon: Icons.router,
              label: 'Gateway',
              value: node.gatewayMac,
            ),
            const Divider(),
            _DetailRow(
              icon: Icons.meeting_room,
              label: 'Room ID',
              value: node.roomId.isEmpty ? 'Chưa gán phòng' : node.roomId,
            ),
            const Divider(),
            _DetailRow(
              icon: Icons.numbers,
              label: 'Mesh Address',
              value: node.meshAddress.toString(),
            ),
          ],
        ),
      ),
    );
  }

  String _getDeviceTypeName(String type) {
    switch (type.toLowerCase()) {
      case 'light':
        return 'Đèn thông minh';
      case 'switch':
        return 'Công tắc';
      case 'sensor':
        return 'Cảm biến';
      default:
        return type;
    }
  }
}

class _DetailRow extends StatelessWidget {
  final IconData icon;
  final String label;
  final String value;

  const _DetailRow({
    required this.icon,
    required this.label,
    required this.value,
  });

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 8),
      child: Row(
        children: [
          Icon(icon, size: 20, color: Colors.grey),
          const SizedBox(width: 12),
          Expanded(
            flex: 2,
            child: Text(label, style: const TextStyle(color: Colors.grey)),
          ),
          Expanded(
            flex: 3,
            child: Text(
              value,
              style: const TextStyle(fontWeight: FontWeight.w500),
              textAlign: TextAlign.right,
              overflow: TextOverflow.ellipsis,
            ),
          ),
        ],
      ),
    );
  }
}

class _StatusCard extends StatelessWidget {
  final NodeModel node;

  const _StatusCard({required this.node});

  @override
  Widget build(BuildContext context) {
    return Card(
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(16)),
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Row(
              children: [
                Icon(Icons.wifi, color: AppConstants.primaryColor),
                SizedBox(width: 8),
                Text(
                  'Trạng thái kết nối',
                  style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
                ),
              ],
            ),
            const SizedBox(height: 16),
            Row(
              children: [
                Container(
                  padding: const EdgeInsets.all(12),
                  decoration: BoxDecoration(
                    color: node.online
                        ? Colors.green.withOpacity(0.1)
                        : Colors.red.withOpacity(0.1),
                    borderRadius: BorderRadius.circular(12),
                  ),
                  child: Icon(
                    node.online ? Icons.cloud_done : Icons.cloud_off,
                    color: node.online ? Colors.green : Colors.red,
                    size: 32,
                  ),
                ),
                const SizedBox(width: 16),
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        node.online ? 'Trực tuyến' : 'Ngoại tuyến',
                        style: TextStyle(
                          fontSize: 18,
                          fontWeight: FontWeight.bold,
                          color: node.online ? Colors.green : Colors.red,
                        ),
                      ),
                      Text(
                        node.online
                            ? 'Thiết bị đang kết nối và sẵn sàng'
                            : 'Không thể kết nối đến thiết bị',
                        style: const TextStyle(color: Colors.grey),
                      ),
                    ],
                  ),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}
