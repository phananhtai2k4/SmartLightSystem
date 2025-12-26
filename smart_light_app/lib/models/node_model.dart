class NodeModel {
  final String id; // nodeMac
  final String roomId;
  final String gatewayMac;
  final String name;
  final String type;
  final int meshAddress;
  final bool online;
  final bool stateOn;

  NodeModel({
    required this.id,
    required this.roomId,
    required this.gatewayMac,
    required this.name,
    required this.type,
    required this.meshAddress,
    required this.online,
    required this.stateOn,
  });

  factory NodeModel.fromMap(Map<String, dynamic> map, String id) {
    return NodeModel(
      id: id,
      roomId: map['roomId'] ?? '',
      gatewayMac: map['gatewayMac'] ?? '',
      name: map['name'] ?? 'Thiết bị',
      type: map['type'] ?? 'light',
      meshAddress: map['meshAddress'] ?? 0,
      online: map['online'] ?? false,
      stateOn: map['state_on'] ?? false,
    );
  }

  Map<String, dynamic> toMap() {
    return {
      'roomId': roomId,
      'gatewayMac': gatewayMac,
      'name': name,
      'type': type,
      'meshAddress': meshAddress,
      'online': online,
      'state_on': stateOn,
    };
  }

  NodeModel copyWith({
    String? id,
    String? roomId,
    String? gatewayMac,
    String? name,
    String? type,
    int? meshAddress,
    bool? online,
    bool? stateOn,
  }) {
    return NodeModel(
      id: id ?? this.id,
      roomId: roomId ?? this.roomId,
      gatewayMac: gatewayMac ?? this.gatewayMac,
      name: name ?? this.name,
      type: type ?? this.type,
      meshAddress: meshAddress ?? this.meshAddress,
      online: online ?? this.online,
      stateOn: stateOn ?? this.stateOn,
    );
  }
}
