import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../../constants.dart';
import '../../providers/home_provider.dart';
import '../../models/gateway_model.dart';

class GatewayDetailScreen extends StatefulWidget {
  final GatewayModel gateway;

  const GatewayDetailScreen({super.key, required this.gateway});

  @override
  State<GatewayDetailScreen> createState() => _GatewayDetailScreenState();
}

class _GatewayDetailScreenState extends State<GatewayDetailScreen> {
  late GatewayModel _gateway;

  @override
  void initState() {
    super.initState();
    _gateway = widget.gateway;
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text(_gateway.name),
        backgroundColor: AppConstants.primaryColor,
        foregroundColor: Colors.white,
        actions: [
          PopupMenuButton<String>(
            onSelected: (value) => _handleMenuAction(value, context),
            itemBuilder: (context) => [
              const PopupMenuItem(
                value: 'delete',
                child: Row(
                  children: [
                    Icon(Icons.delete, color: Colors.red),
                    SizedBox(width: 8),
                    Text('Xóa Gateway', style: TextStyle(color: Colors.red)),
                  ],
                ),
              ),
            ],
          ),
        ],
      ),
      body: Consumer<HomeProvider>(
        builder: (context, homeProvider, child) {
          // Lấy các nodes thuộc gateway này
          final gatewayNodes = homeProvider.nodes
              .where((n) => n.gatewayMac == _gateway.id)
              .toList();

          return ListView(
            padding: const EdgeInsets.all(16),
            children: [
              // Header Gateway
              _GatewayHeader(gateway: _gateway),

              const SizedBox(height: 24),

              // Thông tin chi tiết
              // _DetailCard(gateway: _gateway),

              // const SizedBox(height: 24),

              // MQTT Topics
              // _MqttTopicsCard(gateway: _gateway),
              const SizedBox(height: 24),

              // Nodes thuộc Gateway
              Card(
                shape: RoundedRectangleBorder(
                  borderRadius: BorderRadius.circular(16),
                ),
                child: Padding(
                  padding: const EdgeInsets.all(16),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Row(
                        mainAxisAlignment: MainAxisAlignment.spaceBetween,
                        children: [
                          const Row(
                            children: [
                              Icon(
                                Icons.lightbulb_outline,
                                color: AppConstants.primaryColor,
                              ),
                              SizedBox(width: 8),
                              Text(
                                'Thiết bị kết nối',
                                style: TextStyle(
                                  fontSize: 18,
                                  fontWeight: FontWeight.bold,
                                ),
                              ),
                            ],
                          ),
                          Container(
                            padding: const EdgeInsets.symmetric(
                              horizontal: 12,
                              vertical: 4,
                            ),
                            decoration: BoxDecoration(
                              color: AppConstants.primaryColor.withOpacity(0.1),
                              borderRadius: BorderRadius.circular(12),
                            ),
                            child: Text(
                              '${gatewayNodes.length} thiết bị',
                              style: const TextStyle(
                                color: AppConstants.primaryColor,
                                fontWeight: FontWeight.bold,
                              ),
                            ),
                          ),
                        ],
                      ),
                      const SizedBox(height: 16),
                      if (gatewayNodes.isEmpty)
                        const Center(
                          child: Padding(
                            padding: EdgeInsets.all(24),
                            child: Column(
                              children: [
                                Icon(
                                  Icons.devices_other,
                                  size: 48,
                                  color: Colors.grey,
                                ),
                                SizedBox(height: 8),
                                Text(
                                  'Chưa có thiết bị nào',
                                  style: TextStyle(color: Colors.grey),
                                ),
                              ],
                            ),
                          ),
                        )
                      else
                        ...gatewayNodes.map(
                          (node) => ListTile(
                            leading: Container(
                              padding: const EdgeInsets.all(8),
                              decoration: BoxDecoration(
                                color: node.stateOn
                                    ? AppConstants.accentColor.withOpacity(0.2)
                                    : Colors.grey.shade100,
                                borderRadius: BorderRadius.circular(8),
                              ),
                              child: Icon(
                                Icons.lightbulb,
                                color: node.stateOn
                                    ? AppConstants.accentColor
                                    : Colors.grey,
                              ),
                            ),
                            title: Text(node.name),
                            subtitle: Text('MAC: ${node.id}'),
                            trailing: Row(
                              mainAxisSize: MainAxisSize.min,
                              children: [
                                Container(
                                  width: 8,
                                  height: 8,
                                  decoration: BoxDecoration(
                                    shape: BoxShape.circle,
                                    color: node.online
                                        ? Colors.green
                                        : Colors.red,
                                  ),
                                ),
                                const SizedBox(width: 8),
                                Text(
                                  node.online ? 'Online' : 'Offline',
                                  style: TextStyle(
                                    fontSize: 12,
                                    color: node.online
                                        ? Colors.green
                                        : Colors.red,
                                  ),
                                ),
                              ],
                            ),
                          ),
                        ),
                    ],
                  ),
                ),
              ),
            ],
          );
        },
      ),
    );
  }

  void _handleMenuAction(String action, BuildContext context) {
    if (action == 'delete') {
      _showDeleteConfirmation(context);
    }
  }

  void _showDeleteConfirmation(BuildContext context) {
    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Xóa Gateway'),
        content: Text(
          'Bạn có chắc muốn xóa gateway "${_gateway.name}"?\n\n'
          '⚠️ Tất cả các thiết bị kết nối với gateway này cũng sẽ bị xóa!',
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
              final success = await context.read<HomeProvider>().deleteGateway(
                _gateway.id,
              );
              if (context.mounted) {
                Navigator.pop(context); // Đóng dialog
                if (success) {
                  Navigator.pop(context); // Quay lại màn hình trước
                  ScaffoldMessenger.of(context).showSnackBar(
                    const SnackBar(content: Text('Đã xóa Gateway')),
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

class _GatewayHeader extends StatelessWidget {
  final GatewayModel gateway;

  const _GatewayHeader({required this.gateway});

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.all(24),
      decoration: BoxDecoration(
        gradient: LinearGradient(
          colors: gateway.online
              ? [Colors.green.shade600, Colors.green.shade400]
              : [Colors.grey.shade600, Colors.grey.shade400],
        ),
        borderRadius: BorderRadius.circular(20),
        boxShadow: [
          BoxShadow(
            color: (gateway.online ? Colors.green : Colors.grey).withOpacity(
              0.3,
            ),
            blurRadius: 15,
            offset: const Offset(0, 6),
          ),
        ],
      ),
      child: Column(
        children: [
          Container(
            padding: const EdgeInsets.all(20),
            decoration: BoxDecoration(
              color: Colors.white.withOpacity(0.2),
              shape: BoxShape.circle,
            ),
            child: const Icon(Icons.router, size: 60, color: Colors.white),
          ),
          const SizedBox(height: 16),
          Text(
            gateway.name,
            style: const TextStyle(
              color: Colors.white,
              fontSize: 24,
              fontWeight: FontWeight.bold,
            ),
          ),
          const SizedBox(height: 8),
          Container(
            padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
            decoration: BoxDecoration(
              color: Colors.white.withOpacity(0.2),
              borderRadius: BorderRadius.circular(20),
            ),
            child: Row(
              mainAxisSize: MainAxisSize.min,
              children: [
                Container(
                  width: 10,
                  height: 10,
                  decoration: BoxDecoration(
                    shape: BoxShape.circle,
                    color: gateway.online ? Colors.greenAccent : Colors.red,
                  ),
                ),
                const SizedBox(width: 8),
                Text(
                  gateway.online ? 'Trực tuyến' : 'Ngoại tuyến',
                  style: const TextStyle(
                    color: Colors.white,
                    fontWeight: FontWeight.w500,
                  ),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

class _DetailCard extends StatelessWidget {
  final GatewayModel gateway;

  const _DetailCard({required this.gateway});

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
                  'Thông tin Gateway',
                  style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
                ),
              ],
            ),
            const SizedBox(height: 16),
            _DetailRow(
              icon: Icons.bluetooth,
              label: 'MAC Address',
              value: gateway.id,
            ),
            const Divider(),
            _DetailRow(
              icon: Icons.home,
              label: 'Home ID',
              value: gateway.homeId,
            ),
          ],
        ),
      ),
    );
  }
}

class _MqttTopicsCard extends StatelessWidget {
  final GatewayModel gateway;

  const _MqttTopicsCard({required this.gateway});

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
                Icon(Icons.message, color: AppConstants.primaryColor),
                SizedBox(width: 8),
                Text(
                  'MQTT Topics',
                  style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
                ),
              ],
            ),
            const SizedBox(height: 16),
            _TopicRow(
              label: 'Subscribe',
              value: gateway.topicSub,
              color: Colors.blue,
            ),
            const SizedBox(height: 12),
            _TopicRow(
              label: 'Publish',
              value: gateway.topicPub,
              color: Colors.green,
            ),
            const SizedBox(height: 12),
            _TopicRow(
              label: 'Alert',
              value: gateway.topicAlert,
              color: Colors.orange,
            ),
          ],
        ),
      ),
    );
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

class _TopicRow extends StatelessWidget {
  final String label;
  final String value;
  final Color color;

  const _TopicRow({
    required this.label,
    required this.value,
    required this.color,
  });

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: color.withOpacity(0.1),
        borderRadius: BorderRadius.circular(8),
        border: Border.all(color: color.withOpacity(0.3)),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              Container(
                padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 2),
                decoration: BoxDecoration(
                  color: color,
                  borderRadius: BorderRadius.circular(4),
                ),
                child: Text(
                  label,
                  style: const TextStyle(
                    color: Colors.white,
                    fontSize: 10,
                    fontWeight: FontWeight.bold,
                  ),
                ),
              ),
            ],
          ),
          const SizedBox(height: 8),
          Text(
            value.isEmpty ? 'Chưa cấu hình' : value,
            style: TextStyle(
              fontFamily: 'monospace',
              fontSize: 13,
              color: value.isEmpty ? Colors.grey : Colors.black87,
            ),
          ),
        ],
      ),
    );
  }
}
