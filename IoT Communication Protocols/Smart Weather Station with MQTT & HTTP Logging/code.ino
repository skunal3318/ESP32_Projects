#include <WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"
#include <HTTPClient.h>

#define WIFI_SSID "WIFI_NAME"
#define WIFI_PASS "PASS"

#define MQTT_SERVER "test.mosquitto.org"
#define MQTT_PORT 1883

#define DHTPIN 14
#define DHTTYPE DHT11

#define THINGSPEAK_API_KEY "Your_THINGSPEAK_API"  
#define THINGSPEAK_URL "http://api.thingspeak.com/update"

WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(115200);
  dht.begin();

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");

  client.setServer(MQTT_SERVER, MQTT_PORT);
}

void loop() {
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();

  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
  } else {
    char tempStr[8];
    char humStr[8];
    dtostrf(t, 1, 1, tempStr);
    dtostrf(h, 1, 1, humStr);

    client.publish("/esp32/dht22/temp", tempStr);
    client.publish("/esp32/dht22/humidity", humStr);

    Serial.print("Temperature: "); Serial.print(tempStr);
    Serial.print(" °C | Humidity: "); Serial.println(humStr);

    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      String url = String(THINGSPEAK_URL) + "?api_key=" + THINGSPEAK_API_KEY 
                   + "&field1=" + tempStr + "&field2=" + humStr;
      http.begin(url);
      int httpResponseCode = http.GET();
      if (httpResponseCode > 0) {
        Serial.print("ThingSpeak Response: ");
        Serial.println(httpResponseCode);
      } else {
        Serial.print("Error sending to ThingSpeak: ");
        Serial.println(httpResponseCode);
      }
      http.end();
    }
  }

  delay(5000); 
}

void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    if (client.connect("ESP32Client")) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 2 seconds");
      delay(2000);
    }
  }
}
