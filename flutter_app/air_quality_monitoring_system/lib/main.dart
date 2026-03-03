import 'dart:async';

import 'package:air_quality_monitoring_system/firebase_options.dart';
import 'package:firebase_auth/firebase_auth.dart';
import 'package:firebase_core/firebase_core.dart';
import 'package:firebase_database/firebase_database.dart';
import 'package:flutter/material.dart';
import 'package:fl_chart/fl_chart.dart';

import 'package:intl/intl.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();

  await Firebase.initializeApp(options: DefaultFirebaseOptions.currentPlatform);
  runApp(const MyApp());
}

class MyApp extends StatelessWidget {
  const MyApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        useMaterial3: true,
        colorSchemeSeed: Colors.blue,
        brightness: Brightness.dark,
      ),
      home: const DustDashboard(),
    );
  }
}

class DustDashboard extends StatefulWidget {
  const DustDashboard({super.key});

  @override
  State<DustDashboard> createState() => _DustDashboardState();
}

class _DustDashboardState extends State<DustDashboard> {
  Map<String, dynamic>? latestData;
  String lastUpdate = "Loading...";
  final formatter = DateFormat('dd/MMM/yyyy HH:mm:ss');

  String selectedType = "PM2.5";

  Map<String, double> currentValues = {"PM1.0": 0, "PM2.5": 0, "PM10": 0};
  Map<String, List<double>> history = {"PM1.0": [], "PM2.5": [], "PM10": []};
  StreamSubscription<DatabaseEvent>? _subscription;

  @override
  void initState() {
    super.initState();
    init();
  }

  @override
  void dispose() {
    _subscription?.cancel();
    super.dispose();
  }

  List<double> extractList(List<Map<String, dynamic>> records, String key) {
    return records.map<double>((e) => (e[key] ?? 0).toDouble()).toList();
  }

  Future<void> init() async {
    await FirebaseAuth.instance.signInAnonymously();
    readData();
  }

  void readData() {
    final ref = FirebaseDatabase.instance
        .ref("logs/sps30")
        .orderByKey()
        .limitToLast(10);

    _subscription = ref.onValue.listen((event) {
      if (!mounted) return;
      if (event.snapshot.value == null) return;

      final data = Map<String, dynamic>.from(event.snapshot.value as Map);

      List<Map<String, dynamic>> records = data.values
          .map((e) => Map<String, dynamic>.from(e))
          .toList();

      records.sort(
        (a, b) => (a["timestamp"] ?? 0).compareTo(b["timestamp"] ?? 0),
      );

      final latest = records.last;

      int ts = latest["timestamp"] ?? 0;

      DateTime time = DateTime.fromMillisecondsSinceEpoch(
        ts * 1000,
        isUtc: true,
      ).toLocal();

      setState(() {
        latestData = latest;
        lastUpdate = formatter.format(time);

        currentValues["PM1.0"] = (latest["mc_1p0"] ?? 0).toDouble();
        currentValues["PM2.5"] = (latest["mc_2p5"] ?? 0).toDouble();
        currentValues["PM10"] = (latest["mc_10p0"] ?? 0).toDouble();

        history["PM1.0"] = extractList(records, "mc_1p0");
        history["PM2.5"] = extractList(records, "mc_2p5");
        history["PM10"] = extractList(records, "mc_10p0");
      });
    });
  }

  bool isOnline() {
    if (latestData == null) return false;

    int ts = latestData!["timestamp"] ?? 0;
    final dataTime = DateTime.fromMillisecondsSinceEpoch(
      ts * 1000,
      isUtc: true,
    ).toLocal();

    return DateTime.now().difference(dataTime).inMinutes <= 20;
  }

  Color getAQIColor(double value) {
    if (value <= 25) return Colors.green;
    if (value <= 50) return Colors.yellow;
    if (value <= 100) return Colors.orange;
    return Colors.red;
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text("Air Quality Dashboard"),
        actions: [
          Padding(
            padding: const EdgeInsets.only(right: 16),
            child: Row(
              children: [
                Icon(
                  Icons.circle,
                  size: 12,
                  color: isOnline() ? Colors.green : Colors.red,
                ),
                const SizedBox(width: 6),
                Text(isOnline() ? "Online" : "Offline"),
              ],
            ),
          ),
        ],
      ),
      body: latestData == null
          ? const Center(child: CircularProgressIndicator())
          : Column(
              children: [
                // 🔵 Top Cards
                Padding(
                  padding: const EdgeInsets.all(12),
                  child: Row(
                    children: currentValues.keys.map((type) {
                      final isSelected = type == selectedType;
                      return Expanded(
                        child: GestureDetector(
                          onTap: () {
                            setState(() {
                              selectedType = type;
                            });
                          },
                          child: Card(
                            elevation: isSelected ? 8 : 2,
                            color: isSelected
                                ? getAQIColor(currentValues[type] ?? 0)
                                : Colors.grey[900],
                            child: Padding(
                              padding: const EdgeInsets.symmetric(vertical: 20),
                              child: Column(
                                children: [
                                  Text(
                                    type,
                                    style: const TextStyle(fontSize: 16),
                                  ),
                                  const SizedBox(height: 8),
                                  Text(
                                    currentValues[type]!.toStringAsFixed(1),
                                    style: const TextStyle(
                                      fontSize: 24,
                                      fontWeight: FontWeight.bold,
                                    ),
                                  ),
                                ],
                              ),
                            ),
                          ),
                        ),
                      );
                    }).toList(),
                  ),
                ),
                Padding(
                  padding: const EdgeInsets.only(bottom: 8),
                  child: Text(
                    "Last update: $lastUpdate",
                    style: const TextStyle(color: Colors.grey),
                  ),
                ),
                // 🔵 Chart
                Expanded(
                  child: Padding(
                    padding: const EdgeInsets.all(16),
                    child: Card(
                      elevation: 6,
                      child: Padding(
                        padding: const EdgeInsets.all(16),
                        child: LineChart(
                          LineChartData(
                            gridData: FlGridData(show: true),
                            borderData: FlBorderData(show: false),
                            titlesData: FlTitlesData(show: false),
                            lineTouchData: LineTouchData(
                              touchTooltipData: LineTouchTooltipData(),
                            ),
                            lineBarsData: [
                              LineChartBarData(
                                spots: (history[selectedType] ?? [])
                                    .asMap()
                                    .entries
                                    .map(
                                      (e) => FlSpot(e.key.toDouble(), e.value),
                                    )
                                    .toList(),
                                isCurved: true,
                                gradient: const LinearGradient(
                                  colors: [Colors.blue, Colors.cyan],
                                ),
                                barWidth: 4,
                                dotData: FlDotData(show: false),
                                belowBarData: BarAreaData(
                                  show: true,
                                  gradient: LinearGradient(
                                    colors: [
                                      Colors.blue.withOpacity(0.3),
                                      Colors.transparent,
                                    ],
                                    begin: Alignment.topCenter,
                                    end: Alignment.bottomCenter,
                                  ),
                                ),
                              ),
                            ],
                          ),
                          duration: const Duration(milliseconds: 300),
                        ),
                      ),
                    ),
                  ),
                ),
              ],
            ),
    );
  }
}
