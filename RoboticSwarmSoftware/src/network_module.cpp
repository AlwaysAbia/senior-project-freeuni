#include <Wifi.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

#include "network_module.hpp"
#include "network_credentials.hpp"
#include "motor_module.hpp"
#include "globals.hpp"

WiFiClient espClient;
PubSubClient mqttClient(espClient);

String lastReceivedMessage = "";

//-----------------------------------------------
// Setup Functions
void setupOTA() {
  // Optional: set custom hostname and password
  ArduinoOTA.setHostname(hostname);
  ArduinoOTA.setPassword(otapassword); // Optional but recommended

  ArduinoOTA.onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
          type = "sketch";
      else // U_SPIFFS
          type = "filesystem";
      Serial.println("Start updating " + type);
  });

  ArduinoOTA.onEnd([]() {
      Serial.println("\nEnd");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress * 100) / total);
  });

  ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();
  Serial.println("OTA ready");
}

void connectToHub() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    // Allow OTA even before WiFi succeeds (if AP comes up later)
    ArduinoOTA.handle();
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

//-----------------------------------------------
// MQTT Helper & Core Functions
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  String topicStr = String(topic);
  
  // Check if topic is broadcast or matches my robot ID
  bool isBroadcast = topicStr.equals("command/broadcast");
  bool isMyCommand = topicStr.startsWith("command/individual/") && topicStr.endsWith(hostname);
  
  if (!isBroadcast && !isMyCommand) {
    return;
  }
  
  // Deduplication
  if (message.equals(lastReceivedMessage)) {
    Serial.println("Duplicate message, ignoring");
    return;
  }
  lastReceivedMessage = message;
  
  Serial.print("Received command: ");
  Serial.println(message);
  
  // Parse JSON (outside mutex)
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, message);
  
  if (error) {
    Serial.print("JSON parse failed: ");
    Serial.println(error.c_str());
    return;
  }
  
  // Extract values to local variables (outside mutex)
  bool hasMode = !doc["mode"].isNull();
  State::Mode newMode = State::OFF;
  if (hasMode) {
    const char* mode = doc["mode"];
    if (strcmp(mode, "OFF") == 0) newMode = State::OFF;
    else if (strcmp(mode, "IDLE") == 0) newMode = State::IDLE;
    else if (strcmp(mode, "LINE") == 0) newMode = State::LINE;
    else if (strcmp(mode, "POLYGON") == 0) newMode = State::POLYGON;
    else if (strcmp(mode, "MANUAL") == 0) newMode = State::MANUAL;
  }
  
  bool hasNeighborMaxDist = !doc["neighbor_maxDist"].isNull();
  uint16_t newNeighborMaxDist = hasNeighborMaxDist ? doc["neighbor_maxDist"].as<uint16_t>() : 0;
  
  bool hasIdleThresh = !doc["idle_thresh"].isNull();
  uint16_t newIdleThresh = hasIdleThresh ? doc["idle_thresh"].as<uint16_t>() : 0;
  
  bool hasLineNodeDist = !doc["line_nodeDist"].isNull();
  uint16_t newLineNodeDist = hasLineNodeDist ? doc["line_nodeDist"].as<uint16_t>() : 0;
  
  bool hasLineAlignTol = !doc["line_alignTol"].isNull();
  uint16_t newLineAlignTol = hasLineAlignTol ? doc["line_alignTol"].as<uint16_t>() : 0;
  
  bool hasPolygonSides = !doc["polygon_sides"].isNull();
  uint8_t newPolygonSides = hasPolygonSides ? doc["polygon_sides"].as<uint8_t>() : 0;
  
  bool hasPolygonRadius = !doc["polygon_radius"].isNull();
  uint16_t newPolygonRadius = hasPolygonRadius ? doc["polygon_radius"].as<uint16_t>() : 0;
  
  bool hasPolygonAlignTol = !doc["polygon_alignTol"].isNull();
  uint16_t newPolygonAlignTol = hasPolygonAlignTol ? doc["polygon_alignTol"].as<uint16_t>() : 0;
  
  // Manual move commands
  int l = doc["l"] | 0;
  int r = doc["r"] | 0;
  int b = doc["b"] | 0;
  bool hasManualMove = (l != 0 || r != 0 || b != 0);
  
  // NOW take mutex and update state quickly
  if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    
    if (hasMode) state.mode = newMode;
    if (hasNeighborMaxDist) state.neighbor_maxDist = newNeighborMaxDist;
    if (hasIdleThresh) state.idle_thresh = newIdleThresh;
    if (hasLineNodeDist) state.line_nodeDist = newLineNodeDist;
    if (hasLineAlignTol) state.line_alignTol = newLineAlignTol;
    if (hasPolygonSides) state.polygon_sides = newPolygonSides;
    if (hasPolygonRadius) state.polygon_radius = newPolygonRadius;
    if (hasPolygonAlignTol) state.polygon_alignTol = newPolygonAlignTol;
    
    xSemaphoreGive(stateMutex);
    
    Serial.println("State updated from MQTT");
  } else {
    Serial.println("Failed to acquire mutex for state update");
    return;
  }
  
  // Execute motor commands AFTER releasing mutex
  if (newMode == State::MANUAL && hasManualMove) {
    setMotorSteps(l, r, b);
  }
}

