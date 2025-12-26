import 'package:flutter/material.dart';
import '../models/home_model.dart';
import '../models/room_model.dart';
import '../models/gateway_model.dart';
import '../models/node_model.dart';
import '../services/home_service.dart';

class HomeProvider extends ChangeNotifier {
  final HomeService _homeService = HomeService();

  // State
  List<HomeModel> _homes = [];
  HomeModel? _selectedHome;
  List<RoomModel> _rooms = [];
  List<GatewayModel> _gateways = [];
  List<NodeModel> _nodes = [];
  bool _isLoading = false;
  String? _errorMessage;

  // Getters
  List<HomeModel> get homes => _homes;
  HomeModel? get selectedHome => _selectedHome;
  List<RoomModel> get rooms => _rooms;
  List<GatewayModel> get gateways => _gateways;
  List<NodeModel> get nodes => _nodes;
  bool get isLoading => _isLoading;
  String? get errorMessage => _errorMessage;

  // ==================== HOMES ====================

  /// Load danh sách homes của user
  Future<void> loadHomes() async {
    _isLoading = true;
    _errorMessage = null;
    notifyListeners();

    try {
      _homes = await _homeService.getUserHomes();

      // Nếu chưa có home nào được chọn, chọn home đầu tiên
      if (_selectedHome == null && _homes.isNotEmpty) {
        _selectedHome = _homes.first;
        await loadHomeData();
      }
    } catch (e) {
      _errorMessage = e.toString();
    }

    _isLoading = false;
    notifyListeners();
  }

  /// Chọn home
  Future<void> selectHome(HomeModel home) async {
    _selectedHome = home;
    notifyListeners();
    await loadHomeData();
  }

  /// Load dữ liệu của home đang chọn (rooms, gateways, nodes)
  Future<void> loadHomeData() async {
    if (_selectedHome == null) return;

    _isLoading = true;
    notifyListeners();

    try {
      _rooms = await _homeService.getRooms(_selectedHome!.id);
      _gateways = await _homeService.getGateways(_selectedHome!.id);

      // Load tất cả nodes của home
      List<NodeModel> allNodes = [];
      for (var gateway in _gateways) {
        final nodes = await _homeService.getNodesByGateway(gateway.id);
        allNodes.addAll(nodes);
      }
      _nodes = allNodes;
    } catch (e) {
      _errorMessage = e.toString();
    }

    _isLoading = false;
    notifyListeners();
  }

  /// Tạo home mới
  Future<bool> createHome(String name, String description) async {
    try {
      final newHome = await _homeService.createHome(name, description);
      _homes.add(newHome);

      // Nếu đây là home đầu tiên, tự động chọn nó
      if (_selectedHome == null) {
        _selectedHome = newHome;
      }

      notifyListeners();
      return true;
    } catch (e) {
      _errorMessage = e.toString();
      notifyListeners();
      return false;
    }
  }

  /// Xóa home
  Future<bool> deleteHome(String homeId) async {
    try {
      await _homeService.deleteHome(homeId);
      _homes.removeWhere((h) => h.id == homeId);

      // Nếu xóa home đang chọn, chọn home khác
      if (_selectedHome?.id == homeId) {
        _selectedHome = _homes.isNotEmpty ? _homes.first : null;
        if (_selectedHome != null) {
          await loadHomeData();
        } else {
          _rooms = [];
          _gateways = [];
          _nodes = [];
        }
      }

      notifyListeners();
      return true;
    } catch (e) {
      _errorMessage = e.toString();
      notifyListeners();
      return false;
    }
  }

  // ==================== ROOMS ====================

  /// Tạo room mới
  Future<bool> createRoom(String name, int groupAddress) async {
    if (_selectedHome == null) return false;

    try {
      final newRoom = await _homeService.createRoom(
        _selectedHome!.id,
        name,
        groupAddress,
      );
      _rooms.add(newRoom);
      notifyListeners();
      return true;
    } catch (e) {
      _errorMessage = e.toString();
      notifyListeners();
      return false;
    }
  }

