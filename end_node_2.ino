#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <BH1750.h>
#include "mqttsnHeader.h"


const int cs = 5;
const int reset = 14;
const int irq = 35;

const int soilMoisturePin = 4;
const int ledPin = 32;
BH1750 lightMeter;

int loopCounter = 0;
int messageCounter = 1;

void setup() {
  Serial.begin(115200);
  while (!Serial);
  
  LoRa.setSPIFrequency(4E6);
  LoRa.setPins(cs, reset, irq);
  LoRa.setSignalBandwidth(125E3);  // Set bandwidth to 125kHz
  LoRa.setSpreadingFactor(12);     // Set spreading factor to 12
  LoRa.setTxPower(20);             // Set transmit power to 20dBm
  pinMode(soilMoisturePin, INPUT);
  pinMode(ledPin, OUTPUT);
  delay(1000);

  Wire.begin();
  lightMeter.begin();

  while (!LoRa.begin(915E6)) {
    Serial.println("LoRa initialization failed.");
    delay(500);
  }
  Serial.println("LoRa Initialized!");
}

void loop() {
  if (loopCounter < 11) {
    digitalWrite(ledPin, HIGH);

    float soilMoisture = readSoilMoisture();
    float lightIntensity = readLightIntensity();

    String message = " N2_ID:" + String(messageCounter) + " LI:" + String(lightIntensity, 1) + " SM:" + String(soilMoisture, 1) + " SZ:" + String(message.length());

    Serial.print("Sending message: ");
    Serial.println(message);
    Serial.println("Data size: " + String(message.length()) + " bytes");
    String messageLength = String(message.length());

    String messageB = " N2_ID:" + String(messageCounter) + " LI:" + String(lightIntensity, 1) + " SM:" + String(soilMoisture, 1) + " SZ:" + messageLength;

    const char* messageData = messageB.c_str();

    publishUsingMQTTSN("2", messageData);

    delay(5000);

    loopCounter++;
    messageCounter++;
    if (messageCounter > 11) {
        messageCounter = 1;
    }

  } else {
    Serial.println("Entering Deepsleep for 60s");
    esp_deep_sleep(60e6);
    messageCounter = 1;
  }
}

void publishUsingMQTTSN(const char* topic, const char* data) {
  static uint16_t messageID = 1;

  uint8_t mqttSnMessage[256];
  mqttSnMessage[0] = PUBLISH;
  mqttSnMessage[1] = 0x00;
  mqttSnMessage[2] = 0x00;
  mqttSnMessage[3] = 0x00;

  strncpy((char*)&mqttSnMessage[4], topic, 100);

  uint8_t messageLength = 4 + strlen(topic);

  strncpy((char*)&mqttSnMessage[messageLength], data, 100);

  messageLength += strlen(data);

  LoRa.beginPacket();
  LoRa.write(mqttSnMessage, messageLength);
  LoRa.endPacket();

  messageID++;

  Serial.print("Sent MQTT-SN message: ");
  Serial.println(data);

  digitalWrite(ledPin, LOW);
}

float readSoilMoisture() {
  int rawValue = analogRead(soilMoisturePin);
  float moisturePercentage = map(rawValue, 0, 4095, 100, 0);
  moisturePercentage = constrain(moisturePercentage, 0, 100);
  return moisturePercentage;
}

float readLightIntensity() {
  float lux = lightMeter.readLightLevel();
  Serial.print("Light: ");
  Serial.print(lux);
  Serial.println(" lx");
  return lux;
}
