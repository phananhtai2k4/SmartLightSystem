class GatewayModel {
  final String id; // gatewayMac
  final String homeId;
  final String name;
  final String topicSub;
  final String topicPub;
  final String topicAlert;
  final bool online;

  GatewayModel({
    required this.id,
    required this.homeId,
    required this.name,
    required this.topicSub,
    required this.topicPub,
    required this.topicAlert,
    required this.online,
  });

  factory GatewayModel.fromMap(Map<String, dynamic> map, String id) {
    return GatewayModel(
      id: id,
      homeId: map['homeId'] ?? '',
      name: map['name'] ?? 'Gateway',
      topicSub: map['topicSub'] ?? '',
      topicPub: map['topicPub'] ?? '',
      topicAlert: map['topicAlert'] ?? '',
      online: map['online'] ?? false,
    );
  }

  Map<String, dynamic> toMap() {
    return {
      'homeId': homeId,
      'name': name,
      'topicSub': topicSub,
      'topicPub': topicPub,
      'topicAlert': topicAlert,
      'online': online,
    };
  }
}
