import 'package:flutter/material.dart';
import 'package:cloud_firestore/cloud_firestore.dart';
import 'package:intl/intl.dart';
import '../../constants.dart';
import '../../models/history_event_model.dart';
import '../../models/node_model.dart';

class DeviceHistoryScreen extends StatelessWidget {
  final NodeModel node;

  const DeviceHistoryScreen({super.key, required this.node});

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Lịch sử hoạt động'),
        backgroundColor: AppConstants.primaryColor,
        foregroundColor: Colors.white,
      ),
      body: StreamBuilder<QuerySnapshot>(
        stream: FirebaseFirestore.instance
            .collection('nodes')
            .doc(node.id)
            .collection('events')
            .orderBy('timestamp', descending: true)
            .snapshots(),
        builder: (context, snapshot) {
          if (snapshot.connectionState == ConnectionState.waiting) {
            return const Center(child: CircularProgressIndicator());
          }

          if (snapshot.hasError) {
            return const Center(child: Text('Đã có lỗi xảy ra.'));
          }

          if (!snapshot.hasData || snapshot.data!.docs.isEmpty) {
            return const Center(child: Text('Chưa có lịch sử hoạt động.'));
          }

          final events = snapshot.data!.docs
              .map((doc) => HistoryEventModel.fromFirestore(doc))
              .toList();

          return ListView.separated(
            padding: const EdgeInsets.all(16),
            itemCount: events.length,
            separatorBuilder: (context, index) => const Divider(),
            itemBuilder: (context, index) {
              final event = events[index];
              return _EventTile(event: event);
            },
          );
        },
      ),
    );
  }
}

class _EventTile extends StatelessWidget {
  final HistoryEventModel event;

  const _EventTile({required this.event});

  @override
  Widget build(BuildContext context) {
    IconData icon;
    Color iconColor;
    String title;

    switch (event.type) {
      case 0:
        icon = Icons.phone_android;
        iconColor = Colors.blue;
        title = event.onOff ? 'Bật qua ứng dụng' : 'Tắt qua ứng dụng';
        break;
      case 1:
        icon = Icons.touch_app;
        iconColor = Colors.orange;
        title = event.onOff ? 'Bật bằng nút cứng' : 'Tắt bằng nút cứng';
        break;
      case 2:
        icon = Icons.warning;
        iconColor = Colors.red;
        title = 'Cảnh báo rò điện';
        break;
      default:
        icon = Icons.help_outline;
        iconColor = Colors.grey;
        title = 'Sự kiện không xác định';
    }

    final formattedTime = DateFormat(
      'HH:mm - dd/MM/yyyy',
    ).format(event.timestamp);

    return ListTile(
      leading: CircleAvatar(
        backgroundColor: iconColor.withOpacity(0.2),
        child: Icon(icon, color: iconColor),
      ),
      title: Text(title, style: const TextStyle(fontWeight: FontWeight.w600)),
      subtitle: Text(formattedTime),
      trailing: event.type != 2
          ? Text(
              event.onOff ? 'ON' : 'OFF',
              style: TextStyle(
                fontWeight: FontWeight.bold,
                color: event.onOff ? Colors.green : Colors.grey,
              ),
            )
          : null,
    );
  }
}
