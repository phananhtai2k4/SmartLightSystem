import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../../constants.dart';
import '../../providers/home_provider.dart';
import '../../models/room_model.dart';
import '../../models/node_model.dart';
import 'device_detail_screen.dart';

class RoomDetailScreen extends StatefulWidget {
  final RoomModel room;

  const RoomDetailScreen({super.key, required this.room});

  @override
  State<RoomDetailScreen> createState() => _RoomDetailScreenState();
}

class _RoomDetailScreenState extends State<RoomDetailScreen> {
  late RoomModel _room;

  @override
  void initState() {
    super.initState();
    _room = widget.room;
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text(_room.name),
        backgroundColor: AppConstants.primaryColor,
        foregroundColor: Colors.white,
        actions: [
          // Nút thêm thiết bị
          IconButton(
            icon: const Icon(Icons.add_circle_outline),
            onPressed: () {
              ScaffoldMessenger.of(context).showSnackBar(
                const SnackBar(
                  content: Text(
                    'Tính năng thêm thiết bị sẽ được phát triển sau',
                  ),
                ),
              );
            },
          ),
          // Menu
          PopupMenuButton<String>(
            onSelected: (value) => _handleMenuAction(value, context),
            itemBuilder: (context) => [
              const PopupMenuItem(
                value: 'edit',
                child: Row(
                  children: [
                    Icon(Icons.edit, color: Colors.grey),
                    SizedBox(width: 8),
                    Text('Sửa tên phòng'),
                  ],
                ),
              ),
              const PopupMenuItem(
                value: 'delete',
                child: Row(
                  children: [
                    Icon(Icons.delete, color: Colors.red),
                    SizedBox(width: 8),
                    Text('Xóa phòng', style: TextStyle(color: Colors.red)),
                  ],
                ),
              ),
            ],
          ),
        ],
      ),
      body: Consumer<HomeProvider>(
        builder: (context, homeProvider, child) {
          final nodes = homeProvider.getNodesInRoom(_room.id);
          final isAnyOn = homeProvider.isAnyNodeOnInRoom(_room.id);
          final areAllOn = homeProvider.areAllNodesOnInRoom(_room.id);

          return ListView(
            padding: const EdgeInsets.all(16),
            children: [
              // Header phòng
              _RoomHeader(
                room: _room,
                nodeCount: nodes.length,
                isAnyOn: isAnyOn,
                areAllOn: areAllOn,
                onToggleAll: (value) {
                  homeProvider.toggleAllNodesInRoom(_room.id, value);
                },
              ),

              const SizedBox(height: 24),

              // Thông tin phòng
              // _InfoCard(room: _room),
              const SizedBox(height: 24),

              // Danh sách thiết bị
              Row(
                mainAxisAlignment: MainAxisAlignment.spaceBetween,
                children: [
                  const Text(
                    'Thiết bị trong phòng',
                    style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
                  ),
                  Text(
                    '${nodes.length} thiết bị',
                    style: const TextStyle(color: Colors.grey),
                  ),
                ],
              ),

              const SizedBox(height: 12),

              if (nodes.isEmpty)
                _EmptyDeviceState()
              else
                GridView.builder(
                  shrinkWrap: true,
                  physics: const NeverScrollableScrollPhysics(),
                  gridDelegate: const SliverGridDelegateWithFixedCrossAxisCount(
                    crossAxisCount: 2,
                    childAspectRatio: 1.1,
                    crossAxisSpacing: 12,
                    mainAxisSpacing: 12,
                  ),
                  itemCount: nodes.length,
                  itemBuilder: (context, index) {
                    return _DeviceCard(node: nodes[index]);
                  },
                ),
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
    final controller = TextEditingController(text: _room.name);

    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Sửa tên phòng'),
        content: TextField(
          controller: controller,
          decoration: const InputDecoration(
            labelText: 'Tên phòng',
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
                final success = await context.read<HomeProvider>().updateRoom(
                  _room.id,
                  controller.text.trim(),
                );
                if (context.mounted) {
                  Navigator.pop(context);
                  if (success) {
                    setState(() {
                      _room = RoomModel(
                        id: _room.id,
                        homeId: _room.homeId,
                        name: controller.text.trim(),
                        groupAddress: _room.groupAddress,
                      );
                    });
                    ScaffoldMessenger.of(context).showSnackBar(
                      const SnackBar(content: Text('Đã cập nhật tên phòng')),
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
        title: const Text('Xóa phòng'),
        content: Text(
          'Bạn có chắc muốn xóa phòng "${_room.name}"?\n\nLưu ý: Các thiết bị trong phòng sẽ không bị xóa.',
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
              final success = await context.read<HomeProvider>().deleteRoom(
                _room.id,
              );
              if (context.mounted) {
                Navigator.pop(context); // Đóng dialog
                if (success) {
                  Navigator.pop(context); // Quay lại màn hình trước
                  ScaffoldMessenger.of(
                    context,
                  ).showSnackBar(const SnackBar(content: Text('Đã xóa phòng')));
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

class _RoomHeader extends StatelessWidget {
  final RoomModel room;
  final int nodeCount;
  final bool isAnyOn;
  final bool areAllOn;
  final Function(bool) onToggleAll;

  const _RoomHeader({
    required this.room,
    required this.nodeCount,
    required this.isAnyOn,
    required this.areAllOn,
    required this.onToggleAll,
  });

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.all(20),
      decoration: BoxDecoration(
        gradient: LinearGradient(
          colors: isAnyOn
              ? [
                  AppConstants.accentColor,
                  AppConstants.accentColor.withOpacity(0.7),
                ]
              : [Colors.grey.shade400, Colors.grey.shade300],
        ),
        borderRadius: BorderRadius.circular(16),
        boxShadow: [
          BoxShadow(
            color: (isAnyOn ? AppConstants.accentColor : Colors.grey)
                .withOpacity(0.3),
            blurRadius: 10,
            offset: const Offset(0, 4),
          ),
        ],
      ),
      child: Column(
        children: [
          Icon(_getRoomIcon(room.name), size: 64, color: Colors.white),
          const SizedBox(height: 12),
          Text(
            room.name,
            style: const TextStyle(
              color: Colors.white,
              fontSize: 24,
              fontWeight: FontWeight.bold,
            ),
          ),
          const SizedBox(height: 4),
          Text(
            '$nodeCount thiết bị',
            style: const TextStyle(color: Colors.white70, fontSize: 16),
          ),
          if (nodeCount > 0) ...[
            const SizedBox(height: 16),
            Row(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                const Text(
                  'Bật tất cả',
                  style: TextStyle(color: Colors.white, fontSize: 16),
                ),
                const SizedBox(width: 12),
                Switch(
                  value: areAllOn,
                  activeColor: Colors.white,
                  activeTrackColor: Colors.white38,
                  inactiveThumbColor: Colors.white,
                  inactiveTrackColor: Colors.white24,
                  onChanged: onToggleAll,
                ),
              ],
            ),
          ],
        ],
      ),
    );
  }

  IconData _getRoomIcon(String roomName) {
    final name = roomName.toLowerCase();
    if (name.contains('ngủ') || name.contains('bedroom')) {
      return Icons.bed;
    } else if (name.contains('khách') || name.contains('living')) {
      return Icons.weekend;
    } else if (name.contains('bếp') || name.contains('kitchen')) {
      return Icons.kitchen;
    } else if (name.contains('tắm') ||
        name.contains('wc') ||
        name.contains('bath')) {
      return Icons.bathtub;
    } else if (name.contains('làm việc') || name.contains('office')) {
      return Icons.computer;
    }
    return Icons.meeting_room;
  }
}

class _InfoCard extends StatelessWidget {
  final RoomModel room;

  const _InfoCard({required this.room});

  @override
  Widget build(BuildContext context) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text(
              'Thông tin phòng',
              style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold),
            ),
            const SizedBox(height: 12),
            _InfoRow(icon: Icons.tag, label: 'Room ID', value: room.id),
            const Divider(),
            _InfoRow(
              icon: Icons.numbers,
              label: 'Group Address',
              value: room.groupAddress.toString(),
            ),
          ],
        ),
      ),
    );
  }
}

class _InfoRow extends StatelessWidget {
  final IconData icon;
  final String label;
  final String value;

  const _InfoRow({
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
          Text('$label:', style: const TextStyle(color: Colors.grey)),
          const SizedBox(width: 8),
          Expanded(
            child: Text(
              value,
              style: const TextStyle(fontWeight: FontWeight.w500),
              overflow: TextOverflow.ellipsis,
            ),
          ),
        ],
      ),
    );
  }
}

class _DeviceCard extends StatelessWidget {
  final NodeModel node;

  const _DeviceCard({required this.node});

  @override
  Widget build(BuildContext context) {
    final homeProvider = context.read<HomeProvider>();

    return GestureDetector(
      onTap: () {
        Navigator.push(
          context,
          MaterialPageRoute(builder: (_) => DeviceDetailScreen(node: node)),
        );
      },
      child: Container(
        decoration: BoxDecoration(
          color: node.stateOn
              ? AppConstants.accentColor.withOpacity(0.2)
              : Colors.grey.shade100,
          borderRadius: BorderRadius.circular(16),
          border: Border.all(
            color: node.stateOn
                ? AppConstants.accentColor
                : Colors.grey.shade300,
            width: 2,
          ),
        ),
        padding: const EdgeInsets.all(16),
        child: Column(
          mainAxisAlignment: MainAxisAlignment.spaceBetween,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                Icon(
                  Icons.lightbulb,
                  size: 36,
                  color: node.stateOn ? AppConstants.accentColor : Colors.grey,
                ),
                Container(
                  padding: const EdgeInsets.symmetric(
                    horizontal: 8,
                    vertical: 4,
                  ),
                  decoration: BoxDecoration(
                    color: node.online
                        ? Colors.green.withOpacity(0.2)
                        : Colors.red.withOpacity(0.2),
                    borderRadius: BorderRadius.circular(12),
                  ),
                  child: Text(
                    node.online ? 'Online' : 'Offline',
                    style: TextStyle(
                      fontSize: 10,
                      fontWeight: FontWeight.bold,
                      color: node.online ? Colors.green : Colors.red,
                    ),
                  ),
                ),
              ],
            ),
            const Spacer(),
            Text(
              node.name,
              style: const TextStyle(fontWeight: FontWeight.w600, fontSize: 14),
              maxLines: 1,
              overflow: TextOverflow.ellipsis,
            ),
            const SizedBox(height: 4),
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                Text(
                  node.stateOn ? 'Đang bật' : 'Đã tắt',
                  style: TextStyle(
                    fontSize: 12,
                    color: node.stateOn ? Colors.orange : Colors.grey,
                  ),
                ),
                Transform.scale(
                  scale: 0.8,
                  child: Switch(
                    value: node.stateOn,
                    activeColor: AppConstants.accentColor,
                    onChanged: node.online
                        ? (value) {
                            homeProvider.toggleNode(node.id, value);
                          }
                        : null,
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

class _EmptyDeviceState extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    return Center(
      child: Column(
        children: [
          const SizedBox(height: 40),
          Icon(Icons.lightbulb_outline, size: 80, color: Colors.grey.shade300),
          const SizedBox(height: 16),
          const Text(
            'Chưa có thiết bị nào',
            style: TextStyle(fontSize: 18, color: Colors.grey),
          ),
          const SizedBox(height: 8),
          const Text(
            'Nhấn + để thêm thiết bị',
            style: TextStyle(color: Colors.grey),
          ),
        ],
      ),
    );
  }
}
