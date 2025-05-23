#include "EasyMQTT_ESP8266.h"

const char* SSID   = "Tilo's Galaxy A40";
const char* PASS   = "brockmann";
IPAddress   BROKER (192,168,166,222);

#define LED_PIN D1

EasyMQTT bot(SSID, PASS, BROKER, 1883, "ESPBot");

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String t(topic);                 // sichere Kopie des Topics
  
  // Convert payload to String
  String msg;
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

  // Build and publish log message
  String logMsg = String("Received on [") + t + "] → " + msg;
  bot.client().publish("ESPBot/log", logMsg.c_str(), true);
  Serial.println(logMsg);

  if (t == "ESPBot/cmd/led") {
    blinkLED(LED_PIN, msg.toInt());
    bot.client().publish("MKRobot/cmd/led", msg.toInt(), true);
  }
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  bot.begin();
  bot.client().setCallback(mqttCallback);
  bot.client().subscribe("ESPBot/cmd/#");
  Serial.println("Ready for commands on ESP8266!");
}

void loop() {
  bot.mqtt_loop();
}

void blinkLED(int pin, int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(pin, HIGH);
    delay(500);
    digitalWrite(pin, LOW);
    delay(500);
  }
}
