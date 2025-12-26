import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../../constants.dart';
import '../../providers/home_provider.dart';
import '../../models/home_model.dart';
import '../../models/room_model.dart';
import 'room_detail_screen.dart';

class RoomsScreen extends StatelessWidget {
  const RoomsScreen({super.key});

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Phòng'),
        centerTitle: true,
        backgroundColor: AppConstants.primaryColor,
        foregroundColor: Colors.white,
        actions: [
          // Icon tạo phòng
          IconButton(
            icon: const Icon(Icons.add_circle_outline),
            onPressed: () => _showCreateRoomDialog(context),
          ),
        ],
      ),
      body: Consumer<HomeProvider>(
        builder: (context, homeProvider, child) {
          if (homeProvider.isLoading) {
            return const Center(child: CircularProgressIndicator());
          }

          return RefreshIndicator(
            onRefresh: () => homeProvider.refresh(),
            child: ListView(
              padding: const EdgeInsets.all(16),
              children: [
                // Card chọn nhà
                _HomeSelector(
                  homes: homeProvider.homes,
                  selectedHome: homeProvider.selectedHome,
                  onHomeSelected: (home) => homeProvider.selectHome(home),
                  onCreateHome: () => _showCreateHomeDialog(context),
                ),

                const SizedBox(height: 24),

                // Danh sách phòng
                if (homeProvider.selectedHome != null) ...[
                  Row(
                    mainAxisAlignment: MainAxisAlignment.spaceBetween,
                    children: [
                      const Text(
                        'Danh sách phòng',
                        style: TextStyle(
                          fontSize: 18,
                          fontWeight: FontWeight.bold,
                        ),
                      ),
                      Text(
                        '${homeProvider.rooms.length} phòng',
                        style: const TextStyle(color: Colors.grey),
                      ),
                    ],
                  ),
                  const SizedBox(height: 12),

                  if (homeProvider.rooms.isEmpty)
                    _EmptyRoomState(
                      onCreateRoom: () => _showCreateRoomDialog(context),
                    )
                  else
                    ...homeProvider.rooms.map(
                      (room) => _RoomCard(
                        room: room,
                        nodesInRoom: homeProvider.getNodesInRoom(room.id),
                        isAnyNodeOn: homeProvider.isAnyNodeOnInRoom(room.id),
                        onToggle: (value) {
                          homeProvider.toggleAllNodesInRoom(room.id, value);
                        },
                      ),
                    ),
                ] else
                  _EmptyHomeState(
                    onCreateHome: () => _showCreateHomeDialog(context),
                  ),
              ],
            ),
          );
        },
      ),
    );
  }

  void _showCreateHomeDialog(BuildContext context) {
    final nameController = TextEditingController();
    final descController = TextEditingController();
    final formKey = GlobalKey<FormState>();

    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Tạo nhà mới'),
        content: Form(
          key: formKey,
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              TextFormField(
                controller: nameController,
                decoration: const InputDecoration(
                  labelText: 'Tên nhà',
                  border: OutlineInputBorder(),
                  prefixIcon: Icon(Icons.home),
                ),
                validator: (value) =>
                    value?.isEmpty == true ? 'Vui lòng nhập tên' : null,
              ),
              const SizedBox(height: 16),
              TextFormField(
                controller: descController,
                decoration: const InputDecoration(
                  labelText: 'Mô tả (tùy chọn)',
                  border: OutlineInputBorder(),
                  prefixIcon: Icon(Icons.description),
                ),
                maxLines: 2,
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
                final success = await context.read<HomeProvider>().createHome(
                  nameController.text.trim(),
                  descController.text.trim(),
                );
                if (context.mounted) {
                  Navigator.pop(context);
                  if (success) {
                    ScaffoldMessenger.of(context).showSnackBar(
                      const SnackBar(content: Text('Tạo nhà thành công!')),
                    );
                  }
                }
              }
            },
            child: const Text('Tạo'),
          ),
        ],
      ),
    );
  }

  void _showCreateRoomDialog(BuildContext context) {
    final homeProvider = context.read<HomeProvider>();
    if (homeProvider.selectedHome == null) {
      ScaffoldMessenger.of(
        context,
      ).showSnackBar(const SnackBar(content: Text('Vui lòng tạo nhà trước!')));
      return;
    }

    final nameController = TextEditingController();
    final groupAddressController = TextEditingController();
    final formKey = GlobalKey<FormState>();

    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Tạo phòng mới'),
        content: Form(
          key: formKey,
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              TextFormField(
                controller: nameController,
                decoration: const InputDecoration(
                  labelText: 'Tên phòng',
                  border: OutlineInputBorder(),
                  prefixIcon: Icon(Icons.meeting_room),
                ),
                validator: (value) =>
                    value?.isEmpty == true ? 'Vui lòng nhập tên phòng' : null,
              ),
              const SizedBox(height: 16),
              TextFormField(
                controller: groupAddressController,
                keyboardType: TextInputType.number,
                decoration: const InputDecoration(
                  labelText: 'Group Address (Mesh)',
                  border: OutlineInputBorder(),
                  prefixIcon: Icon(Icons.numbers),
                  hintText: 'Ví dụ: 49152',
                ),
                validator: (value) {
                  if (value?.isEmpty == true)
                    return 'Vui lòng nhập group address';
                  if (int.tryParse(value!) == null) return 'Phải là số';
                  return null;
                },
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
                final success = await homeProvider.createRoom(
                  nameController.text.trim(),
                  int.parse(groupAddressController.text.trim()),
                );
                if (context.mounted) {
                  Navigator.pop(context);
                  if (success) {
                    ScaffoldMessenger.of(context).showSnackBar(
                      const SnackBar(content: Text('Tạo phòng thành công!')),
                    );
                  }
                }
              }
            },
            child: const Text('Tạo'),
          ),
        ],
      ),
    );
  }
}

