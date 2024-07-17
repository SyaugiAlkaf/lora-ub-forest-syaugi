#include <SPI.h>
#include <WiFi.h>
#include <LoRa.h>
#include <PubSubClient.h>
#include "mqttsnHeader.h"

const int cs = 5;
const int reset = 14;
const int irq = 2; // Adjust the IRQ pin based on your hardware configuration

// MQTT Broker settings
const char* mqtt_server = "192.168.43.226";
const int mqtt_port = 1883; // MQTT port (default is 1883)
const char* mqtt_topic = "mqtt-topic"; // MQTT topic to send messages to
const char* mqtt_client_id = "ESP32Client"; // MQTT client ID

const char* ssid = "darurat"; // Change to your Wi-Fi SSID
const char* password = "12345678"; // Change to your Wi-Fi password

unsigned long gatewayMillisForNode1 = 0;
unsigned long gatewayMillisForNode2 = 0;

unsigned long lastReceivedTimestampNode1 = 0;
unsigned long lastReceivedTimestampNode2 = 0;

const unsigned long TIMEOUT = 1000; // 1 seconds

WiFiClient espClient;
PubSubClient client(espClient);

void printMQTTError(int state) {
  switch (state) {
    case MQTT_CONNECTION_TIMEOUT:
      Serial.println("Connection timeout");
      break;
    case MQTT_CONNECTION_LOST:
      Serial.println("Connection lost");
      break;
    case MQTT_CONNECT_FAILED:
      Serial.println("Connect failed");
      break;
    case MQTT_DISCONNECTED:
      Serial.println("Disconnected");
      break;
    case MQTT_CONNECTED:
      Serial.println("Connected");
      break;
    case MQTT_CONNECT_BAD_PROTOCOL:
      Serial.println("Bad protocol");
      break;
    case MQTT_CONNECT_BAD_CLIENT_ID:
      Serial.println("Bad client ID");
      break;
    case MQTT_CONNECT_UNAVAILABLE:
      Serial.println("Connect unavailable");
      break;
    case MQTT_CONNECT_BAD_CREDENTIALS:
      Serial.println("Bad credentials");
      break;
    case MQTT_CONNECT_UNAUTHORIZED:
      Serial.println("Connect unauthorized");
      break;
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial);

  LoRa.setSPIFrequency(4E6);
  LoRa.setPins(cs, reset, irq);
  // LoRa.setSignalBandwidth(125E3);  // Set bandwidth to 125kHz
  // LoRa.setSpreadingFactor(12);     // Set spreading factor to 12
  // LoRa.setTxPower(20);             // Set transmit power to 20dBm
  delay(1000);

  while (!LoRa.begin(915E6)) {
    Serial.println("LoRa initialization failed.");
    delay(500);
  }
  Serial.println("LoRa Initialized!");

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi...");
  }

  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // configTime();  // Configure NTP

  client.setServer(mqtt_server, mqtt_port);
}

void loop() {
  unsigned long currentMillis = millis();
  // Check if Node1 has timed out
  if (currentMillis - lastReceivedTimestampNode1 > TIMEOUT) {
    gatewayMillisForNode1 = 0; // Reset if timeout
  }
    
  // Check if Node2 has timed out
  if (currentMillis - lastReceivedTimestampNode2 > TIMEOUT) {
    gatewayMillisForNode2 = 0; // Reset if timeout
  }

  // Receive MQTT-SN message
  int packetSize = LoRa.parsePacket();
  if (packetSize > 0) {
    uint8_t mqttSnMessage[125]; // Adjust the buffer size as needed

    // Read the packet into the buffer
    LoRa.readBytes(mqttSnMessage, packetSize);

    // Print the entire message for inspection
    Serial.print("Full message received: ");
    for (int i = 0; i < packetSize; i++) {
        Serial.print((char)mqttSnMessage[i]);
    }
    Serial.println();

    Serial.println(packetSize);

    // Process the MQTT-SN message and send it as an MQTT message
    handleMQTTSNMessage(mqttSnMessage, packetSize);

    
  }
  // Maintain the MQTT connection
  if (!client.connected()) {
    reconnect();
  }

  client.loop();
}

