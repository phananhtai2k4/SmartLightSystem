import 'package:cloud_firestore/cloud_firestore.dart';
import 'package:firebase_auth/firebase_auth.dart';
import '../constants.dart';
import '../models/home_model.dart';
import '../models/room_model.dart';
import '../models/gateway_model.dart';
import '../models/node_model.dart';

class HomeService {
  final FirebaseFirestore _firestore = FirebaseFirestore.instance;
  final FirebaseAuth _auth = FirebaseAuth.instance;

  String? get _userId => _auth.currentUser?.uid;

  // ==================== HOMES ====================

  /// Lấy danh sách homes mà user là thành viên (owner hoặc member)
  Future<List<HomeModel>> getUserHomes() async {
    if (_userId == null) return [];

    // Lấy tất cả home_members của user hiện tại
    final memberDocs = await _firestore
        .collection(AppConstants.colHomeMembers)
        .where('userId', isEqualTo: _userId)
        .get();

    if (memberDocs.docs.isEmpty) return [];

    // Lấy danh sách homeIds
    final homeIds = memberDocs.docs
        .map((doc) => doc.data()['homeId'] as String)
        .toList();

    // Lấy thông tin chi tiết của các homes
    List<HomeModel> homes = [];
    for (String homeId in homeIds) {
      final homeDoc = await _firestore
          .collection(AppConstants.colHomes)
          .doc(homeId)
          .get();
      if (homeDoc.exists) {
        homes.add(HomeModel.fromMap(homeDoc.data()!, homeDoc.id));
      }
    }

    return homes;
  }

  /// Tạo home mới
  Future<HomeModel> createHome(String name, String description) async {
    if (_userId == null) throw Exception('Chưa đăng nhập');

    // Tạo homeId trước bằng doc()
    final homeRef = _firestore.collection(AppConstants.colHomes).doc();
    final homeId = homeRef.id;

    // Tạo document home với homeId trong data
    await homeRef.set({
      'homeId': homeId, // homeId == documentId
      'name': name,
      'description': description,
    });

    // Tạo document ID cho home_members theo format userId_homeId
    final memberDocId = '${_userId}_$homeId';

    // Thêm user hiện tại làm owner
    await _firestore
        .collection(AppConstants.colHomeMembers)
        .doc(memberDocId)
        .set({
          'id': memberDocId, // id == documentId
          'homeId': homeId,
          'userId': _userId,
          'role': 'owner',
        });

    return HomeModel(id: homeId, name: name, description: description);
  }

  /// Cập nhật thông tin home
  Future<void> updateHome(
    String homeId,
    String name,
    String description,
  ) async {
    await _firestore.collection(AppConstants.colHomes).doc(homeId).update({
      'homeId': homeId, // Đảm bảo homeId luôn có
      'name': name,
      'description': description,
    });
  }

  /// Xóa home
  Future<void> deleteHome(String homeId) async {
    // Xóa tất cả members
    final members = await _firestore
        .collection(AppConstants.colHomeMembers)
        .where('homeId', isEqualTo: homeId)
        .get();
    for (var doc in members.docs) {
      await doc.reference.delete();
    }

    // Xóa tất cả rooms
    final rooms = await _firestore
        .collection(AppConstants.colRooms)
        .where('homeId', isEqualTo: homeId)
        .get();
    for (var doc in rooms.docs) {
      await doc.reference.delete();
    }

    // Xóa tất cả gateways
    final gateways = await _firestore
        .collection(AppConstants.colGateways)
        .where('homeId', isEqualTo: homeId)
        .get();
    for (var doc in gateways.docs) {
      await doc.reference.delete();
    }

    // Xóa home
    await _firestore.collection(AppConstants.colHomes).doc(homeId).delete();
  }

  // ==================== ROOMS ====================

  /// Lấy danh sách rooms của một home
  Stream<List<RoomModel>> getRoomsStream(String homeId) {
    return _firestore
        .collection(AppConstants.colRooms)
        .where('homeId', isEqualTo: homeId)
        .snapshots()
        .map(
          (snapshot) => snapshot.docs
              .map((doc) => RoomModel.fromMap(doc.data(), doc.id))
              .toList(),
        );
  }

