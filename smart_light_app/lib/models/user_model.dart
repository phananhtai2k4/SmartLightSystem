class UserModel {
  final String id;
  final String email;
  final String displayName;

  UserModel({required this.id, required this.email, required this.displayName});

  // Chuyển từ JSON (Firestore) sang Object
  factory UserModel.fromMap(Map<String, dynamic> map, String id) {
    return UserModel(
      id: id,
      email: map['email'] ?? '',
      displayName: map['displayName'] ?? 'Người dùng mới',
    );
  }

  // Chuyển từ Object sang JSON (để đẩy lên Firestore)
  Map<String, dynamic> toMap() {
    return {
      'email': email,
      'displayName': displayName,
      'createdAt': DateTime.now().toIso8601String(),
    };
  }
}