class _HomeSelector extends StatelessWidget {
  final List<HomeModel> homes;
  final HomeModel? selectedHome;
  final Function(HomeModel) onHomeSelected;
  final VoidCallback onCreateHome;

  const _HomeSelector({
    required this.homes,
    required this.selectedHome,
    required this.onHomeSelected,
    required this.onCreateHome,
  });

  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      onTap: () => _showHomeSelectionBottomSheet(context),
      child: Container(
        padding: const EdgeInsets.all(16),
        decoration: BoxDecoration(
          gradient: LinearGradient(
            colors: [
              AppConstants.primaryColor,
              AppConstants.primaryColor.withOpacity(0.7),
            ],
          ),
          borderRadius: BorderRadius.circular(16),
          boxShadow: [
            BoxShadow(
              color: AppConstants.primaryColor.withOpacity(0.3),
              blurRadius: 10,
              offset: const Offset(0, 4),
            ),
          ],
        ),
        child: Row(
          children: [
            Container(
              padding: const EdgeInsets.all(12),
              decoration: BoxDecoration(
                color: Colors.white.withOpacity(0.2),
                borderRadius: BorderRadius.circular(12),
              ),
              child: const Icon(Icons.home, color: Colors.white, size: 32),
            ),
            const SizedBox(width: 16),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    selectedHome?.name ?? 'Chọn nhà',
                    style: const TextStyle(
                      color: Colors.white,
                      fontSize: 20,
                      fontWeight: FontWeight.bold,
                    ),
                  ),
                  if (selectedHome != null)
                    Text(
                      selectedHome!.description.isEmpty
                          ? 'Nhấn để đổi nhà'
                          : selectedHome!.description,
                      style: const TextStyle(color: Colors.white70),
                    ),
                ],
              ),
            ),
            const Icon(Icons.keyboard_arrow_down, color: Colors.white),
          ],
        ),
      ),
    );
  }

  void _showHomeSelectionBottomSheet(BuildContext context) {
    showModalBottomSheet(
      context: context,
      shape: const RoundedRectangleBorder(
        borderRadius: BorderRadius.vertical(top: Radius.circular(20)),
      ),
      builder: (context) => Container(
        padding: const EdgeInsets.all(16),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Container(
              width: 40,
              height: 4,
              decoration: BoxDecoration(
                color: Colors.grey.shade300,
                borderRadius: BorderRadius.circular(2),
              ),
            ),
            const SizedBox(height: 16),
            const Text(
              'Chọn nhà',
              style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
            ),
            const SizedBox(height: 16),
            if (homes.isEmpty)
              const Padding(
                padding: EdgeInsets.all(24),
                child: Text('Chưa có nhà nào'),
              )
            else
              ...homes.map(
                (home) => ListTile(
                  leading: Container(
                    padding: const EdgeInsets.all(8),
                    decoration: BoxDecoration(
                      color: selectedHome?.id == home.id
                          ? AppConstants.primaryColor.withOpacity(0.1)
                          : Colors.grey.shade100,
                      borderRadius: BorderRadius.circular(8),
                    ),
                    child: Icon(
                      Icons.home,
                      color: selectedHome?.id == home.id
                          ? AppConstants.primaryColor
                          : Colors.grey,
                    ),
                  ),
                  title: Text(home.name),
                  subtitle: home.description.isNotEmpty
                      ? Text(home.description)
                      : null,
                  trailing: selectedHome?.id == home.id
                      ? const Icon(
                          Icons.check,
                          color: AppConstants.primaryColor,
                        )
                      : null,
                  onTap: () {
                    onHomeSelected(home);
                    Navigator.pop(context);
                  },
                ),
              ),
            const Divider(),
            ListTile(
              leading: Container(
                padding: const EdgeInsets.all(8),
                decoration: BoxDecoration(
                  color: Colors.green.withOpacity(0.1),
                  borderRadius: BorderRadius.circular(8),
                ),
                child: const Icon(Icons.add, color: Colors.green),
              ),
              title: const Text('Tạo nhà mới'),
              onTap: () {
                Navigator.pop(context);
                onCreateHome();
              },
            ),
          ],
        ),
      ),
    );
  }
}

