#include <ESP8266WiFi.h>
#include <WiFiClientSecureBearSSL.h>
#include <ESP8266HTTPClient.h>
#include <time.h>
#include <Wire.h>
#include <sps30.h>
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

// เวลาเริ่มบูต (ใช้รีบูตทุก 24 ชม.)
unsigned long bootTime = 0;

// ใช้สำหรับ retry WiFi ทุก 5 วินาที
unsigned long lastAttempt = 0;

// ใช้ตรวจว่าเริ่ม NTP แล้วหรือยัง
bool ntpStarted = false;

// Sync ตามเวลาโลกจริง
unsigned long lastLoggedSlot = 0;


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
   SPS30 STATE
   ========================================================= */
enum SPSState {
  SPS_IDLE,
  SPS_START,
  SPS_WARMUP,
  SPS_READ,
  SPS_STOP
};

SPSState spsState = SPS_IDLE;

unsigned long spsTimer = 0;
unsigned long lastSlot = 0;
int readCount = 0;

float sum_mc1 = 0;
float sum_mc25 = 0;
float sum_mc4 = 0;
float sum_mc10 = 0;

float sum_nc05 = 0;
float sum_nc1 = 0;
float sum_nc25 = 0;
float sum_nc4 = 0;
float sum_nc10 = 0;

float sum_size = 0;


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
void pushData(const String &path, const char *json) {

  checkToken();

  if (currentState != STATE_RUNNING) return;
  if (idToken.length() < 50) return;  // token ปกติยาวมาก

  String url =
    String(DATABASE_URL) + path + ".json?auth=" + idToken;

  https.useHTTP10(true);
  https.setTimeout(5000);  // 5 วินาที
  https.begin(client, url);
  https.addHeader("Content-Type", "application/json");

  int code = https.POST(json);


  if (code == 200) {
    Serial.printf("POST OK: %d\n", code);
  } else if (code == 401) {
    Serial.println("Token Invalid → Relogin");
    currentState = STATE_LOGIN;
  } else if (code != HTTP_CODE_OK) {
    Serial.printf("POST Fail: %d\n", code);
  }

  https.end();
}

/* =========================================================
   SPS30 Handle
   ========================================================= */
void handleSPS30() {

  time_t now = time(nullptr);
  if (now < 1000000000) return;

  unsigned long slot = now / 900;  // 15 นาที

  switch (spsState) {

    case SPS_IDLE:

      if (slot != lastSlot) {
        sum_mc1 = sum_mc25 = sum_mc4 = sum_mc10 = 0;
        sum_nc05 = sum_nc1 = sum_nc25 = sum_nc4 = sum_nc10 = 0;
        sum_size = 0;
        readCount = 0;

        lastSlot = slot;

        sps30_start_measurement();
        spsTimer = millis();
        spsState = SPS_WARMUP;
        Serial.println("SPS → Start Warmup");
      }
      break;

    case SPS_WARMUP:
      
      if (millis() - spsTimer >= 30000) {  // warmup 30 วิ
        readCount = 0;
        spsTimer = millis();
        spsState = SPS_READ;
        Serial.println("SPS → Reading...");
      }
      break;

    case SPS_READ:
      
      if (millis() - spsTimer >= 2000) {  // read

        struct sps30_measurement m;

        if (sps30_read_measurement(&m) == 0) {

          if (!isnan(m.mc_2p5)) {

            sum_mc1 += m.mc_1p0;
            sum_mc25 += m.mc_2p5;
            sum_mc4 += m.mc_4p0;
            sum_mc10 += m.mc_10p0;

            sum_nc05 += m.nc_0p5;
            sum_nc1 += m.nc_1p0;
            sum_nc25 += m.nc_2p5;
            sum_nc4 += m.nc_4p0;
            sum_nc10 += m.nc_10p0;

            sum_size += m.typical_particle_size;

            readCount++;
          }
        }

        spsTimer = millis();

        if (readCount >= 3) {
          spsState = SPS_STOP;
          Serial.println("SPS → Stopped");
        }
      }
      break;

    case SPS_STOP:
      
      if (readCount == 0) {
        spsState = SPS_IDLE;
        break;
      }

      sps30_stop_measurement();

      float avg_mc1 = sum_mc1 / readCount;
      float avg_mc25 = sum_mc25 / readCount;
      float avg_mc4 = sum_mc4 / readCount;
      float avg_mc10 = sum_mc10 / readCount;

      float avg_nc05 = sum_nc05 / readCount;
      float avg_nc1 = sum_nc1 / readCount;
      float avg_nc25 = sum_nc25 / readCount;
      float avg_nc4 = sum_nc4 / readCount;
      float avg_nc10 = sum_nc10 / readCount;

      float avg_size = sum_size / readCount;

      // ยิง Firestore ตรงนี้เลย
      char json[512];
      snprintf(json, sizeof(json),
               "{\"mc_1p0\":%.2f,"
               "\"mc_2p5\":%.2f,"
               "\"mc_4p0\":%.2f,"
               "\"mc_10p0\":%.2f,"
               "\"nc_0p5\":%.2f,"
               "\"nc_1p0\":%.2f,"
               "\"nc_2p5\":%.2f,"
               "\"nc_4p0\":%.2f,"
               "\"nc_10p0\":%.2f,"
               "\"typical_size\":%.2f,"
               "\"timestamp\":%ld}",
               avg_mc1,
               avg_mc25,
               avg_mc4,
               avg_mc10,
               avg_nc05,
               avg_nc1,
               avg_nc25,
               avg_nc4,
               avg_nc10,
               avg_size,
               (long)now);

      pushData("logs/sps30", json);

      struct tm *t = localtime(&now);
      char timeStr[30];
      strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", t);
      Serial.println(timeStr);

      sum_mc1 = sum_mc25 = sum_mc4 = sum_mc10 = 0;
      sum_nc05 = sum_nc1 = sum_nc25 = sum_nc4 = sum_nc10 = 0;
      sum_size = 0;
      readCount = 0;

      spsState = SPS_IDLE;
      break;
  }
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

  Wire.begin(D2, D1);  // ESP8266 I2C
  sensirion_i2c_init();
  if (sps30_probe() != 0) {
    Serial.println("SPS30 not detected at boot");
  }

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
      } else if (millis() - lastAttempt > 10000) {
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
        spsState = SPS_IDLE;  // reset sensor state
        currentState = STATE_WIFI;
        break;
      }

      handleSPS30();

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