  /// Cập nhật tên room
  Future<bool> updateRoom(String roomId, String name) async {
    try {
      await _homeService.updateRoom(roomId, name);
      final index = _rooms.indexWhere((r) => r.id == roomId);
      if (index != -1) {
        _rooms[index] = RoomModel(
          id: roomId,
          homeId: _rooms[index].homeId,
          name: name,
          groupAddress: _rooms[index].groupAddress,
        );
      }
      notifyListeners();
      return true;
    } catch (e) {
      _errorMessage = e.toString();
      notifyListeners();
      return false;
    }
  }

  /// Xóa room
  Future<bool> deleteRoom(String roomId) async {
    try {
      await _homeService.deleteRoom(roomId);
      _rooms.removeWhere((r) => r.id == roomId);
      notifyListeners();
      return true;
    } catch (e) {
      _errorMessage = e.toString();
      notifyListeners();
      return false;
    }
  }

  /// Bật/tắt tất cả đèn trong room
  Future<void> toggleAllNodesInRoom(String roomId, bool stateOn) async {
    try {
      await _homeService.toggleAllNodesInRoom(roomId, stateOn);

      // Cập nhật state local
      for (int i = 0; i < _nodes.length; i++) {
        if (_nodes[i].roomId == roomId) {
          _nodes[i] = _nodes[i].copyWith(stateOn: stateOn);
        }
      }
      notifyListeners();
    } catch (e) {
      _errorMessage = e.toString();
      notifyListeners();
    }
  }

  // ==================== GATEWAYS ====================

  /// Xóa gateway
  Future<bool> deleteGateway(String gatewayMac) async {
    try {
      await _homeService.deleteGateway(gatewayMac);
      _gateways.removeWhere((g) => g.id == gatewayMac);
      _nodes.removeWhere((n) => n.gatewayMac == gatewayMac);
      notifyListeners();
      return true;
    } catch (e) {
      _errorMessage = e.toString();
      notifyListeners();
      return false;
    }
  }

  // ==================== NODES ====================

  /// Lấy nodes của một room
  List<NodeModel> getNodesInRoom(String roomId) {
    return _nodes.where((n) => n.roomId == roomId).toList();
  }

  /// Toggle trạng thái node
  Future<void> toggleNode(String nodeMac, bool stateOn) async {
    try {
      await _homeService.toggleNode(nodeMac, stateOn);

      // Cập nhật state local
      final index = _nodes.indexWhere((n) => n.id == nodeMac);
      if (index != -1) {
        _nodes[index] = _nodes[index].copyWith(stateOn: stateOn);
        notifyListeners();
      }
    } catch (e) {
      _errorMessage = e.toString();
      notifyListeners();
    }
  }

  /// Cập nhật tên node
  Future<bool> updateNodeName(String nodeMac, String name) async {
    try {
      await _homeService.updateNodeName(nodeMac, name);

      final index = _nodes.indexWhere((n) => n.id == nodeMac);
      if (index != -1) {
        _nodes[index] = _nodes[index].copyWith(name: name);
        notifyListeners();
      }
      return true;
    } catch (e) {
      _errorMessage = e.toString();
      notifyListeners();
      return false;
    }
  }

  /// Xóa node
  Future<bool> deleteNode(String nodeMac) async {
    try {
      await _homeService.deleteNode(nodeMac);
      _nodes.removeWhere((n) => n.id == nodeMac);
      notifyListeners();
      return true;
    } catch (e) {
      _errorMessage = e.toString();
      notifyListeners();
      return false;
    }
  }

  /// Kiểm tra xem tất cả nodes trong room có đang bật không
  bool areAllNodesOnInRoom(String roomId) {
    final roomNodes = getNodesInRoom(roomId);
    if (roomNodes.isEmpty) return false;
    return roomNodes.every((n) => n.stateOn);
  }

  /// Kiểm tra xem có node nào trong room đang bật không
  bool isAnyNodeOnInRoom(String roomId) {
    final roomNodes = getNodesInRoom(roomId);
    return roomNodes.any((n) => n.stateOn);
  }

  /// Clear error
  void clearError() {
    _errorMessage = null;
    notifyListeners();
  }

  /// Refresh tất cả data
  Future<void> refresh() async {
    await loadHomes();
  }
}