class _RoomCard extends StatelessWidget {
  final RoomModel room;
  final List nodesInRoom;
  final bool isAnyNodeOn;
  final Function(bool) onToggle;

  const _RoomCard({
    required this.room,
    required this.nodesInRoom,
    required this.isAnyNodeOn,
    required this.onToggle,
  });

  @override
  Widget build(BuildContext context) {
    return Card(
      margin: const EdgeInsets.only(bottom: 12),
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
      child: InkWell(
        borderRadius: BorderRadius.circular(12),
        onTap: () {
          Navigator.push(
            context,
            MaterialPageRoute(builder: (_) => RoomDetailScreen(room: room)),
          );
        },
        child: Padding(
          padding: const EdgeInsets.all(16),
          child: Row(
            children: [
              // Icon phòng
              Container(
                padding: const EdgeInsets.all(12),
                decoration: BoxDecoration(
                  color: isAnyNodeOn
                      ? AppConstants.accentColor.withOpacity(0.2)
                      : Colors.grey.shade100,
                  borderRadius: BorderRadius.circular(12),
                ),
                child: Icon(
                  _getRoomIcon(room.name),
                  size: 28,
                  color: isAnyNodeOn ? AppConstants.accentColor : Colors.grey,
                ),
              ),
              const SizedBox(width: 16),
              // Thông tin phòng
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(
                      room.name,
                      style: const TextStyle(
                        fontSize: 16,
                        fontWeight: FontWeight.w600,
                      ),
                    ),
                    const SizedBox(height: 4),
                    Text(
                      '${nodesInRoom.length} thiết bị',
                      style: const TextStyle(color: Colors.grey, fontSize: 13),
                    ),
                  ],
                ),
              ),
              // Switch bật/tắt cả phòng
              if (nodesInRoom.isNotEmpty)
                Switch(
                  value: isAnyNodeOn,
                  activeColor: AppConstants.accentColor,
                  onChanged: onToggle,
                ),
              const Icon(Icons.chevron_right, color: Colors.grey),
            ],
          ),
        ),
      ),
    );
  }

  IconData _getRoomIcon(String roomName) {
    final name = roomName.toLowerCase();
    if (name.contains('ngủ') || name.contains('bedroom')) {
      return Icons.bed;
    } else if (name.contains('khách') || name.contains('living')) {
      return Icons.weekend;
    } else if (name.contains('bếp') || name.contains('kitchen')) {
      return Icons.kitchen;
    } else if (name.contains('tắm') ||
        name.contains('wc') ||
        name.contains('bath')) {
      return Icons.bathtub;
    } else if (name.contains('làm việc') || name.contains('office')) {
      return Icons.computer;
    } else if (name.contains('ban công') || name.contains('balcony')) {
      return Icons.balcony;
    }
    return Icons.meeting_room;
  }
}

class _EmptyRoomState extends StatelessWidget {
  final VoidCallback onCreateRoom;

  const _EmptyRoomState({required this.onCreateRoom});

  @override
  Widget build(BuildContext context) {
    return Center(
      child: Column(
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          const SizedBox(height: 40),
          Icon(
            Icons.meeting_room_outlined,
            size: 80,
            color: Colors.grey.shade300,
          ),
          const SizedBox(height: 16),
          const Text(
            'Chưa có phòng nào',
            style: TextStyle(fontSize: 18, color: Colors.grey),
          ),
          const SizedBox(height: 8),
          TextButton.icon(
            icon: const Icon(Icons.add),
            label: const Text('Tạo phòng mới'),
            onPressed: onCreateRoom,
          ),
        ],
      ),
    );
  }
}

class _EmptyHomeState extends StatelessWidget {
  final VoidCallback onCreateHome;

  const _EmptyHomeState({required this.onCreateHome});

  @override
  Widget build(BuildContext context) {
    return Center(
      child: Column(
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          const SizedBox(height: 60),
          Icon(Icons.home_outlined, size: 100, color: Colors.grey.shade300),
          const SizedBox(height: 24),
          const Text(
            'Chào mừng đến Smart Light!',
            style: TextStyle(fontSize: 20, fontWeight: FontWeight.bold),
          ),
          const SizedBox(height: 8),
          const Text(
            'Tạo nhà đầu tiên để bắt đầu',
            style: TextStyle(color: Colors.grey),
          ),
          const SizedBox(height: 24),
          ElevatedButton.icon(
            icon: const Icon(Icons.add),
            label: const Text('Tạo nhà mới'),
            style: ElevatedButton.styleFrom(
              backgroundColor: AppConstants.primaryColor,
              foregroundColor: Colors.white,
              padding: const EdgeInsets.symmetric(horizontal: 24, vertical: 12),
            ),
            onPressed: onCreateHome,
          ),
        ],
      ),
    );
  }
}
