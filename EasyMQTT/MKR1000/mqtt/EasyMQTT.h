#ifndef EASY_MQTT_H

#define EASY_MQTT_H
#define MAX_ACTIONS 10
#define TIMEOUT 5000

#include <WiFi101.h>
#include <PubSubClient.h> 
#include <ArduinoJson.h>

class EasyMQTT {
public:
  // ── Public API ────────────────────────────────────────────────
  EasyMQTT( const char*  ssid,
            const char*  pass,
            IPAddress    brokerIP,  
            uint16_t     brokerPort,
            const char*  clientType,
            const char*  actions[],
            const size_t actionCount
        )
    : _ssid(ssid), 
      _pass(pass),
      _brokerIP(brokerIP), 
      _brokerPort(brokerPort),
      _deviceType(clientType),
      _mqtt(_wifiClient),
      _isInUse(false),
      _last(0),
      _lastPingFromClient(0)
      {
        _actionCount = min(actionCount, MAX_ACTIONS);
        for (size_t i = 0; i < _actionCount; ++i) {
          _actions[i] = actions[i];
        }
      }

  /**
   * Has to be called in setup().
   * - It establishes a connection with the wifi and the mqtt broker.
   * - It subscribes to all relevant topics including the action topics.
   * - It creates a unique identifier based on the device name provided in the constructor and its mac adress.
   * - It registers the device on <device-type>/<device-id>/register, where it sends a json file containing all possible actions and sensor data topics which can be processed by a client.
   * 
   * The registration might look like this: 
   * {
   *   id: "robot-1f21",
   *   type: "robot",
   *   actions: ["forward", "left", "right"],
   *   sensors: ["infrared"]
   * }
   */
  void begin() {
    //Serial.begin(115200);
    if (!Serial.available()) {
      delay(1000);
    }

    _connectWiFi();
    _deviceID = _getUniqueId();
    _topicPrefix = String(_deviceType) + "/" + String(_deviceID) + "/";
    _mqtt.setServer(_brokerIP, _brokerPort);
    _connectMQTT();
    _registerDevice();
  }

  /**
   * Has to be called in loop().
   * - If the connection to the broker is no longer valid, it attempts to reconnect.
   * - If five seconds have passed since the last heartbeat, a heartbeat is sent containing current status. It can be either online, offline or locked if a client is connected.
   * - If a client is connected and five seconds have passed since the last ping or action, the status is set back to online, and another client can claim the connection. 
   */
  void mqtt_loop() {
    if (!_mqtt.connected()) {
      Serial.print("connection to broker lost, ");
      _connectMQTT();
    }
    if (_elapsed(5000)) {
      _sendHeartBeat();
    }
    if (_hasTimeoutOccoured()) {
      resetControllerId();
    }
    _mqtt.loop();
  }

  void subscribe(String topicSuffix) {
    String topic = _topicPrefix + topicSuffix;
    Serial.println("subscribing to: " + topic);
    _mqtt.subscribe(topic.c_str());
  }

  bool publish(String topicSuffix, String payload, bool retained=false) {
    String topic = _topicPrefix + topicSuffix;
    if (payload != "") {
      Serial.println("publishing on " + topic + " payload: " + String(payload) + ", which is " + (retained ? "" : "not ") + "retained");
    }
    return _mqtt.publish(topic.c_str(), payload.c_str(), retained);
  }

  PubSubClient &setCallback(void (*callback)(char *, uint8_t *, unsigned int)) {
    return _mqtt.setCallback(callback);
  }

  PubSubClient& client() { 
    return _mqtt; 
  }

  const String getDeviceID() {
    return String(_deviceID);
  }

  String getTopicPrefix() {
    return _topicPrefix;
  }

  bool isInUse() {
    return _isInUse;
  }

  /**
   * When a client wants to controll the devise this code is running on, it has to first claim the connection.
   * This is done by posting the unique raw identifier of the client on the topic <device-type>/<device-id>/controll/claim.
   * If the device is not currently connected it acknowledges the connection by posting "true" on <device-type>/<device-id>/controll/acknowledge and changes its status to locked.
   * In order to maintain the connection an action or a ping must be sent within five seconds of each other.
   */
  bool manageConnection(String topic, String payload) {
    // Handle Connection
    if (!_isInUse) {    
      // Await connection
      if (topic == _topicPrefix + "control/claim" && payload != "") {
        Serial.println("Client " + payload + " claims the connection");
        _acknowledgeConnection(payload); 
        return true;
      } 
      // Do nothing if no client is connected to the robot and no handshake is being attempted
      return false;
    }

    // Measure time since last ping
    if (topic == _topicPrefix + "ping" && payload == _controllerID) {
      _lastPingFromClient = millis();
    }
    return true;
  }

  /**
   * An action sent by a client has at least a client id and optionally a msg. So it might look like this:
   * {id:"webCLient-123", message:"engage"}
   * webCLient-123:engage
   * 
   * This method returns true if the client id matches the chached id that was set when the connection was established. 
   */
  bool clientMatches(const String &actionPayload) {
    int separator = actionPayload.indexOf(':');
    String id;
    if (separator >= 0) {
      // alles vor dem Doppelpunkt
      id = actionPayload.substring(0, separator);
    } else {
      // kein Doppelpunkt → ganze Zeichenkette ist die ID
      id = actionPayload;
    }
    if (id == _controllerID) {
      _lastPingFromClient = millis();
      return true;
    }
    return false;
  }