void handleMQTTSNMessage(const uint8_t* message, int length) {
  // Process the MQTT-SN message received from the end-node
  // You can extract and print the message data here
  // Example: printing the payload data 
  String topic = extractTopic(message, length); 
  String payload = extractPayload(message, length);

  Serial.print("Received MQTT-SN message on topic: ");
  Serial.print(topic);
  Serial.print(" with payload: ");
  Serial.println(payload);

  // Extract the sensor data from the payload
  String sensorData = extractSensorData(payload);

  // Print the MQTT-SN payload data
  Serial.print("Received MQTT-SN message: ");
  Serial.println(sensorData);

  unsigned long currentMillis = millis();
  String gatewayTimestamp = String(currentMillis);
  String nodeMillis = extractMillisFromPayload(payload);
  int payloadID = extractPayloadID(payload);  // This function is added below.
  String modifiedPayload;  // Declare the variable at the beginning of the function
  int rssiValue = LoRa.packetRssi(); // Get RSSI value

  if (topic == "1") {
        if (payloadID == 11) {
            gatewayMillisForNode1 = 0;
        } else {
            gatewayMillisForNode1++; // Increment on each received message
        }
        lastReceivedTimestampNode1 = currentMillis; // Update the last received timestamp
        modifiedPayload = "N1_GW_TSId: " + String(gatewayMillisForNode1) + " GW_TS:" + String(currentMillis) + " GW_RS:" + String(rssiValue) + " " + payload;
    } 
    else if (topic == "2") {
        if (payloadID == 11) {
            gatewayMillisForNode2 = 0;
        } else {
            gatewayMillisForNode2++; // Increment on each received message
        }
        lastReceivedTimestampNode2 = currentMillis; // Update the last received timestamp
        modifiedPayload = "N2_GW_TSId: " + String(gatewayMillisForNode2) + " GW_TS: " + String(currentMillis) + " GW_RS:" + String(rssiValue) + " " + payload;
    } 
    else {
        modifiedPayload = "GW_TS: " + String(currentMillis) + " " + payload; // default
    }

  if (topic == "1" || topic == "2") {
    if (client.publish(mqtt_topic, modifiedPayload.c_str())) {
      Serial.println("MQTT message sent to Node-RED");
    } else {
      Serial.println("Failed to send MQTT message");
      printMQTTError(client.state());
    }
  }
}

int extractPayloadID(const String &payload) {
  // Assuming the format has "ID:XX" where XX is the payload ID
  int idStartIndex = payload.indexOf("ID:");
  if (idStartIndex != -1) {
    int spaceIndex = payload.indexOf(' ', idStartIndex);
    if (spaceIndex == -1) {
      spaceIndex = payload.length();
    }
    return payload.substring(idStartIndex + 3, spaceIndex).toInt();
  }
  return -1;  // Return -1 if ID not found
}

String extractMillisFromPayload(const String &payload) {
  // Assuming the format is " NodeX_Timestamp:millis_value ..."
  int timestampStart = payload.indexOf("TS:");
  if (timestampStart != -1) {
    int spaceIndex = payload.indexOf(' ', timestampStart); // find space after "NodeX_Timestamp:"
    if (spaceIndex == -1) {
      spaceIndex = payload.length();
    }
    return payload.substring(timestampStart + 11, spaceIndex);
  }
  return ""; // Return empty string if millis not found
}

String extractTopic(const uint8_t* message, int length) {
    String strMessage = "";
    int startIdx = -1;
    for (int i = 0; i < length; i++) {
        if (isPrintableChar(message[i]) && message[i] != ' ') {
            if (startIdx == -1) startIdx = i; // Start of topic
            strMessage += (char)message[i];
        } else {
            if (startIdx != -1) break; // End of topic
        }
    }
    return strMessage;
}

String extractPayload(const uint8_t* message, int length) {
    String strMessage = "";
    bool recording = false;
    for (int i = 0; i < length; i++) {
        if (recording) {
            if (isPrintableChar(message[i])) {
                strMessage += (char)message[i];
            } else {
                // End of the meaningful payload, break out of the loop
                break;
            }
        } else {
            // We assume the space character ' ' signifies the end of the topic and the start of the payload
            if (message[i] == ' ') recording = true;
        }
    }
    return strMessage;
}

bool isPrintableChar(uint8_t c) {
  return (c >= 32 && c <= 126); // ASCII printable characters range
}

String extractSensorData(const String &payload) {
  // Implement your extraction logic based on the payload structure
  // For example, you can use substring or regular expressions to extract data.
  // Here, we assume the format is "SoilMoisture:X.X LightIntensity:X.X"
  int soilMoistureStart = payload.indexOf("SM:");
  int lightIntensityStart = payload.indexOf("LI:");
  
  if (soilMoistureStart != -1 && lightIntensityStart != -1) {
    String soilMoisture = payload.substring(soilMoistureStart + 13, soilMoistureStart + 17);
    String lightIntensity = payload.substring(lightIntensityStart + 15, lightIntensityStart + 19);
    return "SoilMoisture: " + soilMoisture + " LightIntensity: " + lightIntensity;
  }

  // If the payload doesn't match the expected format, return an empty string
  return "";
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    if (client.connect(mqtt_client_id)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}