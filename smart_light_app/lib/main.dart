import 'package:firebase_core/firebase_core.dart';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:firebase_auth/firebase_auth.dart';

import 'constants.dart';
import 'providers/user_provider.dart';
import 'providers/home_provider.dart';
import 'screens/auth/login_screen.dart';
import 'screens/home/main_screen.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await Firebase.initializeApp();
  runApp(const MyApp());
}

class MyApp extends StatelessWidget {
  const MyApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MultiProvider(
      providers: [
        ChangeNotifierProvider(create: (_) => UserProvider()),
        ChangeNotifierProvider(create: (_) => HomeProvider()),
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
