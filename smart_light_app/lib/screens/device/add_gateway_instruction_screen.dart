import 'package:flutter/material.dart';
import '../../constants.dart';
import 'add_gateway_progress_screen.dart';

/// Màn hình hướng dẫn cài đặt Gateway
class AddGatewayInstructionScreen extends StatelessWidget {
  final String homeId;
  final String wifiSsid;
  final String wifiPassword;

  const AddGatewayInstructionScreen({
    super.key,
    required this.homeId,
    required this.wifiSsid,
    required this.wifiPassword,
  });

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Hướng dẫn cài đặt'),
        backgroundColor: AppConstants.primaryColor,
        foregroundColor: Colors.white,
      ),
      body: SingleChildScrollView(
        padding: const EdgeInsets.all(20.0),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            // Header
            Center(
              child: Container(
                padding: const EdgeInsets.all(20),
                decoration: BoxDecoration(
                  color: AppConstants.primaryColor.withOpacity(0.1),
                  shape: BoxShape.circle,
                ),
                child: const Icon(
                  Icons.router,
                  size: 60,
                  color: AppConstants.primaryColor,
                ),
              ),
            ),
            const SizedBox(height: 24),

            const Text(
              'Cài đặt Gateway',
              style: TextStyle(fontSize: 22, fontWeight: FontWeight.bold),
              textAlign: TextAlign.center,
            ),
            const SizedBox(height: 8),
            const Text(
              'Thực hiện các bước sau để thiết lập Gateway',
              style: TextStyle(fontSize: 14, color: Colors.grey),
              textAlign: TextAlign.center,
            ),
            const SizedBox(height: 32),

            // Instruction Steps
            _InstructionStep(
              stepNumber: 1,
              title: 'Lắp đặt thiết bị',
              description:
                  'Lắp đặt Gateway và kết nối nguồn điện cho thiết bị.',
              icon: Icons.power,
            ),
            const SizedBox(height: 16),

            _InstructionStep(
              stepNumber: 2,
              title: 'Kích hoạt chế độ cấu hình',
              description:
                  'Bấm và giữ nút bấm trên Gateway trong khoảng 3 giây cho đến khi đèn báo nhấp nháy.',
              icon: Icons.touch_app,
            ),
            const SizedBox(height: 16),

            _InstructionStep(
              stepNumber: 3,
              title: 'Kết nối WiFi của Gateway',
              description:
                  'Vào cài đặt WiFi trên điện thoại và kết nối với mạng WiFi có tên "GATEWAY_XXXX".',
              icon: Icons.wifi,
            ),
            const SizedBox(height: 16),

            _InstructionStep(
              stepNumber: 4,
              title: 'Tiếp tục cấu hình',
              description:
                  'Sau khi kết nối WiFi thành công, bấm nút "Tiếp tục" bên dưới.',
              icon: Icons.check_circle,
            ),
            const SizedBox(height: 32),

            // Warning note
            Container(
              padding: const EdgeInsets.all(12),
              decoration: BoxDecoration(
                color: Colors.blue.shade50,
                borderRadius: BorderRadius.circular(8),
                border: Border.all(color: Colors.blue.shade200),
              ),
              child: Row(
                children: [
                  Icon(Icons.info_outline, color: Colors.blue.shade700),
                  const SizedBox(width: 12),
                  Expanded(
                    child: Text(
                      'Đảm bảo điện thoại đã kết nối với mạng WiFi "GATEWAY_XXXX" trước khi tiếp tục.',
                      style: TextStyle(
                        fontSize: 12,
                        color: Colors.blue.shade900,
                      ),
                    ),
                  ),
                ],
              ),
            ),
            const SizedBox(height: 32),

            // Continue Button
            SizedBox(
              width: double.infinity,
              child: ElevatedButton(
                onPressed: () {
                  Navigator.push(
                    context,
                    MaterialPageRoute(
                      builder: (_) => AddGatewayProgressScreen(
                        homeId: homeId,
                        wifiSsid: wifiSsid,
                        wifiPassword: wifiPassword,
                      ),
                    ),
                  );
                },
                style: ElevatedButton.styleFrom(
                  backgroundColor: AppConstants.primaryColor,
                  foregroundColor: Colors.white,
                  padding: const EdgeInsets.symmetric(vertical: 16),
                  shape: RoundedRectangleBorder(
                    borderRadius: BorderRadius.circular(12),
                  ),
                ),
                child: const Text(
                  'Tiếp tục',
                  style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold),
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class _InstructionStep extends StatelessWidget {
  final int stepNumber;
  final String title;
  final String description;
  final IconData icon;

  const _InstructionStep({
    required this.stepNumber,
    required this.title,
    required this.description,
    required this.icon,
  });

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: Colors.grey.shade50,
        borderRadius: BorderRadius.circular(12),
        border: Border.all(color: Colors.grey.shade200),
      ),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          // Step number badge
          Container(
            width: 32,
            height: 32,
            decoration: BoxDecoration(
              color: AppConstants.primaryColor,
              shape: BoxShape.circle,
            ),
            child: Center(
              child: Text(
                '$stepNumber',
                style: const TextStyle(
                  color: Colors.white,
                  fontWeight: FontWeight.bold,
                ),
              ),
            ),
          ),
          const SizedBox(width: 12),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  title,
                  style: const TextStyle(
                    fontSize: 16,
                    fontWeight: FontWeight.bold,
                  ),
                ),
                const SizedBox(height: 4),
                Text(
                  description,
                  style: TextStyle(fontSize: 13, color: Colors.grey.shade700),
                ),
              ],
            ),
          ),
          Icon(icon, color: AppConstants.primaryColor, size: 28),
        ],
      ),
    );
  }
}
