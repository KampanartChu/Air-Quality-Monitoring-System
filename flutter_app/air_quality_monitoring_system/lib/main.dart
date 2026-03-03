import 'package:flutter/material.dart';
import 'package:firebase_core/firebase_core.dart';
import 'package:firebase_auth/firebase_auth.dart';
import 'package:firebase_database/firebase_database.dart';
import 'package:intl/intl.dart';
import 'firebase_options.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();

  await Firebase.initializeApp(options: DefaultFirebaseOptions.currentPlatform);

  runApp(const MyApp());
}

class MyApp extends StatelessWidget {
  const MyApp({super.key});

  @override
  Widget build(BuildContext context) {
    return const MaterialApp(
      debugShowCheckedModeBanner: false,
      home: HomePage(),
    );
  }
}

class HomePage extends StatefulWidget {
  const HomePage({super.key});

  @override
  State<HomePage> createState() => _HomePageState();
}

class _HomePageState extends State<HomePage> {
  Map<String, dynamic>? latestData;
  String lastUpdate = "Loading...";
  final formatter = DateFormat('dd/MMM/yyyy HH:mm:ss');

  @override
  void initState() {
    super.initState();
    init();
  }

  Future<void> init() async {
    await FirebaseAuth.instance.signInAnonymously();
    readData();
  }

  void readData() {
    final ref = FirebaseDatabase.instance
        .ref("logs/sps30")
        .orderByKey()
        .limitToLast(1);

    ref.onValue.listen((event) {
      if (!mounted) return;

      if (event.snapshot.value != null) {
        final data = event.snapshot.value as Map;
        final latest = data.values.first as Map;

        int ts = latest["timestamp"];

        DateTime time = DateTime.fromMillisecondsSinceEpoch(
          ts * 1000,
          isUtc: true,
        ).toLocal();

        setState(() {
          latestData = Map<String, dynamic>.from(latest);
          lastUpdate = formatter.format(time);
        });
      }
    });
  }

  Widget buildCard(String title, dynamic value, String unit) {
    return Card(
      elevation: 4,
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(16)),
      child: Padding(
        padding: const EdgeInsets.all(20),
        child: Column(
          children: [
            Text(
              title,
              style: const TextStyle(fontSize: 18, fontWeight: FontWeight.w600),
            ),
            const SizedBox(height: 10),
            Text(
              value?.toString() ?? "--",
              style: const TextStyle(fontSize: 36, fontWeight: FontWeight.bold),
            ),
            Text(unit, style: const TextStyle(fontSize: 16)),
          ],
        ),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text(
          "ESP8266 + SPS30 Air monitor",
          style: TextStyle(fontSize: 12),
        ),
        actions: [
          TextButton(
            onPressed: () {
              // ยังไม่ทำอะไร
            },
            child: const Text("History"),
          ),
        ],
      ),
      body: latestData == null
          ? const Center(child: CircularProgressIndicator())
          : LayoutBuilder(
              builder: (context, constraints) {
                return SingleChildScrollView(
                  child: ConstrainedBox(
                    constraints: BoxConstraints(
                      minHeight: constraints.maxHeight,
                    ),
                    child: IntrinsicHeight(
                      child: Center(
                        child: Padding(
                          padding: const EdgeInsets.all(16),
                          child: Column(
                            mainAxisAlignment: MainAxisAlignment.center,
                            children: [
                              buildCard(
                                "PM1.0",
                                latestData!["mc_1p0"],
                                "µg/m³",
                              ),
                              const SizedBox(height: 12),
                              buildCard(
                                "PM2.5",
                                latestData!["mc_2p5"],
                                "µg/m³",
                              ),
                              const SizedBox(height: 12),
                              buildCard(
                                "PM10",
                                latestData!["mc_10p0"],
                                "µg/m³",
                              ),
                              const SizedBox(height: 12),
                              buildCard(
                                "Particle Size",
                                latestData!["typical_size"],
                                "µm",
                              ),
                              const SizedBox(height: 30),
                              Text(
                                "Last Update: $lastUpdate",
                                style: const TextStyle(fontSize: 14),
                              ),
                            ],
                          ),
                        ),
                      ),
                    ),
                  ),
                );
              },
            ),
    );
  }
}
