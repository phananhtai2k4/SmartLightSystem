import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:firebase_auth/firebase_auth.dart';
import '../../constants.dart';
import '../../providers/user_provider.dart';
import '../../providers/home_provider.dart';
import '../../services/home_service.dart';

class ProfileScreen extends StatefulWidget {
  const ProfileScreen({super.key});

  @override
  State<ProfileScreen> createState() => _ProfileScreenState();
}

class _ProfileScreenState extends State<ProfileScreen> {
  final HomeService _homeService = HomeService();
  Map<String, dynamic>? _userInfo;
  bool _isLoading = true;

  @override
  void initState() {
    super.initState();
    _loadUserInfo();
  }

  Future<void> _loadUserInfo() async {
    final info = await _homeService.getUserInfo();
    setState(() {
      _userInfo = info;
      _isLoading = false;
    });
  }

  @override
  Widget build(BuildContext context) {
    final user = FirebaseAuth.instance.currentUser;

    return Scaffold(
      appBar: AppBar(
        title: const Text('Cá nhân'),
        centerTitle: true,
        backgroundColor: AppConstants.primaryColor,
        foregroundColor: Colors.white,
      ),
      body: _isLoading
          ? const Center(child: CircularProgressIndicator())
          : ListView(
              padding: const EdgeInsets.all(16),
              children: [
                // Avatar và tên
                _ProfileHeader(
                  user: user,
                  userInfo: _userInfo,
                  onEditProfile: () => _showEditProfileDialog(context),
                ),

                const SizedBox(height: 24),

                // Thông tin tài khoản
                _InfoSection(user: user, userInfo: _userInfo),

                const SizedBox(height: 24),

                // Thống kê
                Consumer<HomeProvider>(
                  builder: (context, homeProvider, child) {
                    return _StatsSection(
                      homeCount: homeProvider.homes.length,
                      roomCount: homeProvider.rooms.length,
                      deviceCount: homeProvider.nodes.length,
                      gatewayCount: homeProvider.gateways.length,
                    );
                  },
                ),

                const SizedBox(height: 24),

                // Menu cài đặt
                _SettingsSection(
                  onChangePassword: () => _showChangePasswordDialog(context),
                  onAbout: () => _showAboutDialog(context),
                  onLogout: () => _showLogoutConfirmation(context),
                ),
              ],
            ),
    );
  }

  void _showEditProfileDialog(BuildContext context) {
    final nameController = TextEditingController(
      text: _userInfo?['displayName'] ?? '',
    );

    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Chỉnh sửa thông tin'),
        content: TextField(
          controller: nameController,
          decoration: const InputDecoration(
            labelText: 'Tên hiển thị',
            border: OutlineInputBorder(),
            prefixIcon: Icon(Icons.person),
          ),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('Hủy'),
          ),
          ElevatedButton(
            style: ElevatedButton.styleFrom(
              backgroundColor: AppConstants.primaryColor,
              foregroundColor: Colors.white,
            ),
            onPressed: () async {
              if (nameController.text.trim().isNotEmpty) {
                await _homeService.updateUserInfo(nameController.text.trim());
                if (context.mounted) {
                  Navigator.pop(context);
                  _loadUserInfo();
                  ScaffoldMessenger.of(context).showSnackBar(
                    const SnackBar(content: Text('Đã cập nhật thông tin')),
                  );
                }
              }
            },
            child: const Text('Lưu'),
          ),
        ],
      ),
    );
  }

  void _showChangePasswordDialog(BuildContext context) {
    final currentPassController = TextEditingController();
    final newPassController = TextEditingController();
    final confirmPassController = TextEditingController();
    final formKey = GlobalKey<FormState>();

    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Đổi mật khẩu'),
        content: Form(
          key: formKey,
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              TextFormField(
                controller: currentPassController,
                obscureText: true,
                decoration: const InputDecoration(
                  labelText: 'Mật khẩu hiện tại',
                  border: OutlineInputBorder(),
                ),
                validator: (value) =>
                    value!.isEmpty ? 'Vui lòng nhập mật khẩu hiện tại' : null,
              ),
              const SizedBox(height: 16),
              TextFormField(
                controller: newPassController,
                obscureText: true,
                decoration: const InputDecoration(
                  labelText: 'Mật khẩu mới',
                  border: OutlineInputBorder(),
                ),
                validator: (value) =>
                    value!.length < 6 ? 'Mật khẩu phải >= 6 ký tự' : null,
              ),
              const SizedBox(height: 16),
              TextFormField(
                controller: confirmPassController,
                obscureText: true,
                decoration: const InputDecoration(
                  labelText: 'Xác nhận mật khẩu mới',
                  border: OutlineInputBorder(),
                ),
                validator: (value) => value != newPassController.text
                    ? 'Mật khẩu không khớp'
                    : null,
              ),
            ],
          ),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('Hủy'),
          ),
          ElevatedButton(
            style: ElevatedButton.styleFrom(
              backgroundColor: AppConstants.primaryColor,
              foregroundColor: Colors.white,
            ),
            onPressed: () async {
              if (formKey.currentState!.validate()) {
                try {
                  final user = FirebaseAuth.instance.currentUser;
                  final credential = EmailAuthProvider.credential(
                    email: user!.email!,
                    password: currentPassController.text,
                  );
                  await user.reauthenticateWithCredential(credential);
                  await user.updatePassword(newPassController.text);

                  if (context.mounted) {
                    Navigator.pop(context);
                    ScaffoldMessenger.of(context).showSnackBar(
                      const SnackBar(
                        content: Text('Đổi mật khẩu thành công!'),
                        backgroundColor: Colors.green,
                      ),
                    );
                  }
                } catch (e) {
                  if (context.mounted) {
                    ScaffoldMessenger.of(context).showSnackBar(
                      SnackBar(
                        content: Text('Lỗi: ${e.toString()}'),
                        backgroundColor: Colors.red,
                      ),
                    );
                  }
                }
              }
            },
            child: const Text('Đổi mật khẩu'),
          ),
        ],
      ),
    );
  }

  void _showAboutDialog(BuildContext context) {
    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: Row(
          children: [
            Icon(Icons.lightbulb, color: AppConstants.accentColor),
            const SizedBox(width: 8),
            const Text('Smart Light'),
          ],
        ),
        content: const Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text('Phiên bản: 1.0.0'),
            SizedBox(height: 8),
            Text(
              'Ứng dụng điều khiển đèn thông minh sử dụng công nghệ BLE Mesh.',
            ),
            SizedBox(height: 16),
            Text(
              '© 2025 Smart Light Team',
              style: TextStyle(color: Colors.grey),
            ),
          ],
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('Đóng'),
          ),
        ],
      ),
    );
  }

  void _showLogoutConfirmation(BuildContext context) {
    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Đăng xuất'),
        content: const Text('Bạn có chắc muốn đăng xuất khỏi tài khoản?'),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('Hủy'),
          ),
          ElevatedButton(
            style: ElevatedButton.styleFrom(
              backgroundColor: Colors.red,
              foregroundColor: Colors.white,
            ),
            onPressed: () {
              Navigator.pop(context);
              context.read<UserProvider>().logout();
            },
            child: const Text('Đăng xuất'),
          ),
        ],
      ),
    );
  }
}

