#include <ESP8266WiFi.h>
#include <WiFiClientSecureBearSSL.h>
#include <ESP8266HTTPClient.h>
#include <time.h>
#include "Config_private.h"

/* =========================================================
   ROOT CA (Google Trust Services - GTS Root R1)
   ใช้สำหรับยืนยัน SSL ของ Firebase
   ========================================================= */

static const char rootCACert[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFVzCCAz+gAwIBAgINAgPlk28xsBNJiGuiFzANBgkqhkiG9w0BAQwFADBHMQsw
CQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzEU
MBIGA1UEAxMLR1RTIFJvb3QgUjEwHhcNMTYwNjIyMDAwMDAwWhcNMzYwNjIyMDAw
MDAwWjBHMQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZp
Y2VzIExMQzEUMBIGA1UEAxMLR1RTIFJvb3QgUjEwggIiMA0GCSqGSIb3DQEBAQUA
A4ICDwAwggIKAoICAQC2EQKLHuOhd5s73L+UPreVp0A8of2C+X0yBoJx9vaMf/vo
27xqLpeXo4xL+Sv2sfnOhB2x+cWX3u+58qPpvBKJXqeqUqv4IyfLpLGcY9vXmX7w
Cl7raKb0xlpHDU0QM+NOsROjyBhsS+z8CZDfnWQpJSMHobTSPS5g4M/SCYe7zUjw
TcLCeoiKu7rPWRnWr4+wB7CeMfGCwcDfLqZtbBkOtdh+JhpFAz2weaSUKK0Pfybl
qAj+lug8aJRT7oM6iCsVlgmy4HqMLnXWnOunVmSPlk9orj2XwoSPwLxAwAtcvfaH
szVsrBhQf4TgTM2S0yDpM7xSma8ytSmzJSq0SPly4cpk9+aCEI3oncKKiPo4Zor8
Y/kB+Xj9e1x3+naH+uzfsQ55lVe0vSbv1gHR6xYKu44LtcXFilWr06zqkUspzBmk
MiVOKvFlRNACzqrOSbTqn3yDsEB750Orp2yjj32JgfpMpf/VjsPOS+C12LOORc92
wO1AK/1TD7Cn1TsNsYqiA94xrcx36m97PtbfkSIS5r762DL8EGMUUXLeXdYWk70p
aDPvOmbsB4om3xPXV2V4J95eSRQAogB/mqghtqmxlbCluQ0WEdrHbEg8QOB+DVrN
VjzRlwW5y0vtOUucxD/SVRNuJLDWcfr0wbrM7Rv1/oFB2ACYPTrIrnqYNxgFlQID
AQABo0IwQDAOBgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4E
FgQU5K8rJnEaK0gnhS9SZizv8IkTcT4wDQYJKoZIhvcNAQEMBQADggIBAJ+qQibb
C5u+/x6Wki4+omVKapi6Ist9wTrYggoGxval3sBOh2Z5ofmmWJyq+bXmYOfg6LEe
QkEzCzc9zolwFcq1JKjPa7XSQCGYzyI0zzvFIoTgxQ6KfF2I5DUkzps+GlQebtuy
h6f88/qBVRRiClmpIgUxPoLW7ttXNLwzldMXG+gnoot7TiYaelpkttGsN/H9oPM4
7HLwEXWdyzRSjeZ2axfG34arJ45JK3VmgRAhpuo+9K4l/3wV3s6MJT/KYnAK9y8J
ZgfIPxz88NtFMN9iiMG1D53Dn0reWVlHxYciNuaCp+0KueIHoI17eko8cdLiA6Ef
MgfdG+RCzgwARWGAtQsgWSl4vflVy2PFPEz0tv/bal8xa5meLMFrUKTX5hgUvYU/
Z6tGn6D/Qqc6f1zLXbBwHSs09dR2CQzreExZBfMzQsNhFRAbd03OIozUhfJFfbdT
6u9AWpQKXCBfTkBdYiJ23//OYb2MI3jSNwLgjt7RETeJ9r/tSQdirpLsQBqvFAnZ
0E6yove+7u7Y/9waLd64NnHi/Hm3lCXRSHNboTXns5lndcEZOitHTtNCjv0xyBZm
2tIMPNuzjsmhDYAPexZ3FL//2wmUspO8IFgV6dtxQ/PeEMMA3KgqlbbC1j+Qa3bb
bP6MvPJwNQzcmRk13NfIRmPVNnGuV/u3gm3c
-----END CERTIFICATE-----
)EOF";

static BearSSL::X509List cert(rootCACert);

/* =========================================================
   GLOBAL VARIABLES
   ========================================================= */

BearSSL::WiFiClientSecure client;
HTTPClient https;

// Firebase Tokens
String idToken;
String refreshToken;

// เวลา token หมดอายุ
unsigned long tokenExpire = 0;

// เวลาเขียนข้อมูลล่าสุด
unsigned long lastSend = 0;

// เวลาเริ่มบูต (ใช้รีบูตทุก 24 ชม.)
unsigned long bootTime = 0;

// ใช้สำหรับ retry WiFi ทุก 5 วินาที
unsigned long lastAttempt = 0;

// ใช้ตรวจว่าเริ่ม NTP แล้วหรือยัง
bool ntpStarted = false;


/* =========================================================
   STATE MACHINE
   ========================================================= */

