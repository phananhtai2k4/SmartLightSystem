import 'dart:async';
import 'package:cloud_firestore/cloud_firestore.dart';
import 'package:firebase_messaging/firebase_messaging.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter_local_notifications/flutter_local_notifications.dart';
import '../constants.dart';

class NotificationService {
  // Singleton Pattern
  static final NotificationService _instance = NotificationService._internal();
  factory NotificationService() => _instance;
  NotificationService._internal();

  final FirebaseFirestore _firestore = FirebaseFirestore.instance;
  final FirebaseMessaging _messaging = FirebaseMessaging.instance;
  static final FlutterLocalNotificationsPlugin _localNotificationsPlugin =
      FlutterLocalNotificationsPlugin();

  String? _lastToken;
  StreamSubscription<String>? _tokenRefreshSubscription;

  /// Initialize notifications (FCM & Local Notifications)
  Future<void> initialize() async {
    // 1. Configure iOS presentation options for foreground notifications
    await _messaging.setForegroundNotificationPresentationOptions(
      alert: true,
      badge: true,
      sound: true,
    );

    // 2. Initialize local notifications for Android
    const AndroidInitializationSettings initializationSettingsAndroid =
        AndroidInitializationSettings('@mipmap/ic_launcher');

    const InitializationSettings initializationSettings = InitializationSettings(
      android: initializationSettingsAndroid,
    );

    await _localNotificationsPlugin.initialize(
      settings: initializationSettings,
      onDidReceiveNotificationResponse: (NotificationResponse details) {
        debugPrint('🔔 Local notification clicked: ${details.payload}');
      },
    );

    // Create high importance Android notification channel
    const AndroidNotificationChannel channel = AndroidNotificationChannel(
      'fcm_fallback_notification_channel', // Match the channelId set in the backend
      'Urgent Notifications',
      description: 'Used for important alert notifications',
      importance: Importance.max,
      playSound: true,
    );

    final androidPlugin = _localNotificationsPlugin
        .resolvePlatformSpecificImplementation<
            AndroidFlutterLocalNotificationsPlugin>();

    await androidPlugin?.createNotificationChannel(channel);
    
    // Request Android 13+ permission
    await androidPlugin?.requestNotificationsPermission();

    // 3. Listen to foreground messages
    FirebaseMessaging.onMessage.listen((RemoteMessage message) {
      debugPrint('🔔 Received a foreground message: ${message.messageId}');
      _showLocalNotification(message);
    });
  }

  void _showLocalNotification(RemoteMessage message) async {
    try {
      RemoteNotification? notification = message.notification;
      if (notification == null) return;

      await _localNotificationsPlugin.show(
        id: notification.hashCode,
        title: notification.title,
        body: notification.body,
        notificationDetails: const NotificationDetails(
          android: AndroidNotificationDetails(
            'fcm_fallback_notification_channel',
            'Urgent Notifications',
            channelDescription: 'Used for important alert notifications',
            importance: Importance.max,
            priority: Priority.high,
            icon: '@mipmap/ic_launcher',
            playSound: true,
          ),
        ),
      );
    } catch (e) {
      debugPrint('🔔 Error displaying local notification: $e');
    }
  }

  /// Request notification permissions and return the current token
  Future<String?> getFcmToken() async {
    try {
      NotificationSettings settings = await _messaging.requestPermission(
        alert: true,
        badge: true,
        sound: true,
      );
      if (settings.authorizationStatus == AuthorizationStatus.authorized) {
        String? token = await _messaging.getToken();
        debugPrint('🔔 FCM Token obtained: $token');
        return token;
      } else {
        debugPrint('🔔 User declined or has not accepted notification permission');
      }
    } catch (e) {
      debugPrint('🔔 Error getting FCM token: $e');
    }
    return null;
  }

