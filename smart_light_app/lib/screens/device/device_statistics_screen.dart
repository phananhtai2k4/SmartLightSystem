import 'package:flutter/material.dart';
import 'package:cloud_firestore/cloud_firestore.dart';
import 'package:syncfusion_flutter_charts/charts.dart';
import 'package:intl/intl.dart';
import '../../constants.dart';
import '../../models/node_model.dart';
import '../../models/statistic_model.dart';

enum TimeRange { h1, h24, d7, d30 }

class DeviceStatisticsScreen extends StatefulWidget {
  final NodeModel node;

  const DeviceStatisticsScreen({super.key, required this.node});

  @override
  State<DeviceStatisticsScreen> createState() => _DeviceStatisticsScreenState();
}

class _DeviceStatisticsScreenState extends State<DeviceStatisticsScreen> {
  TimeRange _selectedRange = TimeRange.h1;
  late ZoomPanBehavior _zoomPanBehavior;

  @override
  void initState() {
    super.initState();
    _zoomPanBehavior = ZoomPanBehavior(
      enablePinching: true,
      enablePanning: true,
      zoomMode: ZoomMode.x,
    );
  }

  DateTime _truncateTo(DateTime dt, TimeRange range) {
    if (range == TimeRange.h1) {
      return DateTime(dt.year, dt.month, dt.day, dt.hour, dt.minute);
    } else if (range == TimeRange.h24) {
      int m = (dt.minute / 10).floor() * 10;
      return DateTime(dt.year, dt.month, dt.day, dt.hour, m);
    } else if (range == TimeRange.d7) {
      return DateTime(dt.year, dt.month, dt.day, dt.hour);
    } else {
      int h = (dt.hour / 4).floor() * 4;
      return DateTime(dt.year, dt.month, dt.day, h);
    }
  }

  Stream<QuerySnapshot> _buildStream() {
    final now = DateTime.now();
    DateTime start;
    String collection;
    String timeField;

    switch (_selectedRange) {
      case TimeRange.h1:
        start = now.subtract(const Duration(hours: 1));
        collection = 'measurements_raw';
        timeField = 'minute';
        break;
      case TimeRange.h24:
        start = now.subtract(const Duration(hours: 24));
        collection = 'measurements_10m';
        timeField = 'timestamp';
        break;
      case TimeRange.d7:
        start = now.subtract(const Duration(days: 7));
        collection = 'measurements_1h';
        timeField = 'timestamp';
        break;
      case TimeRange.d30:
        start = now.subtract(const Duration(days: 30));
        collection = 'measurements_4h';
        timeField = 'timestamp';
        break;
    }

    return FirebaseFirestore.instance
        .collection('nodes')
        .doc(widget.node.id)
        .collection(collection)
        .where(timeField, isGreaterThanOrEqualTo: start)
        .orderBy(timeField, descending: false)
        .snapshots();
  }

