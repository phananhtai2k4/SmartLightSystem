import 'package:flutter/material.dart';

class AppConstants {
  // === 1. MQTT CONFIG (Lấy từ HiveMQ Cluster của bạn) ===
  // Bạn vào HiveMQ Cloud -> Clusters -> Overview để lấy thông tin này
  static const String mqttHost = 'c8xxxxxxxx.s1.eu.hivemq.cloud'; // Ví dụ URL
  static const int mqttPort = 8883; // Cổng SSL thường là 8883
  static const String mqttUser =
      'your_username'; // User bạn tạo trong HiveMQ Access Management
  static const String mqttPass = 'your_password';

  // === 2. FIRESTORE COLLECTION NAMES (Khớp với sơ đồ DB) ===
  static const String colUsers = 'users';
  static const String colHomes = 'homes';
  static const String colGateways = 'gateways';
  static const String colRooms = 'rooms';
  static const String colNodes = 'nodes';

  // === 3. UI COLORS (Màu sắc chủ đạo) ===
  static const Color primaryColor = Color(0xFF2196F3); // Xanh dương
  static const Color accentColor = Color(0xFFFFC107); // Vàng cam
  static const Color bgColor = Color(0xFFF5F5F5); // Xám nhạt
}
