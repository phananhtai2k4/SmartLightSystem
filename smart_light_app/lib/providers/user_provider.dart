import 'package:flutter/material.dart';
import 'package:firebase_auth/firebase_auth.dart';
import '../services/auth_service.dart';
import '../services/mqtt_service.dart';

class UserProvider extends ChangeNotifier {
  final AuthService _authService = AuthService();
  final MqttService _mqttService = MqttService();

  bool _isLoading = false;
  String? _errorMessage;
  bool _isMqttConnected = false;

  bool get isLoading => _isLoading;
  String? get errorMessage => _errorMessage;
  User? get user => _authService.currentUser;
  bool get isMqttConnected => _isMqttConnected;
  MqttService get mqttService => _mqttService;

  // Hàm tiện ích để báo loading
  void _setLoading(bool value) {
    _isLoading = value;
    notifyListeners();
  }

  // Xử lý Đăng nhập
  Future<bool> login(String email, String password) async {
    _setLoading(true);
    _errorMessage = null;
    try {
      await _authService.signIn(email: email, password: password);

      // Kết nối MQTT sau khi đăng nhập thành công
      await _connectMqtt();

      _setLoading(false);
      return true; // Thành công
    } catch (e) {
      _errorMessage = e.toString();
      _setLoading(false);
      return false; // Thất bại
    }
  }

  // Kết nối MQTT với clientId là userId
  Future<void> _connectMqtt() async {
    final userId = _authService.currentUser?.uid;
    if (userId != null) {
      // Sử dụng userId làm clientId để đảm bảo unique
      final clientId = 'smart_light_$userId';
      _isMqttConnected = await _mqttService.connect(clientId);

      // Lắng nghe trạng thái kết nối
      _mqttService.connectionStatusStream.listen((state) {
        _isMqttConnected = _mqttService.isConnected;
        notifyListeners();
      });

      notifyListeners();
    }
  }

  // Xử lý Đăng ký
  Future<bool> register(String email, String password, String name) async {
    _setLoading(true);
    _errorMessage = null;
    try {
      await _authService.signUp(email: email, password: password, name: name);

      // Kết nối MQTT sau khi đăng ký thành công
      await _connectMqtt();

      _setLoading(false);
      return true;
    } catch (e) {
      _errorMessage = e.toString();
      _setLoading(false);
      return false;
    }
  }

  // Xử lý Đăng xuất
  Future<void> logout() async {
    // Ngắt kết nối MQTT trước
    _mqttService.disconnect();
    _isMqttConnected = false;

    await _authService.signOut();
    notifyListeners();
  }

  // Xử lý Quên mật khẩu
  Future<bool> resetPassword(String email) async {
    try {
      await _authService.resetPassword(email: email);
      return true;
    } catch (e) {
      _errorMessage = e.toString();
      notifyListeners();
      return false;
    }
  }
}