  /// Lấy danh sách rooms (Future)
  Future<List<RoomModel>> getRooms(String homeId) async {
    final snapshot = await _firestore
        .collection(AppConstants.colRooms)
        .where('homeId', isEqualTo: homeId)
        .get();
    return snapshot.docs
        .map((doc) => RoomModel.fromMap(doc.data(), doc.id))
        .toList();
  }

  /// Tạo room mới
  Future<RoomModel> createRoom(
    String homeId,
    String name,
    int groupAddress,
  ) async {
    // Tạo roomId trước bằng doc()
    final roomRef = _firestore.collection(AppConstants.colRooms).doc();
    final roomId = roomRef.id;

    await roomRef.set({
      'roomId': roomId, // roomId == documentId
      'homeId': homeId,
      'name': name,
      'groupAddress': groupAddress,
    });

    return RoomModel(
      id: roomId,
      homeId: homeId,
      name: name,
      groupAddress: groupAddress,
    );
  }

  /// Cập nhật room
  Future<void> updateRoom(String roomId, String name) async {
    await _firestore.collection(AppConstants.colRooms).doc(roomId).update({
      'roomId': roomId, // Đảm bảo roomId luôn có
      'name': name,
    });
  }

  /// Xóa room
  Future<void> deleteRoom(String roomId) async {
    await _firestore.collection(AppConstants.colRooms).doc(roomId).delete();
  }

  // ==================== GATEWAYS ====================

  /// Lấy danh sách gateways của một home (Stream realtime)
  Stream<List<GatewayModel>> getGatewaysStream(String homeId) {
    return _firestore
        .collection(AppConstants.colGateways)
        .where('homeId', isEqualTo: homeId)
        .snapshots()
        .map(
          (snapshot) => snapshot.docs
              .map((doc) => GatewayModel.fromMap(doc.data(), doc.id))
              .toList(),
        );
  }

  /// Lấy danh sách gateways (Future)
  Future<List<GatewayModel>> getGateways(String homeId) async {
    final snapshot = await _firestore
        .collection(AppConstants.colGateways)
        .where('homeId', isEqualTo: homeId)
        .get();
    return snapshot.docs
        .map((doc) => GatewayModel.fromMap(doc.data(), doc.id))
        .toList();
  }

  /// Xóa gateway
  Future<void> deleteGateway(String gatewayMac) async {
    // Xóa tất cả nodes thuộc gateway này
    final nodes = await _firestore
        .collection(AppConstants.colNodes)
        .where('gatewayMac', isEqualTo: gatewayMac)
        .get();
    for (var doc in nodes.docs) {
      await doc.reference.delete();
    }

    await _firestore
        .collection(AppConstants.colGateways)
        .doc(gatewayMac)
        .delete();
  }

  // ==================== NODES ====================

  /// Lấy danh sách nodes của một room (Stream realtime)
  Stream<List<NodeModel>> getNodesByRoomStream(String roomId) {
    return _firestore
        .collection(AppConstants.colNodes)
        .where('roomId', isEqualTo: roomId)
        .snapshots()
        .map(
          (snapshot) => snapshot.docs
              .map((doc) => NodeModel.fromMap(doc.data(), doc.id))
              .toList(),
        );
  }

  /// Lấy tất cả nodes của một home (thông qua gateway)
  Stream<List<NodeModel>> getNodesByHomeStream(String homeId) {
    // Đầu tiên lấy tất cả gateways của home
    return _firestore
        .collection(AppConstants.colGateways)
        .where('homeId', isEqualTo: homeId)
        .snapshots()
        .asyncMap((gatewaySnapshot) async {
          final gatewayMacs = gatewaySnapshot.docs
              .map((doc) => doc.id)
              .toList();

          if (gatewayMacs.isEmpty) return <NodeModel>[];

          // Lấy tất cả nodes thuộc các gateways
          List<NodeModel> allNodes = [];
          for (String mac in gatewayMacs) {
            final nodesSnapshot = await _firestore
                .collection(AppConstants.colNodes)
                .where('gatewayMac', isEqualTo: mac)
                .get();
            allNodes.addAll(
              nodesSnapshot.docs.map(
                (doc) => NodeModel.fromMap(doc.data(), doc.id),
              ),
            );
          }
          return allNodes;
        });
  }

