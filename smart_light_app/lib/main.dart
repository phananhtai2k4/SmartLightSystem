import 'package:firebase_core/firebase_core.dart';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:firebase_auth/firebase_auth.dart';

import 'constants.dart';
import 'providers/user_provider.dart';
import 'providers/home_provider.dart';
import 'screens/auth/login_screen.dart';
import 'screens/home/main_screen.dart';
import 'services/notification_service.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  print('🚀 App starting...');
  await Firebase.initializeApp();
  print('🔥 Firebase initialized');

  await NotificationService().initialize();
  print('🔔 NotificationService initialized');

  // Khởi tạo UserProvider ngay từ đầu (eager) thay vì lazy
  final userProvider = UserProvider();
  print('👤 UserProvider created');

  // Khởi tạo HomeProvider và inject MqttService
  final homeProvider = HomeProvider();
  homeProvider.setMqttService(userProvider.mqttService);
  print('🏠 HomeProvider created with MqttService');

  runApp(MyApp(userProvider: userProvider, homeProvider: homeProvider));
}

class MyApp extends StatelessWidget {
  final UserProvider userProvider;
  final HomeProvider homeProvider;

  const MyApp({
    super.key,
    required this.userProvider,
    required this.homeProvider,
  });

  @override
  Widget build(BuildContext context) {
    return MultiProvider(
      providers: [
        ChangeNotifierProvider.value(value: userProvider),
        ChangeNotifierProvider.value(value: homeProvider),
      ],
      child: MaterialApp(
        title: 'Smart Light',
        debugShowCheckedModeBanner: false,
        theme: ThemeData(
          primaryColor: AppConstants.primaryColor,
          colorScheme: ColorScheme.fromSeed(
            seedColor: AppConstants.primaryColor,
          ),
          useMaterial3: true,
        ),
        // Dùng StreamBuilder để lắng nghe sự kiện Đăng nhập/Đăng xuất
        home: StreamBuilder<User?>(
          stream: FirebaseAuth.instance.authStateChanges(),
          builder: (context, snapshot) {
            if (snapshot.connectionState == ConnectionState.waiting) {
              return const Scaffold(
                body: Center(child: CircularProgressIndicator()),
              );
            }
            if (snapshot.hasData) {
              // Đã đăng nhập -> Vào MainScreen với BottomNavBar
              return const MainScreen();
            }
            // Chưa đăng nhập -> Vào Login
            return const LoginScreen();
          },
        ),
      ),
    );
  }
}