  /**
   * An action sent by a client has at least a client id and optionally a msg. So it might look like this:
   * {id:"webCLient-123", message:"engage"}
   * webCLient-123:engage
   * 
   * actionPayload: The full json-encoded payload.
   * expectedContent: The desired decoded message
   * 
   * This method returns true if the encoded message matches the passed one.
   */
  bool payloadContentMatches(const String &actionPayload, const String &expectedContent) {
    int separator = actionPayload.indexOf(':');
    if (separator < 0) {
      // keine Nachricht enthalten
      return false;
    }
    // alles hinter dem Doppelpunkt
    String content = actionPayload.substring(separator + 1);
    return (content == expectedContent);
  }

  /*bool isAction(const String &topic) {
    return false;
  }*/

  String convertPayloadToString(uint8_t* payload, unsigned int length) {
    String payload_str;
    for (unsigned int i = 0; i < length; i++) {
      payload_str += (char)payload[i];
    }
    return payload_str;
  }

  void resetControllerId() {
    _controllerID = "";
    _isInUse = false;
    publish("control/claim", "", true);
    publish("control/acknowledge", "", true);    
  }

  bool _elapsed(unsigned long interval) {
    unsigned long now = millis();
    if (now - _last >= interval) {
      _last = now;
      return true;
    }
    return false;
  }

private:
  // ── Hilfsfunktionen ───────────────────────────────────────────
  void _connectWiFi() {
    if (String(_pass) == "") {
      WiFi.begin(_ssid);
    } else {
      WiFi.begin(_ssid, _pass);
    }
    Serial.println("connecting to WiFi...");
 
    delay(5000);
    if (WiFi.status() != WL_CONNECTED) {
      _connectWiFi();
    }   if (WiFi.status() != WL_CONNECTED) {
    return;
    }
    Serial.print("\nIP: ");
    Serial.println( IPAddress(WiFi.localIP()) );
  }

  void _connectMQTT() {
    while (!_mqtt.connected()) {
      Serial.println("connecting to MQTT… ");
      if (_mqtt.connect(_deviceID,
          /*willTopic:   */ (_topicPrefix + "status").c_str(),
          /*willQos*:    */ 0,
          /*willRetain:  */ true,
          /*willMessage: */ "{\"online\":false}")) {
        Serial.println("success");
        _subscribeToRelevantTopics();
        _sendHeartBeat();

      } else {
        Serial.print("Fehler rc=");
        Serial.print(_mqtt.state());
        Serial.println(" – neuer Versuch in 5 s");
        delay(5000);
      }
    }
  }

  bool _registerDevice() {
    if (!_mqtt.connected()) {
      Serial.println("Device registration failed: Client not connected!");
    }
    StaticJsonDocument<256> doc;

    doc["id"] = _deviceID;
    doc["type"] = _deviceType;

    JsonArray actionsArray = doc.createNestedArray("actions");
    for (size_t i = 0; i < _actionCount; i++) {
      actionsArray.add(_actions[i]);
    }

    char payload[512];
    serializeJson(doc, payload);

    // Register Device
    Serial.println("Device registration: " + String(payload));
    return publish("register", payload, true);
  }

  void _subscribeToRelevantTopics() {
    for (int i = 0; i < _actionCount; i++) {
      subscribe("action/" + String(_actions[i]));
    }
    subscribe("control/#");
    subscribe("ping");
    subscribe("action/#");
  }

  void _acknowledgeConnection(String controllerID) {
    _controllerID = controllerID;
    _isInUse = true;
    _lastPingFromClient = millis();

    publish("control/acknowledge", "true");
  }

  const char* _getUniqueId() {
    static char out[16];  

    // Retrieve Serial Number
    uint32_t w0 = *(uint32_t*)0x0080A00C;
    uint32_t w1 = *(uint32_t*)0x0080A040;
    uint32_t w2 = *(uint32_t*)0x0080A044;
    uint32_t w3 = *(uint32_t*)0x0080A048;

    // Use last three bytes as probably unique identifier
    uint8_t b0 = (w0 >> 16) & 0xFF;
    uint8_t b1 = (w1 >> 8 ) & 0xFF;
    uint8_t b2 =  w2        & 0xFF;

    sprintf(out, "Robot-%02X%02X%02X", b0, b1, b2);
    return out;  // z.B. "Dev-5F0F8A"
  }

  bool _hasTimeoutOccoured() {
    if (!_isInUse) {
      return false;
    }
    unsigned long now = millis();
    if (now - _lastPingFromClient >= 5000) {
      Serial.println("timeout has occured: " + String(now) + "-" + String(_lastPingFromClient) + "=" + String(now - _lastPingFromClient));
      return true;
    } 
    return false;
  }

  void _sendHeartBeat() {
    if (_isInUse) {
      publish("status", "{\"online\":true, \"locked\":true}", true);
    } else {
      publish("status", "{\"online\":true, \"locked\":false}", true);
    }
  }

  // ── Member ───────────────────────────────────────────────────
  const char*  _ssid;
  const char*  _pass;
  IPAddress    _brokerIP;
  uint16_t     _brokerPort;
  const char*  _deviceType;
  const char*  _deviceID;
  
  String  _topicPrefix;

  String _controllerID;
  bool _isInUse;

  const char* _actions[MAX_ACTIONS]; size_t _actionCount;

  unsigned long _last; // Measure time for accurate timing
  unsigned long _lastPingFromClient;

  WiFiClient   _wifiClient;
  PubSubClient _mqtt;
};

#endif  // ROBOT_MQTT_H
