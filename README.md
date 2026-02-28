# 🌫️ Air Quality Monitoring System  
**SPS30 Sensor • ESP32 • Firebase • Flutter**

A full-stack IoT system for real-time air-quality monitoring, combining hardware sensing, cloud data storage, and mobile visualization. This project demonstrates end‑to‑end skills across embedded development, cloud architecture, and mobile app engineering.

---

## 🚀 Project Overview
This system measures particulate matter (PM1.0 / PM2.5 / PM4 / PM10) using the **Sensirion SPS30** sensor, sends the data to **Firebase**, and visualizes it on a **Flutter mobile application** with real‑time updates and historical charts.

System architecture:

[SPS30 Sensor] → [ESP32] → [Firebase Firestore] → [Flutter Mobile App]


---

## 🧩 Features

### 📡 Hardware (ESP32 + SPS30)
- Reads PM1.0 / PM2.5 / PM4 / PM10 values from SPS30  
- Sends data to Firebase in real time  
- Automatic WiFi reconnection  
- Lightweight JSON payloads for efficient communication  

### ☁️ Cloud (Firebase)
- Firestore database for structured time‑series data  
- Secure access via Firebase Authentication  
- Firestore indexes for fast historical queries  
- Optional Cloud Functions for daily logs or alerts  

### 📱 Mobile App (Flutter)
- Real‑time dashboard for current air‑quality values  
- Historical charts (1 hour / 24 hours / 7 days)  
- Clean, modern UI with Light/Dark mode  
- Built using `fl_chart` or `syncfusion_flutter_charts`  

---

## 📁 Repository Structure
/  
├── firmware/                # ESP32 + SPS30 firmware  
│  ├── src/  
│  ├── wiring-diagram.png  
│  └── README.md  
│  
├── firebase/                # Firebase configuration  
│   ├── firestore.rules  
│   ├── firestore.indexes.json  
│   └── example-data.json  
│  
└── flutter_app/             # Flutter mobile application  
├── lib/  
├── assets/screenshots/  
└── README.md  

---

## 🔌 Hardware Setup

| SPS30 Pin | ESP32 Pin |
|-----------|-----------|
| VIN       | 5V        |
| GND       | GND       |
| SDA       | GPIO 21   |
| SCL       | GPIO 22   |

A wiring diagram is included in `firmware/wiring-diagram.png`.

---

## 🛠️ Key Code Examples

### ESP32 → Firebase (simplified)
```cpp
HTTPClient http;
http.begin("https://your-project.firebaseio.com/air_quality.json");
http.addHeader("Content-Type", "application/json");

String payload = "{\"pm25\": " + String(pm25) + ", \"timestamp\": " + String(time(nullptr)) + "}";
http.POST(payload);
http.end();
Flutter → Firestore (real-time stream)
dart
StreamBuilder(
  stream: FirebaseFirestore.instance
      .collection('air_quality')
      .orderBy('timestamp', descending: true)
      .limit(1)
      .snapshots(),
  builder: (context, snapshot) {
    if (!snapshot.hasData) return CircularProgressIndicator();
    final data = snapshot.data!.docs.first.data();
    return Text("PM2.5: ${data['pm25']} µg/m³");
  },
);
```
---
### 📊 App Screenshots
Add screenshots in flutter_app/assets/screenshots/ and embed them here:

![Dashboard](assets/screenshots/dashboard.png)
![History Chart](assets/screenshots/history.png)
---
### 🔐 Firebase Security Rules (example)
```
match /air_quality/{docId} {
  allow read, write: if request.auth != null;
}
```
---

### 🧠 What This Project Demonstrates

- End‑to‑end IoT system design
- Embedded programming with ESP32
- Cloud architecture using Firebase
- Real‑time data streaming
- Mobile development with Flutter
- Data visualization and UI design
- Secure and scalable Firestore structure

### 📌 Future Improvements
- Push notifications when PM2.5 exceeds safe levels
- Web dashboard (Flutter Web or React)
- Batch data uploads to reduce Firestore cost
- Sensor calibration for improved accuracy

---
# 📄 License
MIT License (or your preferred license)

---
# 👤 Author
Your Name
Your Role / Interests
Email / LinkedIn / GitHub
