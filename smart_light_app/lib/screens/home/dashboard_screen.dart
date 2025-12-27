import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../../constants.dart';
import '../../providers/home_provider.dart';
import '../../models/gateway_model.dart';
import '../../models/node_model.dart';
import 'device_detail_screen.dart';
import 'gateway_detail_screen.dart';
import '../device/add_device_screen.dart';

class DashboardScreen extends StatelessWidget {
  const DashboardScreen({super.key});

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Tất cả thiết bị'),
        centerTitle: true,
        backgroundColor: AppConstants.primaryColor,
        foregroundColor: Colors.white,
        actions: [
          // Icon thêm thiết bị
          Consumer<HomeProvider>(
            builder: (context, homeProvider, _) {
              return IconButton(
                icon: const Icon(Icons.add_circle_outline),
                onPressed: () {
                  if (homeProvider.selectedHome == null) {
                    ScaffoldMessenger.of(context).showSnackBar(
                      const SnackBar(
                        content: Text(
                          'Vui lòng tạo nhà trước khi thêm thiết bị',
                        ),
                        backgroundColor: Colors.orange,
                      ),
                    );
                    return;
                  }
                  Navigator.push(
                    context,
                    MaterialPageRoute(
                      builder: (_) => AddDeviceScreen(
                        homeId: homeProvider.selectedHome!.id,
                      ),
                    ),
                  );
                },
              );
            },
          ),
        ],
      ),
      body: Consumer<HomeProvider>(
        builder: (context, homeProvider, child) {
          if (homeProvider.isLoading) {
            return const Center(child: CircularProgressIndicator());
          }

          if (homeProvider.selectedHome == null) {
            return const Center(
              child: Column(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  Icon(Icons.home_outlined, size: 80, color: Colors.grey),
                  SizedBox(height: 16),
                  Text(
                    'Chưa có nhà nào',
                    style: TextStyle(fontSize: 18, color: Colors.grey),
                  ),
                  SizedBox(height: 8),
                  Text(
                    'Vui lòng tạo nhà ở tab Phòng',
                    style: TextStyle(color: Colors.grey),
                  ),
                ],
              ),
            );
          }

          final gateways = homeProvider.gateways;
          final nodes = homeProvider.nodes;

          if (gateways.isEmpty && nodes.isEmpty) {
            return const Center(
              child: Column(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  Icon(Icons.devices_other, size: 80, color: Colors.grey),
                  SizedBox(height: 16),
                  Text(
                    'Chưa có thiết bị nào',
                    style: TextStyle(fontSize: 18, color: Colors.grey),
                  ),
                ],
              ),
            );
          }

          return RefreshIndicator(
            onRefresh: () => homeProvider.refresh(),
            child: ListView(
              padding: const EdgeInsets.all(16),
              children: [
                // Header nhà hiện tại
                Container(
                  padding: const EdgeInsets.all(16),
                  decoration: BoxDecoration(
                    gradient: LinearGradient(
                      colors: [
                        AppConstants.primaryColor,
                        AppConstants.primaryColor.withOpacity(0.7),
                      ],
                    ),
                    borderRadius: BorderRadius.circular(12),
                  ),
                  child: Row(
                    children: [
                      const Icon(Icons.home, color: Colors.white, size: 32),
                      const SizedBox(width: 12),
                      Expanded(
                        child: Column(
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: [
                            Text(
                              homeProvider.selectedHome!.name,
                              style: const TextStyle(
                                color: Colors.white,
                                fontSize: 18,
                                fontWeight: FontWeight.bold,
                              ),
                            ),
                            Text(
                              '${gateways.length} Gateway • ${nodes.length} Đèn',
                              style: const TextStyle(color: Colors.white70),
                            ),
                          ],
                        ),
                      ),
                    ],
                  ),
                ),

                const SizedBox(height: 24),

                // Section Gateways
                if (gateways.isNotEmpty) ...[
                  const _SectionHeader(icon: Icons.router, title: 'Gateways'),
                  const SizedBox(height: 12),
                  ...gateways.map((gateway) => _GatewayCard(gateway: gateway)),
                  const SizedBox(height: 24),
                ],

                // Section Đèn
                if (nodes.isNotEmpty) ...[
                  const _SectionHeader(
                    icon: Icons.lightbulb_outline,
                    title: 'Đèn',
                  ),
                  const SizedBox(height: 12),
                  GridView.builder(
                    shrinkWrap: true,
                    physics: const NeverScrollableScrollPhysics(),
                    gridDelegate:
                        const SliverGridDelegateWithFixedCrossAxisCount(
                          crossAxisCount: 2,
                          childAspectRatio: 1.1,
                          crossAxisSpacing: 12,
                          mainAxisSpacing: 12,
                        ),
                    itemCount: nodes.length,
                    itemBuilder: (context, index) {
                      return _NodeCard(node: nodes[index]);
                    },
                  ),
                ],
              ],
            ),
          );
        },
      ),
    );
  }
}

class _SectionHeader extends StatelessWidget {
  final IconData icon;
  final String title;

  const _SectionHeader({required this.icon, required this.title});

  @override
  Widget build(BuildContext context) {
    return Row(
      children: [
        Icon(icon, color: AppConstants.primaryColor),
        const SizedBox(width: 8),
        Text(
          title,
          style: const TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
        ),
      ],
    );
  }
}

class _GatewayCard extends StatelessWidget {
  final GatewayModel gateway;

  const _GatewayCard({required this.gateway});

  @override
  Widget build(BuildContext context) {
    return Card(
      margin: const EdgeInsets.only(bottom: 12),
      child: ListTile(
        leading: Container(
          padding: const EdgeInsets.all(8),
          decoration: BoxDecoration(
            color: gateway.online
                ? Colors.green.withOpacity(0.1)
                : Colors.grey.withOpacity(0.1),
            borderRadius: BorderRadius.circular(8),
          ),
          child: Icon(
            Icons.router,
            color: gateway.online ? Colors.green : Colors.grey,
          ),
        ),
        title: Text(
          gateway.name,
          style: const TextStyle(fontWeight: FontWeight.w600),
        ),
        subtitle: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text('MAC: ${gateway.id}', style: const TextStyle(fontSize: 12)),
            Row(
              children: [
                Container(
                  width: 8,
                  height: 8,
                  decoration: BoxDecoration(
                    shape: BoxShape.circle,
                    color: gateway.online ? Colors.green : Colors.grey,
                  ),
                ),
                const SizedBox(width: 4),
                Text(
                  gateway.online ? 'Trực tuyến' : 'Ngoại tuyến',
                  style: TextStyle(
                    fontSize: 12,
                    color: gateway.online ? Colors.green : Colors.grey,
                  ),
                ),
              ],
            ),
          ],
        ),
        trailing: const Icon(Icons.chevron_right),
        onTap: () {
          Navigator.push(
            context,
            MaterialPageRoute(
              builder: (_) => GatewayDetailScreen(gateway: gateway),
            ),
          );
        },
      ),
    );
  }
}

class _NodeCard extends StatelessWidget {
  final NodeModel node;

  const _NodeCard({required this.node});

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
                // Trạng thái online
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
                    node.online ? 'ON' : 'OFF',
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
