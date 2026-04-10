import 'dart:async';
import 'dart:convert';
import 'package:flutter/material.dart';
import 'package:cloud_firestore/cloud_firestore.dart';
import 'package:mqtt_client/mqtt_client.dart';
import '../constants.dart';
import '../models/home_model.dart';
import '../models/room_model.dart';
import '../models/gateway_model.dart';
import '../models/node_model.dart';
import '../services/home_service.dart';
import '../services/mqtt_service.dart';

class HomeProvider extends ChangeNotifier {
  final HomeService _homeService = HomeService();
  final FirebaseFirestore _firestore = FirebaseFirestore.instance;
  MqttService? _mqttService;

  // State
  List<HomeModel> _homes = [];
  HomeModel? _selectedHome;
  List<RoomModel> _rooms = [];
  List<GatewayModel> _gateways = [];
  List<NodeModel> _nodes = [];
  bool _isLoading = false;
  String? _errorMessage;

  // Realtime subscriptions
  StreamSubscription? _roomsSubscription;
  StreamSubscription? _gatewaysSubscription;
  StreamSubscription? _nodesSubscription;
  StreamSubscription? _mqttMessageSubscription;

  // Subscribed topics để tránh subscribe lại
  final Set<String> _subscribedTopics = {};

  // Getters
  List<HomeModel> get homes => _homes;
  HomeModel? get selectedHome => _selectedHome;
  List<RoomModel> get rooms => _rooms;
  List<GatewayModel> get gateways => _gateways;
  List<NodeModel> get nodes => _nodes;
  bool get isLoading => _isLoading;
  String? get errorMessage => _errorMessage;

  /// Inject MqttService và bắt đầu lắng nghe messages
  void setMqttService(MqttService mqttService) {
    _mqttService = mqttService;
    _setupMqttMessageListener();
  }

  /// Setup listener cho MQTT messages từ gateway
  void _setupMqttMessageListener() {
    _mqttMessageSubscription?.cancel();
    _mqttMessageSubscription = _mqttService?.messageStream.listen((message) {
      _handleMqttMessage(message);
    });
  }

  /// Xử lý message MQTT nhận được từ gateway (Luồng 2)
  void _handleMqttMessage(MqttReceivedMessage<MqttMessage> message) {
    try {
      final payload = message.payload as MqttPublishMessage;
      final payloadString = MqttPublishPayload.bytesToStringAsString(
        payload.payload.message,
      );

      debugPrint('📩 HomeProvider: MQTT message on ${message.topic}');

      final json = jsonDecode(payloadString) as Map<String, dynamic>;
      final name = json['name'] as String?;

      // Xử lý CmdGetData từ gateway - cập nhật trạng thái đèn
      if (name == 'CmdGetData') {
        final address = json['address'] as int?;
        final state = json['state'] as int?;

        if (address != null && state != null) {
          debugPrint(
            '💡 Received state update: address=$address, state=$state',
          );
          _updateNodeStateFromMqtt(address, state == 1);
        }
      }
    } catch (e) {
      debugPrint('❌ Error parsing MQTT message: $e');
    }
  }

  /// Cập nhật trạng thái node từ MQTT message và sync lên Firestore
  Future<void> _updateNodeStateFromMqtt(int meshAddress, bool stateOn) async {
    // Tìm node có meshAddress tương ứng
    final nodeIndex = _nodes.indexWhere((n) => n.meshAddress == meshAddress);

    if (nodeIndex != -1) {
      final node = _nodes[nodeIndex];

      // Chỉ cập nhật nếu state thay đổi
      if (node.stateOn != stateOn) {
        debugPrint(
          '🔄 Updating node ${node.name} (${node.id}): stateOn=$stateOn',
        );

        // Cập nhật local state
        _nodes[nodeIndex] = node.copyWith(stateOn: stateOn);
        notifyListeners();

        // Cập nhật lên Firestore
        try {
          await _firestore
              .collection(AppConstants.colNodes)
              .doc(node.id)
              .update({'state_on': stateOn});
          debugPrint('✅ Firestore updated for node ${node.id}');
        } catch (e) {
          debugPrint('❌ Failed to update Firestore: $e');
        }
      }
    } else {
      debugPrint('⚠️ Node with meshAddress $meshAddress not found');
    }
  }

