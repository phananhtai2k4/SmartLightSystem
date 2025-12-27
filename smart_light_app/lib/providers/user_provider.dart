import 'dart:async';
import 'package:flutter/foundation.dart';
import 'package:firebase_auth/firebase_auth.dart';
import 'package:connectivity_plus/connectivity_plus.dart';
import 'package:mqtt_client/mqtt_client.dart';
import '../services/auth_service.dart';
import '../services/mqtt_service.dart';

class UserProvider extends ChangeNotifier {
  final AuthService _authService = AuthService();
  final MqttService _mqttService = MqttService();
  final Connectivity _connectivity = Connectivity();

  bool _isLoading = false;
  String? _errorMessage;
  bool _isMqttConnected = false;
  StreamSubscription<List<ConnectivityResult>>? _connectivitySubscription;
  StreamSubscription<User?>? _authSubscription;

  bool get isLoading => _isLoading;
  String? get errorMessage => _errorMessage;
  User? get user => _authService.currentUser;
  bool get isMqttConnected => _isMqttConnected;
  MqttService get mqttService => _mqttService;

  UserProvider() {
    print('🚀 UserProvider: Initializing...');
    // Lắng nghe sự kiện thay đổi kết nối mạng
    _listenToConnectivityChanges();
    // Lắng nghe auth state changes (quan trọng: Firebase cần thời gian restore session)
    _listenToAuthChanges();
  }

  /// Lắng nghe sự kiện thay đổi auth (login/logout)
  void _listenToAuthChanges() {
    _authSubscription = FirebaseAuth.instance.authStateChanges().listen((user) {
      print('🔐 UserProvider: Auth state changed - user: ${user?.uid}');
      if (user != null && !_isMqttConnected) {
        // User đã đăng nhập, kết nối MQTT
        _initMqttIfLoggedIn();
      } else if (user == null && _isMqttConnected) {
        // User đăng xuất, ngắt MQTT
        _mqttService.disconnect();
        _isMqttConnected = false;
        notifyListeners();
      }
    });
  }

  /// Lắng nghe sự kiện thay đổi kết nối WiFi
  void _listenToConnectivityChanges() {
    _connectivitySubscription = _connectivity.onConnectivityChanged.listen((
      results,
    ) {
      print('📶 Connectivity changed: $results');

      // Kiểm tra nếu có kết nối WiFi hoặc Mobile
      final hasConnection =
          results.contains(ConnectivityResult.wifi) ||
          results.contains(ConnectivityResult.mobile) ||
          results.contains(ConnectivityResult.ethernet);

      if (hasConnection &&
          _authService.currentUser != null &&
          !_isMqttConnected) {
        print('📶 Network connected & user logged in -> Connecting MQTT...');
        _connectMqtt();
      }
    });
  }

  /// Kiểm tra và kết nối MQTT nếu user đã đăng nhập
  Future<void> _initMqttIfLoggedIn() async {
    if (_authService.currentUser != null) {
      // Kiểm tra kết nối mạng trước
      final connectivityResult = await _connectivity.checkConnectivity();
      final hasConnection =
          connectivityResult.contains(ConnectivityResult.wifi) ||
          connectivityResult.contains(ConnectivityResult.mobile) ||
          connectivityResult.contains(ConnectivityResult.ethernet);

      if (hasConnection) {
        print(
          '🔌 UserProvider: User logged in & network available -> Connecting MQTT...',
        );
        await _connectMqtt();
      } else {
        print(
          '⚠️ UserProvider: User logged in but no network, waiting for connection...',
        );
      }
    }
  }

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
  bool _isConnectingMqtt = false; // Guard để tránh gọi connect nhiều lần
  StreamSubscription<MqttConnectionState>? _mqttStatusSubscription;

  Future<void> _connectMqtt() async {
    // Guard: Nếu đang connecting thì skip
    if (_isConnectingMqtt) {
      print('⚠️ MQTT: Already connecting, skipping...');
      return;
    }

    // Nếu đã connected thì chỉ cần update state
    if (_mqttService.isConnected) {
      print('✅ MQTT: Already connected');
      _isMqttConnected = true;
      notifyListeners();
      return;
    }

    final userId = _authService.currentUser?.uid;
    if (userId != null) {
      _isConnectingMqtt = true;

      // Sử dụng userId làm clientId để đảm bảo unique
      final clientId = 'smart_light_$userId';
      _isMqttConnected = await _mqttService.connect(clientId);

      _isConnectingMqtt = false;

      // Chỉ tạo subscription 1 lần
      _mqttStatusSubscription ??= _mqttService.connectionStatusStream.listen((
        state,
      ) {
        final wasConnected = _isMqttConnected;
        _isMqttConnected = _mqttService.isConnected;
        if (wasConnected != _isMqttConnected) {
          print('📡 MQTT status changed: $_isMqttConnected');
          notifyListeners();
        }
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

  /// Dispose resources
  @override
  void dispose() {
    _connectivitySubscription?.cancel();
    _authSubscription?.cancel();
    _mqttStatusSubscription?.cancel();
    _mqttService.dispose();
    super.dispose();
  }
}
