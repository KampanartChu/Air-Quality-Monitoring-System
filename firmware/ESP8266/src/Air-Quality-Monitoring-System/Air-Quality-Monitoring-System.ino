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
String extractValue(const String &source, const char *key) {

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

  String url =
    String("https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=") + API_KEY;

  https.useHTTP10(true);   // ลด RAM
  https.setTimeout(5000);  // 5 วินาที
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

  String url =
    String("https://securetoken.googleapis.com/v1/token?key=") + API_KEY;

  https.useHTTP10(true);
  https.setTimeout(5000);  // 5 วินาที
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
    if (!refreshFirebase()) {
      currentState = STATE_LOGIN;  // กลับไป login ใหม่
    }
  }
}


/* =========================================================
   WRITE DATA TO FIREBASE
   ========================================================= */
void pushData(const String &path, const String &json) {

  if (currentState != STATE_RUNNING) return;
  if (idToken.length() < 50) return;  // token ปกติยาวมาก

  checkToken();

  String url =
    String(DATABASE_URL) + path + ".json?auth=" + idToken;

  https.useHTTP10(true);
  https.setTimeout(5000);  // 5 วินาที
  https.begin(client, url);
  https.addHeader("Content-Type", "application/json");

  // int code = https.PUT(value);
  int code = https.POST(json);

  if (code == 401) {
    Serial.println("Token Invalid → Relogin");
    currentState = STATE_LOGIN;
  } else if (code != HTTP_CODE_OK) {
    Serial.printf("PUT Fail: %d\n", code);
  }

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

  client.setTrustAnchors(&cert);
  client.setBufferSizes(512, 512);  // ลด RAM usage

  idToken.reserve(1200);
  refreshToken.reserve(600);

  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);

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

      static unsigned long timeStart = 0;  // << ประกาศตรงนี้

      if (WiFi.status() != WL_CONNECTED) {
        currentState = STATE_WIFI;
        break;
      }

      // เริ่มจับเวลาเมื่อเข้า STATE_TIME ครั้งแรก
      if (timeStart == 0) {
        timeStart = millis();
      }

      // เริ่ม NTP ครั้งเดียว
      if (!ntpStarted) {
        Serial.println("Start NTP");
        configTime(0, 0, "pool.ntp.org", "time.nist.gov");
        // configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov"); //Thai offset
        ntpStarted = true;
      }

      // ได้เวลาแล้ว
      if (time(nullptr) > 1000000000) {
        Serial.println("TIME OK");
        timeStart = 0;  // รีเซ็ตตัวจับเวลา
        currentState = STATE_LOGIN;
        break;
      }

      // ⛔ Timeout 10 วินาที
      if (millis() - timeStart > 10000) {
        Serial.println("TIME FAIL → Retry");
        ntpStarted = false;
        timeStart = 0;  // รีเซ็ตเพื่อเริ่มใหม่
      }

      break;


      /* ================= LOGIN ================= */
    case STATE_LOGIN:

      static unsigned long lastLoginAttempt = 0;

      if (millis() - lastLoginAttempt > 2000) {
        lastLoginAttempt = millis();
        if (loginFirebase()) {
          Serial.println("LOGIN OK");
          currentState = STATE_RUNNING;
        } else {
          Serial.println("LOGIN FAIL");
        }
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
      if (millis() - lastSend > 10000) {  //900000 = 15 min
        lastSend = millis();

        char json[256];
        time_t now = time(nullptr);

        snprintf(json, sizeof(json),
                 "{\"nc_0p5\":%.2f,"
                 "\"nc_1p0\":%.2f,"
                 "\"nc_2p5\":%.2f,"
                 "\"nc_4p0\":%.2f,"
                 "\"nc_10p0\":%.2f,"
                 "\"typical_size\":%.2f,"
                 "\"timestamp\":%lu}",
                0.5,//  m.nc_0p5,
                1.0,//  m.nc_1p0,
                2.5,//  m.nc_2p5,
                4.0,//  m.nc_4p0,
                10.0,//  m.nc_10p0,
                99,//  m.typical_particle_size,
                 now);

        // writeData("esp/value", "30");
        pushData("logs/sps30", String(json));
        Serial.println("SEND OK");
      }

      // Debug Heap
      static unsigned long lastHeap = 0;
      if (millis() - lastHeap > 60000) {
        lastHeap = millis();
        Serial.printf("Heap: %u\n", ESP.getFreeHeap());
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