class _ProfileHeader extends StatelessWidget {
  final User? user;
  final Map<String, dynamic>? userInfo;
  final VoidCallback onEditProfile;

  const _ProfileHeader({
    required this.user,
    required this.userInfo,
    required this.onEditProfile,
  });

  @override
  Widget build(BuildContext context) {
    final displayName =
        userInfo?['displayName'] ?? user?.displayName ?? 'Người dùng';
    final email = user?.email ?? '';

    return Container(
      padding: const EdgeInsets.all(24),
      decoration: BoxDecoration(
        gradient: LinearGradient(
          colors: [
            AppConstants.primaryColor,
            AppConstants.primaryColor.withOpacity(0.7),
          ],
        ),
        borderRadius: BorderRadius.circular(20),
        boxShadow: [
          BoxShadow(
            color: AppConstants.primaryColor.withOpacity(0.3),
            blurRadius: 15,
            offset: const Offset(0, 6),
          ),
        ],
      ),
      child: Column(
        children: [
          // Avatar
          Stack(
            children: [
              CircleAvatar(
                radius: 50,
                backgroundColor: Colors.white.withOpacity(0.2),
                child: Text(
                  displayName.isNotEmpty ? displayName[0].toUpperCase() : 'U',
                  style: const TextStyle(
                    fontSize: 40,
                    fontWeight: FontWeight.bold,
                    color: Colors.white,
                  ),
                ),
              ),
              Positioned(
                bottom: 0,
                right: 0,
                child: GestureDetector(
                  onTap: onEditProfile,
                  child: Container(
                    padding: const EdgeInsets.all(8),
                    decoration: const BoxDecoration(
                      color: Colors.white,
                      shape: BoxShape.circle,
                    ),
                    child: Icon(
                      Icons.edit,
                      size: 16,
                      color: AppConstants.primaryColor,
                    ),
                  ),
                ),
              ),
            ],
          ),
          const SizedBox(height: 16),
          // Tên
          Text(
            displayName,
            style: const TextStyle(
              color: Colors.white,
              fontSize: 24,
              fontWeight: FontWeight.bold,
            ),
          ),
          const SizedBox(height: 4),
          // Email
          Text(
            email,
            style: TextStyle(
              color: Colors.white.withOpacity(0.8),
              fontSize: 14,
            ),
          ),
        ],
      ),
    );
  }
}

class _InfoSection extends StatelessWidget {
  final User? user;
  final Map<String, dynamic>? userInfo;

  const _InfoSection({required this.user, required this.userInfo});