  /// Start listening to dynamic token changes at runtime
  void listenToTokenRefresh(String userId) {
    _tokenRefreshSubscription?.cancel();
    _tokenRefreshSubscription = _messaging.onTokenRefresh.listen((newToken) async {
      debugPrint('🔔 FCM Token refreshed: $newToken');
      if (_lastToken != null && _lastToken != newToken) {
        // Remove the old token from Firestore gateways
        await removeTokenFromUserGateways(userId, token: _lastToken);
      }
      // Add the new token to Firestore gateways
      await addTokenToUserGateways(userId, token: newToken);
    });
  }

  /// Stop listening to token changes
  void stopListeningToTokenRefresh() {
    _tokenRefreshSubscription?.cancel();
    _tokenRefreshSubscription = null;
    debugPrint('🔔 Stopped listening to FCM token refresh');
  }

  /// Lấy danh sách gateways liên kết với user (qua userId hoặc qua thành viên Home)
  Future<List<DocumentSnapshot>> _getUserGateways(String userId) async {
    List<DocumentSnapshot> gateways = [];

    try {
      // 1. Tìm gateways có userId trực tiếp bằng userId
      final directGateways = await _firestore
          .collection(AppConstants.colGateways)
          .where('userId', isEqualTo: userId)
          .get();
      gateways.addAll(directGateways.docs);

      // 2. Tìm gateways của các home mà user này là thành viên (giúp hỗ trợ các gateway cũ chưa có userId)
      final memberDocs = await _firestore
          .collection(AppConstants.colHomeMembers)
          .where('userId', isEqualTo: userId)
          .get();

      if (memberDocs.docs.isNotEmpty) {
        final homeIds = memberDocs.docs
            .map((doc) => doc.data()['homeId'] as String)
            .toList();

        for (String homeId in homeIds) {
          final homeGateways = await _firestore
              .collection(AppConstants.colGateways)
              .where('homeId', isEqualTo: homeId)
              .get();

          for (var doc in homeGateways.docs) {
            // Tránh trùng lặp
            if (!gateways.any((g) => g.id == doc.id)) {
              gateways.add(doc);
            }
          }
        }
      }
    } catch (e) {
      debugPrint('🔔 Error querying gateways for user $userId: $e');
    }

    return gateways;
  }

  /// Add FCM token to all gateways managed by the user
  Future<void> addTokenToUserGateways(String userId, {String? token}) async {
    try {
      final tokenToUse = token ?? await getFcmToken();
      if (tokenToUse == null) return;

      _lastToken = tokenToUse;

      final gateways = await _getUserGateways(userId);

      if (gateways.isEmpty) {
        debugPrint('🔔 No gateways found for user $userId to add FCM token.');
        return;
      }

      final batch = _firestore.batch();
      for (var doc in gateways) {
        batch.update(doc.reference, {
          'fcm_tokens': FieldValue.arrayUnion([tokenToUse])
        });
      }
      await batch.commit();
      debugPrint('🔔 Added FCM token to ${gateways.length} gateways for user $userId');
    } catch (e) {
      debugPrint('🔔 Error adding FCM token to user gateways: $e');
    }
  }

  /// Remove FCM token from all gateways managed by the user
  Future<void> removeTokenFromUserGateways(String userId, {String? token}) async {
    try {
      final tokenToUse = token ?? _lastToken ?? await _messaging.getToken();
      if (tokenToUse == null) return;

      final gateways = await _getUserGateways(userId);

      if (gateways.isEmpty) {
        debugPrint('🔔 No gateways found for user $userId to remove FCM token.');
        return;
      }

      final batch = _firestore.batch();
      for (var doc in gateways) {
        batch.update(doc.reference, {
          'fcm_tokens': FieldValue.arrayRemove([tokenToUse])
        });
      }
      await batch.commit();
      debugPrint('🔔 Removed FCM token from ${gateways.length} gateways for user $userId');

      if (tokenToUse == _lastToken) {
        _lastToken = null;
      }
    } catch (e) {
      debugPrint('🔔 Error removing FCM token from user gateways: $e');
    }
  }
}
