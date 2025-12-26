import 'package:flutter/material.dart';

class AppConstants {
  // === 1. MQTT CONFIG (HiveMQ Cloud) ===
  static const String mqttHost =
      '6abef4b39f794ca5bf9ae3fd100385e6.s1.eu.hivemq.cloud';
  static const int mqttPort = 8883; // Cổng SSL/TLS
  static const String mqttUser = 'taitumtt04';
  static const String mqttPass = 'Taitum26102004';

  // === 2. FIRESTORE COLLECTION NAMES (Khớp với sơ đồ DB) ===
  static const String colUsers = 'users';
  static const String colHomes = 'homes';
  static const String colHomeMembers = 'home_members';
  static const String colGateways = 'gateways';
  static const String colRooms = 'rooms';
  static const String colNodes = 'nodes';

  // === 3. UI COLORS (Màu sắc chủ đạo) ===
  static const Color primaryColor = Color(0xFF2196F3); // Xanh dương
  static const Color accentColor = Color(0xFFFFC107); // Vàng cam
  static const Color bgColor = Color(0xFFF5F5F5); // Xám nhạt
}
