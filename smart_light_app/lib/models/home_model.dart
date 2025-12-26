import 'package:cloud_firestore/cloud_firestore.dart';

class HomeModel {
  final String id;
  final String name;
  final String description;

  HomeModel({required this.id, required this.name, required this.description});

  factory HomeModel.fromMap(Map<String, dynamic> map, String id) {
    return HomeModel(
      id: id,
      name: map['name'] ?? '',
      description: map['description'] ?? '',
    );
  }

  Map<String, dynamic> toMap() {
    return {'name': name, 'description': description};
  }
}

class HomeMemberModel {
  final String id;
  final String homeId;
  final String userId;
  final String role; // "owner" | "member"

  HomeMemberModel({
    required this.id,
    required this.homeId,
    required this.userId,
    required this.role,
  });

  factory HomeMemberModel.fromMap(Map<String, dynamic> map, String id) {
    return HomeMemberModel(
      id: id,
      homeId: map['homeId'] ?? '',
      userId: map['userId'] ?? '',
      role: map['role'] ?? 'member',
    );
  }

  Map<String, dynamic> toMap() {
    return {'homeId': homeId, 'userId': userId, 'role': role};
  }

  bool get isOwner => role == 'owner';
}