  /// Subscribe topicPub của tất cả gateways
  void _subscribeGatewayTopics() {
    if (_mqttService == null || !_mqttService!.isConnected) {
      debugPrint('⚠️ MQTT not connected, cannot subscribe gateway topics');
      return;
    }

    for (final gateway in _gateways) {
      final topicPub = gateway.topicPub;
      if (topicPub.isNotEmpty && !_subscribedTopics.contains(topicPub)) {
        debugPrint('📡 Subscribing to gateway topic: $topicPub');
        _mqttService!.subscribe(topicPub);
        _subscribedTopics.add(topicPub);
      }
    }
  }

  /// Dispose subscriptions
  @override
  void dispose() {
    _cancelSubscriptions();
    _mqttMessageSubscription?.cancel();
    super.dispose();
  }

  void _cancelSubscriptions() {
    _roomsSubscription?.cancel();
    _gatewaysSubscription?.cancel();
    _nodesSubscription?.cancel();
  }

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

  /// Load dữ liệu của home đang chọn (rooms, gateways, nodes) với realtime listeners
  Future<void> loadHomeData() async {
    if (_selectedHome == null) return;

    _isLoading = true;
    notifyListeners();

    // Hủy subscriptions cũ
    _cancelSubscriptions();

    try {
      final homeId = _selectedHome!.id;

      // Realtime listener cho rooms
      _roomsSubscription = _firestore
          .collection(AppConstants.colRooms)
          .where('homeId', isEqualTo: homeId)
          .snapshots()
          .listen((snapshot) {
            _rooms = snapshot.docs
                .map((doc) => RoomModel.fromMap(doc.data(), doc.id))
                .toList();
            debugPrint('🔄 Rooms updated: ${_rooms.length} rooms');
            notifyListeners();
          });

      // Realtime listener cho gateways
      _gatewaysSubscription = _firestore
          .collection(AppConstants.colGateways)
          .where('homeId', isEqualTo: homeId)
          .snapshots()
          .listen((snapshot) {
            _gateways = snapshot.docs
                .map((doc) => GatewayModel.fromMap(doc.data(), doc.id))
                .toList();
            debugPrint('🔄 Gateways updated: ${_gateways.length} gateways');

            // Subscribe topicPub của các gateways mới
            _subscribeGatewayTopics();

            notifyListeners();
          });

      // Realtime listener cho nodes (theo homeId thay vì gatewayMac)
      _nodesSubscription = _firestore
          .collection(AppConstants.colNodes)
          .where('homeId', isEqualTo: homeId)
          .snapshots()
          .listen((snapshot) {
            _nodes = snapshot.docs
                .map((doc) => NodeModel.fromMap(doc.data(), doc.id))
                .toList();
            debugPrint('🔄 Nodes updated: ${_nodes.length} nodes');
            notifyListeners();
          });
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
        _cancelSubscriptions(); // Hủy listeners cũ
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

  /// Toggle trạng thái node - Luồng 1: Chỉ gửi MQTT request, không cập nhật UI
  /// UI sẽ được cập nhật khi nhận được CmdGetData từ gateway (Luồng 2)
  void toggleNode(String nodeMac, bool requestedState) {
    // Tìm node
    final node = _nodes.firstWhere(
      (n) => n.id == nodeMac,
      orElse: () => throw Exception('Node not found'),
    );

    // Tìm gateway của node
    final gateway = _gateways.firstWhere(
      (g) => g.id == node.gatewayMac,
      orElse: () => throw Exception('Gateway not found'),
    );

    if (_mqttService == null || !_mqttService!.isConnected) {
      debugPrint('⚠️ MQTT not connected, cannot send command');
      _errorMessage = 'Không có kết nối MQTT';
      notifyListeners();
      return;
    }

    // Tạo message CmdSetData
    final message = {
      'name': 'CmdSetData',
      'value': {'address': node.meshAddress, 'state': requestedState ? 1 : 0},
    };

    // Gửi đến topicSub của gateway
    debugPrint('📤 Sending CmdSetData to ${gateway.topicSub}: $message');
    _mqttService!.publishJson(gateway.topicSub, message);
  }

  /// Toggle tất cả nodes trong room - gửi MQTT cho từng node
  void toggleAllNodesInRoom(String roomId, bool requestedState) {
    final roomNodes = getNodesInRoom(roomId);
    for (final node in roomNodes) {
      toggleNode(node.id, requestedState);
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
