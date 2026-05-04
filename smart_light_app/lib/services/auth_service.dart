import 'package:firebase_auth/firebase_auth.dart';
import 'package:cloud_firestore/cloud_firestore.dart';
import '../constants.dart';

class AuthService {
  final FirebaseAuth _auth = FirebaseAuth.instance;
  final FirebaseFirestore _firestore = FirebaseFirestore.instance;

  // Lấy user hiện tại
  User? get currentUser => _auth.currentUser;

  // Stream theo dõi trạng thái đăng nhập (để tự động chuyển màn hình)
  Stream<User?> get authStateChanges => _auth.authStateChanges();

  // 1. Đăng ký (Sign Up)
  Future<void> signUp({
    required String email,
    required String password,
    required String name,
  }) async {
    try {
      // Tạo tài khoản trên Firebase Auth
      UserCredential result = await _auth.createUserWithEmailAndPassword(
        email: email,
        password: password,
      );

      // Lưu thêm thông tin vào Firestore (Collection 'users')
      if (result.user != null) {
        final userId = result.user!.uid;
        await _firestore.collection(AppConstants.colUsers).doc(userId).set({
          'email': email,
          'displayName': name,
          'createdAt': FieldValue.serverTimestamp(), // Lấy giờ server
          'userId': userId,
        });

        // Cập nhật tên hiển thị trên Auth luôn
        await result.user!.updateDisplayName(name);

        // --- BẮT ĐẦU: Tạo nhà mặc định cho user ---
        final homeRef = _firestore.collection(AppConstants.colHomes).doc();
        final homeId = homeRef.id;

        // 1. Lưu thông tin nhà (Home)
        await homeRef.set({
          'homeId': homeId,
          'name': 'Nhà của $name',
          'description': 'Nhà mặc định',
        });

        // 2. Phân quyền chủ nhà (Owner) trong home_members
        final memberDocId = '${userId}_$homeId';
        await _firestore
            .collection(AppConstants.colHomeMembers)
            .doc(memberDocId)
            .set({
              'id': memberDocId,
              'homeId': homeId,
              'userId': userId,
              'role': 'owner',
            });
        // --- KẾT THÚC: Tạo nhà mặc định ---
      }
    } on FirebaseAuthException catch (e) {
      throw e.message ?? "Lỗi đăng ký không xác định";
    }
  }

  // 2. Đăng nhập (Sign In)
  Future<void> signIn({required String email, required String password}) async {
    try {
      await _auth.signInWithEmailAndPassword(email: email, password: password);
    } on FirebaseAuthException catch (e) {
      throw e.message ?? "Lỗi đăng nhập";
    }
  }

  // 3. Đăng xuất (Sign Out)
  Future<void> signOut() async {
    await _auth.signOut();
  }

  // 4. Quên mật khẩu (Reset Password)
  Future<void> resetPassword({required String email}) async {
    try {
      await _auth.sendPasswordResetEmail(email: email);
    } on FirebaseAuthException catch (e) {
      throw e.message ?? "Lỗi gửi email reset";
    }
  }
}
