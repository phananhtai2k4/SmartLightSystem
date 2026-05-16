import 'package:cloud_firestore/cloud_firestore.dart';

class HistoryEventModel {
  final String id;
  final DateTime timestamp;
  final int type;
  final bool onOff;

  HistoryEventModel({
    required this.id,
    required this.timestamp,
    required this.type,
    required this.onOff,
  });

  factory HistoryEventModel.fromFirestore(DocumentSnapshot doc) {
    final data = doc.data() as Map<String, dynamic>? ?? {};
    return HistoryEventModel(
      id: doc.id,
      timestamp: (data['timestamp'] as Timestamp?)?.toDate() ?? DateTime.now(),
      type: data['type'] as int? ?? 0,
      onOff: data['onoff'] as bool? ?? false,
    );
  }
}
