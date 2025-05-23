// ─── EasyMQTT_ESP.h ─────────────────────────────────────────────
#ifndef EASY_MQTT_ESP8266_H
#define EASY_MQTT_ESP8266_H

#include <ESP8266WiFi.h>
#include <PubSubClient.h>

class EasyMQTT {
public:
  // Konstruktor genau wie beim MKR
  EasyMQTT(const char* ssid,
           const char* pass,
           IPAddress   brokerIP,
           uint16_t    brokerPort,
           const char* clientId)
    : _ssid(ssid), _pass(pass),
      _brokerIP(brokerIP), _brokerPort(brokerPort),
      _clientId(clientId),
      _mqtt(_wifiClient) {}

  /// WiFi & MQTT initialisieren
  void begin() {
    Serial.begin(9600);
    // kein "while(!Serial)" nötig auf ESP8266

    _connectWiFi();
    _mqtt.setServer(_brokerIP, _brokerPort);
    _connectMQTT();
  }

  /// Muss in loop() aufgerufen werden
  void mqtt_loop() {
    if (!_mqtt.connected()) {
      _connectMQTT();
    }
    _mqtt.loop();
  }

  /// Direkter Zugriff aufs PubSubClient-Objekt
  PubSubClient& client() {
    return _mqtt;
  }

private:
  // verbindet zum WLAN
  void _connectWiFi() {
    Serial.print("Connecting to WiFi ");
    Serial.print(_ssid);
    Serial.print(" …");
    WiFi.begin(_ssid, _pass);
    if (WiFi.status() != WL_CONNECTED) {
      delay(5000);
      if (WiFi.status() != WL_CONNECTED) {
        _connectWiFi();
      }
      return;
    }
    Serial.println();
    Serial.print("WiFi connected, IP: ");
    Serial.println(WiFi.localIP());
  }

  // verbindet zum MQTT-Broker
  void _connectMQTT() {
    while (!_mqtt.connected()) {
      Serial.print("Connecting to MQTT broker at ");
      Serial.print(_brokerIP);
      Serial.print(':');
      Serial.print(_brokerPort);
      Serial.print(" … ");
      if (_mqtt.connect(_clientId)) {
        Serial.println("OK");
        _mqtt.publish("ESPBot/state", "online", true);
      } else {
        Serial.print("failed, rc=");
        Serial.print(_mqtt.state());
        Serial.println(" – retry in 5s");
        delay(5000);
      }
    }
  }

  // ── Member ───────────────────────────────────────────────────
  const char*  _ssid;
  const char*  _pass;
  IPAddress    _brokerIP;
  uint16_t     _brokerPort;
  const char*  _clientId;

  WiFiClient   _wifiClient;
  PubSubClient _mqtt;
};

#endif // EASY_MQTT_ESP_H