  List<ChartData> _parseData(QuerySnapshot snapshot) {
    final now = DateTime.now();
    DateTime start;
    int steps;
    Duration stepDuration;

    switch (_selectedRange) {
      case TimeRange.h1:
        start = now.subtract(const Duration(hours: 1));
        steps = 60;
        stepDuration = const Duration(minutes: 1);
        break;
      case TimeRange.h24:
        start = now.subtract(const Duration(hours: 24));
        steps = 144; // 24 * 6
        stepDuration = const Duration(minutes: 10);
        break;
      case TimeRange.d7:
        start = now.subtract(const Duration(days: 7));
        steps = 168; // 7 * 24
        stepDuration = const Duration(hours: 1);
        break;
      case TimeRange.d30:
        start = now.subtract(const Duration(days: 30));
        steps = 180; // 30 * 6
        stepDuration = const Duration(hours: 4);
        break;
    }

    final startTrunc = _truncateTo(start, _selectedRange);
    List<ChartData> finalData = [];

    if (_selectedRange == TimeRange.h1) {
      Map<DateTime, List<ChartData>> rawMap = {};
      for (var doc in snapshot.docs) {
        final d = doc.data() as Map<String, dynamic>;
        final ts = (d['minute'] as Timestamp?)?.toDate();
        if (ts == null) continue;
        final baseMin = DateTime(ts.year, ts.month, ts.day, ts.hour, ts.minute);

        List<ChartData> minPts = [];
        final arr = d['data'] as List<dynamic>? ?? [];
        for (var item in arr) {
          final t = item['t'] as int? ?? 0;
          final light = (item['light'] ?? 0).toDouble();
          final current = (item['current'] ?? 0).toDouble();
          minPts.add(
            ChartData(baseMin.add(Duration(seconds: t)), current, light),
          );
        }
        rawMap[baseMin] = minPts;
      }

      for (int i = 0; i <= steps; i++) {
        final expected = startTrunc.add(Duration(minutes: i));
        if (rawMap.containsKey(expected) && rawMap[expected]!.isNotEmpty) {
          finalData.addAll(rawMap[expected]!);
        } else {
          finalData.add(ChartData(expected, 0, 0));
        }
      }
    } else {
      Map<DateTime, ChartData> aggMap = {};
      for (var doc in snapshot.docs) {
        final d = doc.data() as Map<String, dynamic>;
        final ts = (d['timestamp'] as Timestamp?)?.toDate();
        if (ts == null) continue;
        final base = _truncateTo(ts, _selectedRange);
        final light = (d['avg_light'] ?? 0).toDouble();
        final current = (d['avg_current'] ?? 0).toDouble();
        aggMap[base] = ChartData(base, current, light);
      }

      for (int i = 0; i <= steps; i++) {
        final expected = startTrunc.add(stepDuration * i);
        if (aggMap.containsKey(expected)) {
          finalData.add(aggMap[expected]!);
        } else {
          finalData.add(ChartData(expected, 0, 0));
        }
      }
    }

    return finalData;
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Thống kê thiết bị'),
        backgroundColor: AppConstants.primaryColor,
        foregroundColor: Colors.white,
      ),
      body: Column(
        children: [
          _buildPillButtons(),
          Expanded(
            child: StreamBuilder<QuerySnapshot>(
              stream: _buildStream(),
              builder: (context, snapshot) {
                if (snapshot.connectionState == ConnectionState.waiting) {
                  return const Center(child: CircularProgressIndicator());
                }
                if (snapshot.hasError) {
                  return Center(
                    child: Text('Lỗi tải dữ liệu: ${snapshot.error}'),
                  );
                }

                final rawData = snapshot.data;
                List<ChartData> data = [];
                if (rawData != null) {
                  data = _parseData(rawData);
                }

                return ListView(
                  padding: const EdgeInsets.all(16),
                  children: [
                    _buildChartCard(
                      title: 'Dòng điện hiện tại',
                      data: data,
                      isLight: false,
                    ),
                    const SizedBox(height: 24),
                    _buildChartCard(
                      title: 'Ánh sáng môi trường',
                      data: data,
                      isLight: true,
                    ),
                  ],
                );
              },
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildPillButtons() {
    return Container(
      padding: const EdgeInsets.all(16),
      width: double.infinity,
      child: SegmentedButton<TimeRange>(
        segments: const [
          ButtonSegment(value: TimeRange.h1, label: Text('1H')),
          ButtonSegment(value: TimeRange.h24, label: Text('24H')),
          ButtonSegment(value: TimeRange.d7, label: Text('7D')),
          ButtonSegment(value: TimeRange.d30, label: Text('30D')),
        ],
        selected: {_selectedRange},
        onSelectionChanged: (Set<TimeRange> newSelection) {
          setState(() {
            _selectedRange = newSelection.first;
          });
        },
      ),
    );
  }

  Widget _buildChartCard({
    required String title,
    required List<ChartData> data,
    required bool isLight,
  }) {
    DateTimeIntervalType intervalType;
    DateFormat dateFormat;

    switch (_selectedRange) {
      case TimeRange.h1:
        intervalType = DateTimeIntervalType.minutes;
        dateFormat = DateFormat('HH:mm');
        break;
      case TimeRange.h24:
        intervalType = DateTimeIntervalType.hours;
        dateFormat = DateFormat('HH:mm');
        break;
      case TimeRange.d7:
        intervalType = DateTimeIntervalType.days;
        dateFormat = DateFormat('dd/MM');
        break;
      case TimeRange.d30:
        intervalType = DateTimeIntervalType.days;
        dateFormat = DateFormat('dd/MM');
        break;
    }

    // Prepare Colors
    final gradientColors = isLight
        ? [Colors.orange.withOpacity(0.3), Colors.orange.withOpacity(0.01)]
        : [Colors.blue.withOpacity(0.3), Colors.blue.withOpacity(0.01)];

    return Card(
      elevation: 4,
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(16)),
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                Icon(
                  isLight ? Icons.light_mode : Icons.electric_bolt,
                  color: isLight ? Colors.orange : Colors.blue,
                ),
                const SizedBox(width: 8),
                Text(
                  title,
                  style: const TextStyle(
                    fontSize: 18,
                    fontWeight: FontWeight.bold,
                  ),
                ),
              ],
            ),
            const SizedBox(height: 16),
            SizedBox(
              height: 250,
              child: SfCartesianChart(
                zoomPanBehavior: _zoomPanBehavior,
                trackballBehavior: TrackballBehavior(
                  enable: true,
                  activationMode: ActivationMode.singleTap,
                  lineType: TrackballLineType.vertical,
                  tooltipSettings: InteractiveTooltip(
                    enable: true,
                    format: isLight
                        ? 'point.x : point.y lux'
                        : 'point.x : point.y mA',
                  ),
                ),
                primaryXAxis: DateTimeAxis(
                  intervalType: intervalType,
                  dateFormat: dateFormat,
                  majorGridLines: const MajorGridLines(width: 0),
                  edgeLabelPlacement: EdgeLabelPlacement.shift,
                ),
                primaryYAxis: NumericAxis(
                  axisLine: const AxisLine(width: 0),
                  majorTickLines: const MajorTickLines(size: 0),
                  labelFormat: isLight ? '{value} lux' : '{value} mA',
                ),
                series: <CartesianSeries>[
                  SplineAreaSeries<ChartData, DateTime>(
                    dataSource: data,
                    xValueMapper: (ChartData data, _) => data.time,
                    yValueMapper: (ChartData data, _) =>
                        isLight ? data.light : data.current,
                    gradient: LinearGradient(
                      colors: gradientColors,
                      begin: Alignment.topCenter,
                      end: Alignment.bottomCenter,
                    ),
                    borderColor: isLight ? Colors.orange : Colors.blue,
                    borderWidth: 2,
                    animationDuration: 1000,
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }
}