  @override
  Widget build(BuildContext context) {
    return Card(
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(16)),
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Row(
              children: [
                Icon(Icons.info_outline, color: AppConstants.primaryColor),
                SizedBox(width: 8),
                Text(
                  'Thông tin tài khoản',
                  style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
                ),
              ],
            ),
            const SizedBox(height: 16),
            _InfoRow(
              icon: Icons.email,
              label: 'Email',
              value: user?.email ?? 'Chưa có',
            ),
            const Divider(),
            _InfoRow(
              icon: Icons.fingerprint,
              label: 'User ID',
              value: user?.uid ?? 'Chưa có',
            ),
            const Divider(),
            _InfoRow(
              icon: Icons.calendar_today,
              label: 'Ngày tạo',
              value: _formatDate(userInfo?['createdAt']),
            ),
          ],
        ),
      ),
    );
  }

  String _formatDate(dynamic timestamp) {
    if (timestamp == null) return 'Không rõ';
    try {
      if (timestamp is String) {
        final date = DateTime.parse(timestamp);
        return '${date.day}/${date.month}/${date.year}';
      }
      // Firestore Timestamp
      final date = timestamp.toDate();
      return '${date.day}/${date.month}/${date.year}';
    } catch (e) {
      return 'Không rõ';
    }
  }
}

class _InfoRow extends StatelessWidget {
  final IconData icon;
  final String label;
  final String value;

  const _InfoRow({
    required this.icon,
    required this.label,
    required this.value,
  });

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 8),
      child: Row(
        children: [
          Icon(icon, size: 20, color: Colors.grey),
          const SizedBox(width: 12),
          Expanded(
            flex: 2,
            child: Text(label, style: const TextStyle(color: Colors.grey)),
          ),
          Expanded(
            flex: 3,
            child: Text(
              value,
              style: const TextStyle(fontWeight: FontWeight.w500),
              textAlign: TextAlign.right,
              overflow: TextOverflow.ellipsis,
            ),
          ),
        ],
      ),
    );
  }
}

class _StatsSection extends StatelessWidget {
  final int homeCount;
  final int roomCount;
  final int deviceCount;
  final int gatewayCount;

  const _StatsSection({
    required this.homeCount,
    required this.roomCount,
    required this.deviceCount,
    required this.gatewayCount,
  });

  @override
  Widget build(BuildContext context) {
    return Card(
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(16)),
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Row(
              children: [
                Icon(Icons.analytics, color: AppConstants.primaryColor),
                SizedBox(width: 8),
                Text(
                  'Thống kê',
                  style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
                ),
              ],
            ),
            const SizedBox(height: 16),
            Row(
              children: [
                _StatItem(
                  icon: Icons.home,
                  value: homeCount.toString(),
                  label: 'Nhà',
                  color: Colors.blue,
                ),
                _StatItem(
                  icon: Icons.meeting_room,
                  value: roomCount.toString(),
                  label: 'Phòng',
                  color: Colors.green,
                ),
                _StatItem(
                  icon: Icons.router,
                  value: gatewayCount.toString(),
                  label: 'Gateway',
                  color: Colors.orange,
                ),
                _StatItem(
                  icon: Icons.lightbulb,
                  value: deviceCount.toString(),
                  label: 'Đèn',
                  color: Colors.purple,
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}

class _StatItem extends StatelessWidget {
  final IconData icon;
  final String value;
  final String label;
  final Color color;

  const _StatItem({
    required this.icon,
    required this.value,
    required this.label,
    required this.color,
  });

  @override
  Widget build(BuildContext context) {
    return Expanded(
      child: Column(
        children: [
          Container(
            padding: const EdgeInsets.all(12),
            decoration: BoxDecoration(
              color: color.withOpacity(0.1),
              shape: BoxShape.circle,
            ),
            child: Icon(icon, color: color),
          ),
          const SizedBox(height: 8),
          Text(
            value,
            style: TextStyle(
              fontSize: 20,
              fontWeight: FontWeight.bold,
              color: color,
            ),
          ),
          Text(label, style: const TextStyle(fontSize: 12, color: Colors.grey)),
        ],
      ),
    );
  }
}

class _SettingsSection extends StatelessWidget {
  final VoidCallback onChangePassword;
  final VoidCallback onAbout;
  final VoidCallback onLogout;

  const _SettingsSection({
    required this.onChangePassword,
    required this.onAbout,
    required this.onLogout,
  });

  @override
  Widget build(BuildContext context) {
    return Card(
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(16)),
      child: Column(
        children: [
          ListTile(
            leading: const Icon(Icons.lock_outline),
            title: const Text('Đổi mật khẩu'),
            trailing: const Icon(Icons.chevron_right),
            onTap: onChangePassword,
          ),
          const Divider(height: 1),
          ListTile(
            leading: const Icon(Icons.info_outline),
            title: const Text('Về ứng dụng'),
            trailing: const Icon(Icons.chevron_right),
            onTap: onAbout,
          ),
          const Divider(height: 1),
          ListTile(
            leading: const Icon(Icons.logout, color: Colors.red),
            title: const Text('Đăng xuất', style: TextStyle(color: Colors.red)),
            onTap: onLogout,
          ),
        ],
      ),
    );
  }
}