enum SystemState {
  STATE_WIFI,    // กำลังเชื่อม WiFi
  STATE_TIME,    // กำลัง Sync เวลา
  STATE_LOGIN,   // Login Firebase
  STATE_RUNNING  // ทำงานปกติ
};

SystemState currentState = STATE_WIFI;


/* =========================================================
   STRING PARSER (ดึงค่าจาก JSON แบบประหยัด RAM)
   ========================================================= */

String extractValue(String source, const char *key) {

  int start = source.indexOf(key);
  if (start == -1) return "";

  start = source.indexOf(":", start);
  start = source.indexOf("\"", start) + 1;
  int end = source.indexOf("\"", start);

  return source.substring(start, end);
}


/* =========================================================
   LOGIN FIREBASE
   ========================================================= */

bool loginFirebase() {

  client.setTrustAnchors(&cert);
  client.setBufferSizes(512, 512);  // ลด RAM usage

  String url =
    String("https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=") + API_KEY;

  https.useHTTP10(true);  // ลด RAM
  https.begin(client, url);
  https.addHeader("Content-Type", "application/json");

  String payload =
    String("{\"email\":\"") + USER_EMAIL + "\",\"password\":\"" + USER_PASSWORD + "\",\"returnSecureToken\":true}";

  int code = https.POST(payload);

  if (code == HTTP_CODE_OK) {

    String response = https.getString();

    idToken = extractValue(response, "idToken");
    refreshToken = extractValue(response, "refreshToken");

    tokenExpire = millis() + 3500000;  // ~58 นาที

    https.end();
    return true;
  }

  https.end();
  return false;
}


/* =========================================================
   REFRESH TOKEN
   ========================================================= */

bool refreshFirebase() {

  client.setTrustAnchors(&cert);

  String url =
    String("https://securetoken.googleapis.com/v1/token?key=") + API_KEY;

  https.useHTTP10(true);
  https.begin(client, url);
  https.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String payload =
    "grant_type=refresh_token&refresh_token=" + refreshToken;

  int code = https.POST(payload);

  if (code == HTTP_CODE_OK) {

    String response = https.getString();

    idToken = extractValue(response, "id_token");
    refreshToken = extractValue(response, "refresh_token");

    tokenExpire = millis() + 3500000;

    https.end();
    return true;
  }

  https.end();
  return false;
}


/* =========================================================
   CHECK TOKEN (เรียกก่อนใช้งาน Firebase ทุกครั้ง)
   ========================================================= */

void checkToken() {
  if (millis() > tokenExpire - 60000) {
    refreshFirebase();  // ต่ออายุล่วงหน้า 1 นาที
  }
}


/* =========================================================
   WRITE DATA TO FIREBASE
   ========================================================= */

void writeData(const String &path, const String &value) {

  checkToken();

  client.setTrustAnchors(&cert);

  String url =
    String(DATABASE_URL) + path + ".json?auth=" + idToken;

  https.useHTTP10(true);
  https.begin(client, url);
  https.addHeader("Content-Type", "application/json");

  https.PUT(value);

  https.end();
}


/* =========================================================
   SETUP
   ========================================================= */

void setup() {

  Serial.begin(115200);

  // เริ่มเชื่อม WiFi ครั้งแรก
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  bootTime = millis();
}


/* =========================================================
   LOOP (STATE MACHINE)
   ========================================================= */

void loop() {

  switch (currentState) {

      /* ================= WIFI ================= */

    case STATE_WIFI:

      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("WIFI OK");
        currentState = STATE_TIME;
        ntpStarted = false;
      } else if (millis() - lastAttempt > 5000) {
        Serial.println("Retry WiFi...");
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        lastAttempt = millis();
      }

      break;


      /* ================= TIME ================= */

    case STATE_TIME:

      if (WiFi.status() != WL_CONNECTED) {
        currentState = STATE_WIFI;
        break;
      }

      // เริ่ม NTP ครั้งเดียว
      if (!ntpStarted) {
        Serial.println("Start NTP");
        configTime(0, 0, "pool.ntp.org", "time.nist.gov");
        ntpStarted = true;
      }

      // รอจนได้เวลา Unix จริง
      if (time(nullptr) > 1000000000) {
        Serial.println("TIME OK");
        currentState = STATE_LOGIN;
      }

      break;


      /* ================= LOGIN ================= */

    case STATE_LOGIN:

      if (loginFirebase()) {
        Serial.println("LOGIN OK");
        currentState = STATE_RUNNING;
      } else {
        Serial.println("LOGIN FAIL");
        delay(2000);  // retry ช้า ๆ
      }

      break;


      /* ================= RUNNING ================= */

    case STATE_RUNNING:

      // ถ้า WiFi หลุด → กลับไปเริ่มใหม่
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi Lost");
        currentState = STATE_WIFI;
        break;
      }

      // ส่งข้อมูลทุก 10 วินาที
      if (millis() - lastSend > 10000) {
        lastSend = millis();

        writeData("esp/value", "30");
        Serial.println("SEND OK");
      }

      break;
  }

  /* ================= DAILY REBOOT ================= */

  // รีบูตทุก 24 ชั่วโมง (เพิ่มเสถียรภาพภาคสนาม)
  if (millis() - bootTime > 86400000UL) {
    Serial.println("Daily Reboot");
    delay(1000);
    ESP.restart();
  }
}