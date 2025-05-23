#ifndef EASY_MQTT_H
#define EASY_MQTT_H

#include <WiFi101.h>
#include <PubSubClient.h>

class EasyMQTT {
public:
  // ── Public API ────────────────────────────────────────────────
  EasyMQTT(const char* ssid,
            const char* pass,
            IPAddress   brokerIP,
            uint16_t    brokerPort,
            const char* clientId)
    : _ssid(ssid), _pass(pass),
      _brokerIP(brokerIP), _brokerPort(brokerPort),
      _clientId(clientId), _mqtt(_wifiClient) {}

  /// WiFi verbinden, MQTT konfigurieren und (erstmalig) verbinden
  void begin() {
    Serial.begin(115200);
    while (!Serial) ;

    _connectWiFi();
    _mqtt.setServer(_brokerIP, _brokerPort);
    _connectMQTT();
  }

  /// Muss in loop() aufgerufen werden, sorgt für Reconnect & Ping
  void mqtt_loop() {
    if (!_mqtt.connected()) {
      _connectMQTT();
    }
    _mqtt.loop();
  }

  PubSubClient& client() { 
    return _mqtt; 
  }

private:
  // ── Hilfsfunktionen ───────────────────────────────────────────
  void _connectWiFi() {
    Serial.println("connecting to WIFI...");
    WiFi.begin(_ssid, _pass);
    if (WiFi.status() != WL_CONNECTED) {
      delay(5000);
      if (WiFi.status() != WL_CONNECTED) {
        _connectWiFi();
      }
      return;
    }
    Serial.print("\nIP: ");
    Serial.println( IPAddress(WiFi.localIP()) );
  }

  void _connectMQTT() {
    while (!_mqtt.connected()) {
      Serial.println("connecting to MQTT… ");
      if (_mqtt.connect(_clientId)) {
        Serial.println("success");
        _mqtt.publish("MKRobot/state", "online", true);
      } else {
        Serial.print("Fehler rc=");
        Serial.print(_mqtt.state());
        Serial.println(" – neuer Versuch in 5 s");
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

#endif  // ROBOT_MQTT_H
