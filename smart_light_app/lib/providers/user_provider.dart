import 'package:flutter/material.dart';
import 'package:firebase_auth/firebase_auth.dart';
import '../services/auth_service.dart';

class UserProvider extends ChangeNotifier {
  final AuthService _authService = AuthService();

  bool _isLoading = false;
  String? _errorMessage;

  bool get isLoading => _isLoading;
  String? get errorMessage => _errorMessage;
  User? get user => _authService.currentUser;

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
      _setLoading(false);
      return true; // Thành công
    } catch (e) {
      _errorMessage = e.toString();
      _setLoading(false);
      return false; // Thất bại
    }
  }

  // Xử lý Đăng ký
  Future<bool> register(String email, String password, String name) async {
    _setLoading(true);
    _errorMessage = null;
    try {
      await _authService.signUp(email: email, password: password, name: name);
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
