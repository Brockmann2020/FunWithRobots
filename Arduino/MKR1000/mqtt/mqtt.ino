#include "EasyMQTT.h"

const char* SSID   = "Tilo's Galaxy A40";
const char* PASS   = "brockmann";
IPAddress   BROKER (192,168,166,222);

#define LED_PIN  1    // D1 on MKR1000

EasyMQTT bot(SSID, PASS, BROKER, 1883, "MKRobot");

bool shutdownFlag = false;

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Copy topic
  String t(topic);
  
  // Convert payload to String
  String msg;
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

  // Build and publish log message
  String logMsg = String("Received on [") + t + "] → " + msg;
  bot.client().publish("MKRobot/log", logMsg.c_str(), true);
  Serial.println(logMsg);

  // 3) Process commands
  if (t == "MKRobot/cmd/led") {
    blinkLED(LED_PIN, msg.toInt());
    bot.client().publish("ESPBot/cmd/led", "5", true);
  }
  else if (t == "MKRobot/cmd/shutdown") {
    shutdownFlag = true;
  }
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  bot.begin();

  // register callback and subscribe to all sub-topics
  bot.client().setCallback(mqttCallback);
  bot.client().subscribe("MKRobot/cmd/#");

  Serial.println("Ready for commands!");
}

void loop() {
  bot.mqtt_loop();

  if (shutdownFlag) {
    Serial.println("Shutting down...");
    while (true) { /* halt */ }
  }
}

// --- Helper: blink the LED on D1 ---
void blinkLED(int pin, int times) {
  String info = String("Blinking LED ")
              + String(times)
              + " times";
  bot.client().publish("MKRobot/log", info.c_str(), true);

  for (int i = 0; i < times; i++) {
    digitalWrite(pin, HIGH);
    delay(500);
    digitalWrite(pin, LOW);
    delay(500);
  }

  bot.client().publish("MKRobot/log", "Finished blinking", true);
}