  /// Lấy nodes (Future)
  Future<List<NodeModel>> getNodesByRoom(String roomId) async {
    final snapshot = await _firestore
        .collection(AppConstants.colNodes)
        .where('roomId', isEqualTo: roomId)
        .get();
    return snapshot.docs
        .map((doc) => NodeModel.fromMap(doc.data(), doc.id))
        .toList();
  }

  /// Cập nhật trạng thái bật/tắt của node
  Future<void> toggleNode(String nodeMac, bool stateOn) async {
    await _firestore.collection(AppConstants.colNodes).doc(nodeMac).update({
      'nodeMac': nodeMac, // Đảm bảo nodeMac luôn có
      'state_on': stateOn,
    });
  }

  /// Cập nhật tên node
  Future<void> updateNodeName(String nodeMac, String name) async {
    await _firestore.collection(AppConstants.colNodes).doc(nodeMac).update({
      'nodeMac': nodeMac, // Đảm bảo nodeMac luôn có
      'name': name,
    });
  }

  /// Helper để xóa các documents trong một subcollection
  Future<void> _deleteSubcollection(DocumentReference parentRef, String subcollectionName) async {
    final snapshot = await parentRef.collection(subcollectionName).get();
    if (snapshot.docs.isNotEmpty) {
      final batch = _firestore.batch();
      for (var doc in snapshot.docs) {
        batch.delete(doc.reference);
      }
      await batch.commit();
    }
  }

  /// Xóa node và các subcollections của nó triệt để
  Future<void> deleteNode(String nodeMac) async {
    final nodeRef = _firestore.collection(AppConstants.colNodes).doc(nodeMac);
    
    // Dọn dẹp các subcollections con để tránh dữ liệu rác
    await _deleteSubcollection(nodeRef, 'events');
    await _deleteSubcollection(nodeRef, 'measurements_raw');
    await _deleteSubcollection(nodeRef, 'measurements_10m');
    await _deleteSubcollection(nodeRef, 'measurements_1h');
    await _deleteSubcollection(nodeRef, 'measurements_4h');
    
    // Xóa document gốc
    await nodeRef.delete();
  }

  /// Lấy nodes theo gateway
  Future<List<NodeModel>> getNodesByGateway(String gatewayMac) async {
    final snapshot = await _firestore
        .collection(AppConstants.colNodes)
        .where('gatewayMac', isEqualTo: gatewayMac)
        .get();
    return snapshot.docs
        .map((doc) => NodeModel.fromMap(doc.data(), doc.id))
        .toList();
  }

  /// Bật/tắt tất cả nodes trong một room
  Future<void> toggleAllNodesInRoom(String roomId, bool stateOn) async {
    final nodes = await _firestore
        .collection(AppConstants.colNodes)
        .where('roomId', isEqualTo: roomId)
        .get();

    final batch = _firestore.batch();
    for (var doc in nodes.docs) {
      batch.update(doc.reference, {
        'nodeMac': doc.id, // Đảm bảo nodeMac luôn có
        'state_on': stateOn,
      });
    }
    await batch.commit();
  }

  // ==================== USER INFO ====================

  /// Lấy thông tin user
  Future<Map<String, dynamic>?> getUserInfo() async {
    if (_userId == null) return null;
    final doc = await _firestore
        .collection(AppConstants.colUsers)
        .doc(_userId)
        .get();
    return doc.data();
  }

  /// Cập nhật thông tin user
  Future<void> updateUserInfo(String displayName) async {
    if (_userId == null) return;
    await _firestore.collection(AppConstants.colUsers).doc(_userId).update({
      'userId': _userId, // Đảm bảo userId luôn có
      'displayName': displayName,
    });
    // Cập nhật cả trên Auth
    await _auth.currentUser?.updateDisplayName(displayName);
  }
}
