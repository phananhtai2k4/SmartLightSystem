import 'package:flutter/material.dart';
import '../../constants.dart';
import 'add_gateway_wifi_screen.dart';
import 'add_light_instruction_screen.dart';

/// Màn hình chọn loại thiết bị cần thêm
class AddDeviceScreen extends StatelessWidget {
  final String homeId;

  const AddDeviceScreen({super.key, required this.homeId});

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Thêm thiết bị'),
        backgroundColor: AppConstants.primaryColor,
        foregroundColor: Colors.white,
      ),
      body: Padding(
        padding: const EdgeInsets.all(20.0),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text(
              'Chọn loại thiết bị',
              style: TextStyle(fontSize: 24, fontWeight: FontWeight.bold),
            ),
            const SizedBox(height: 8),
            const Text(
              'Chọn loại thiết bị bạn muốn thêm vào nhà',
              style: TextStyle(fontSize: 14, color: Colors.grey),
            ),
            const SizedBox(height: 30),

            // Gateway Card
            _DeviceTypeCard(
              icon: Icons.router,
              title: 'Gateway',
              description: 'Thiết bị trung tâm kết nối các đèn trong mạng mesh',
              onTap: () {
                Navigator.push(
                  context,
                  MaterialPageRoute(
                    builder: (_) => AddGatewayWifiScreen(homeId: homeId),
                  ),
                );
              },
            ),

            const SizedBox(height: 16),

            // Light Card
            _DeviceTypeCard(
              icon: Icons.lightbulb,
              title: 'Đèn thông minh',
              description: 'Đèn LED có thể điều khiển qua mạng mesh',
              onTap: () {
                Navigator.push(
                  context,
                  MaterialPageRoute(
                    builder: (_) => AddLightInstructionScreen(homeId: homeId),
                  ),
                );
              },
            ),
          ],
        ),
      ),
    );
  }
}

class _DeviceTypeCard extends StatelessWidget {
  final IconData icon;
  final String title;
  final String description;
  final VoidCallback onTap;
  final bool isComingSoon;

  const _DeviceTypeCard({
    required this.icon,
    required this.title,
    required this.description,
    required this.onTap,
    this.isComingSoon = false,
  });

  @override
  Widget build(BuildContext context) {
    return Card(
      elevation: 2,
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(12),
        side: BorderSide(
          color: isComingSoon
              ? Colors.grey.shade300
              : AppConstants.primaryColor.withOpacity(0.3),
        ),
      ),
      child: InkWell(
        onTap: onTap,
        borderRadius: BorderRadius.circular(12),
        child: Padding(
          padding: const EdgeInsets.all(20.0),
          child: Row(
            children: [
              Container(
                padding: const EdgeInsets.all(12),
                decoration: BoxDecoration(
                  color: isComingSoon
                      ? Colors.grey.shade200
                      : AppConstants.primaryColor.withOpacity(0.1),
                  borderRadius: BorderRadius.circular(12),
                ),
                child: Icon(
                  icon,
                  size: 40,
                  color: isComingSoon ? Colors.grey : AppConstants.primaryColor,
                ),
              ),
              const SizedBox(width: 16),
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Row(
                      children: [
                        Text(
                          title,
                          style: TextStyle(
                            fontSize: 18,
                            fontWeight: FontWeight.bold,
                            color: isComingSoon ? Colors.grey : Colors.black87,
                          ),
                        ),
                        if (isComingSoon) ...[
                          const SizedBox(width: 8),
                          Container(
                            padding: const EdgeInsets.symmetric(
                              horizontal: 8,
                              vertical: 2,
                            ),
                            decoration: BoxDecoration(
                              color: Colors.orange.shade100,
                              borderRadius: BorderRadius.circular(8),
                            ),
                            child: const Text(
                              'Sắp ra mắt',
                              style: TextStyle(
                                fontSize: 10,
                                color: Colors.orange,
                                fontWeight: FontWeight.bold,
                              ),
                            ),
                          ),
                        ],
                      ],
                    ),
                    const SizedBox(height: 4),
                    Text(
                      description,
                      style: TextStyle(
                        fontSize: 13,
                        color: isComingSoon
                            ? Colors.grey.shade400
                            : Colors.grey.shade600,
                      ),
                    ),
                  ],
                ),
              ),
              Icon(
                Icons.chevron_right,
                color: isComingSoon ? Colors.grey.shade300 : Colors.grey,
              ),
            ],
          ),
        ),
      ),
    );
  }
}