void mqttReconnect() {
  // Try to connect once, don't block
  if(!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    if(mqttClient.connect(hostname, mqtt_user, mqtt_password)) {
      Serial.println("connected");
      
      char commandTopic[100];
      char statusTopic[100];
      snprintf(commandTopic, sizeof(commandTopic), "command/individual/%s", hostname);
      snprintf(statusTopic, sizeof(statusTopic), "telemetry/%s/status", hostname);

      mqttClient.subscribe("command/broadcast");
      mqttClient.subscribe(commandTopic);

      const char* pubConMsg = "Connected to MQTT";
      mqttClient.publish(statusTopic, pubConMsg);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" will retry in next cycle");
    }
  }
}

void buildStatusPayload(char* buffer, size_t bufferSize) {
  // Local copies of state variables
  State::Mode mode;
  uint16_t neighbor_maxDist;
  uint16_t idle_thresh;
  uint16_t line_nodeDist, line_alignTol;
  uint8_t polygon_sides;
  uint16_t polygon_radius, polygon_alignTol;
  uint32_t distances[6];
  
  // Take mutex, copy data, release immediately
  if(xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE) {
    mode = state.mode;
    neighbor_maxDist = state.neighbor_maxDist;
    idle_thresh = state.idle_thresh;
    line_nodeDist = state.line_nodeDist;
    line_alignTol = state.line_alignTol;
    polygon_sides = state.polygon_sides;
    polygon_radius = state.polygon_radius;
    polygon_alignTol = state.polygon_alignTol;
    memcpy(distances, state.distances, sizeof(distances));
    
    xSemaphoreGive(stateMutex);
  }
  
  // Build JSON with ArduinoJson
  JsonDocument doc;
  
  // Mode as string
  const char* modeStr;
  switch(mode) {
    case State::OFF: modeStr = "OFF"; break;
    case State::IDLE: modeStr = "IDLE"; break;
    case State::LINE: modeStr = "LINE"; break;
    case State::POLYGON: modeStr = "POLYGON"; break;
    case State::MANUAL: modeStr = "MANUAL"; break;
    default: modeStr = "UNKNOWN"; break;
  }
  doc["mode"] = modeStr;
  
  // General parameters
  doc["neighbor_maxDist"] = neighbor_maxDist;
  
  // Mode-specific parameters
  switch(mode) {
    case State::IDLE:
      doc["idle_thresh"] = idle_thresh;
      break;
    case State::LINE:
      doc["line_nodeDist"] = line_nodeDist;
      doc["line_alignTol"] = line_alignTol;
      break;
    case State::POLYGON:
      doc["polygon_sides"] = polygon_sides;
      doc["polygon_radius"] = polygon_radius;
      doc["polygon_alignTol"] = polygon_alignTol;
      break;
    default:
      break;
  }
  
  // Distance array
  JsonArray distArray = doc["distances"].to<JsonArray>();
  for (int i = 0; i < 6; i++) {
    distArray.add(distances[i]);
  }
  
  // Serialize to buffer
  serializeJson(doc, buffer, bufferSize);
}

//-----------------------------------------------
// Server Setup, FreeRTOS Task
void setupServer() {
  mqttClient.setServer(mqtt_broker, mqtt_port);
  mqttClient.setCallback(mqttCallback);

  mqttReconnect();
}

// FreeRTOS Task
void networkTask(void* parameter) {
  const TickType_t xFrequency = pdMS_TO_TICKS(10); // Check MQTT every 10ms

  TickType_t lastStatusPublish = 0;
  const TickType_t STATUS_PUBLISH_INTERVAL = pdMS_TO_TICKS(1000); // Publish status every 1 second

  while (true) {
      ArduinoOTA.handle();
      if(!mqttClient.connected()) {
        mqttReconnect();
      }
      mqttClient.loop();

      // Only publish status periodically
      TickType_t now = xTaskGetTickCount();
      if (now - lastStatusPublish >= STATUS_PUBLISH_INTERVAL) {
          char statusTopici[100];
          snprintf(statusTopici, sizeof(statusTopici), "telemetry/%s/status", (char*)hostname);
          
          char statusData[512];
          buildStatusPayload(statusData, sizeof(statusData));
          
          mqttClient.publish(statusTopici, statusData);
          lastStatusPublish = now;
      }

      vTaskDelay(xFrequency);
  }
}