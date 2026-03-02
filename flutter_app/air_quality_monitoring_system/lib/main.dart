import 'package:flutter/material.dart';
import 'package:firebase_core/firebase_core.dart';
import 'package:firebase_auth/firebase_auth.dart';
import 'package:firebase_database/firebase_database.dart';
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
  String value = "Loading...";

  @override
  void initState() {
    super.initState();
    init();
  }

  Future<void> init() async {
    await auth(); // รอ login เสร็จ
    readData(); // ค่อยอ่านข้อมูล
  }

  Future<void> auth() async {
    try {
      await FirebaseAuth.instance.signInAnonymously();
      print("Login success");
    } catch (e) {
      print("Auth error: $e");
    }
  }

  void readData() {
    final ref = FirebaseDatabase.instance.ref("esp/value");

    ref.onValue.listen((event) {
      setState(() {
        value = event.snapshot.value?.toString() ?? "No Data";
      });
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text("ESP32 Monitor")),
      body: Center(
        child: Text("ESP Value: $value", style: const TextStyle(fontSize: 32)),
      ),
    );
  }
}
