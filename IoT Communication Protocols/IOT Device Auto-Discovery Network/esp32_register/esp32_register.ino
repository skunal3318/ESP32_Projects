#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

const char* WIFI_SSID = "Airtel_Zerotouch";
const char* WIFI_PASS = "Airtel@123";

const char* SERVER_HOST = " 192.168.1.5"; // <-- change to your server LAN IP
const int SERVER_PORT = 5000;
const char* REGISTER_PATH = "/register";

const char* DEVICE_NAME = "ESP32_Sensor_1"; // change per device
const unsigned long POST_INTERVAL_MS = 30000; // send every 30s

String lastIP = "";
unsigned long lastPost = 0;

void setup() {
  Serial.begin(115200);
  delay(100);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected, IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi connection failed.");
  }
  // do an initial registration immediately
  postIfNeeded(true);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    // try reconnect
    WiFi.reconnect();
    delay(1000);
  }
  postIfNeeded(false);
  delay(1000);
}

void postIfNeeded(bool force) {
  String ip = WiFi.localIP().toString();
  unsigned long now = millis();
  bool timeDue = (now - lastPost) >= POST_INTERVAL_MS;
  bool ipChanged = (ip != lastIP);

  if (force || timeDue || ipChanged) {
    if (sendRegistration(ip)) {
      lastIP = ip;
      lastPost = now;
      Serial.printf("Registered: %s -> %s\n", DEVICE_NAME, ip.c_str());
    } else {
      Serial.println("Registration failed");
    }
  }
}

bool sendRegistration(String ip) {
  if (WiFi.status() != WL_CONNECTED) return false;

  HTTPClient http;
  String url = String("http://") + SERVER_HOST + ":" + SERVER_PORT + REGISTER_PATH;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<256> doc;
  doc["device_name"] = DEVICE_NAME;
  doc["ip"] = ip;
  doc["status"] = "online";

  String payload;
  serializeJson(doc, payload);

  int code = http.POST(payload);
  if (code > 0) {
    String resp = http.getString();
    Serial.printf("POST %d, resp: %s\n", code, resp.c_str());
  } else {
    Serial.printf("HTTP POST failed, error: %s\n", http.errorToString(code).c_str());
  }
  http.end();
  return (code >= 200 && code < 300);
}
