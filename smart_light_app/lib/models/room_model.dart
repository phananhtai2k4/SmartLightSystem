class RoomModel {
  final String id;
  final String homeId;
  final String name;
  final int groupAddress;

  RoomModel({
    required this.id,
    required this.homeId,
    required this.name,
    required this.groupAddress,
  });

  factory RoomModel.fromMap(Map<String, dynamic> map, String id) {
    return RoomModel(
      id: id,
      homeId: map['homeId'] ?? '',
      name: map['name'] ?? '',
      groupAddress: map['groupAddress'] ?? 0,
    );
  }

  Map<String, dynamic> toMap() {
    return {'homeId': homeId, 'name': name, 'groupAddress': groupAddress};
  }
}
