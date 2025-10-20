#include <Wifi.h>
#include <WebServer.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>

#include "network_module.hpp"
#include "network_credentials.hpp"
#include "motor_module.hpp"
#include "globals.hpp"

WebServer server(80);

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
// HTTP Request Handler Functions
void handleUpdateState() {
  bool updated = false;
  String msg = "Updated: ";

  // --- Mode selection ---
  if (server.hasArg("mode")) {
    String modeStr = server.arg("mode");
    if      (modeStr == "OFF")     state.mode = State::OFF;
    else if (modeStr == "IDLE")    state.mode = State::IDLE;
    else if (modeStr == "LINE")    state.mode = State::LINE;
    else if (modeStr == "POLYGON") state.mode = State::POLYGON;
    else if (modeStr == "MANUAL")  state.mode = State::MANUAL;
    else {
      server.send(400, "text/plain", "Invalid mode");
      return;
    }
    updated = true;
    msg += "mode ";
  }

  // --- Common / General Config ---
  if (server.hasArg("neighbor_maxDist")) {
    state.neighbor_maxDist = server.arg("neighbor_maxDist").toInt();
    updated = true;
    msg += "neighbor_maxDist ";
  }

  // --- Idle mode ---
  if (server.hasArg("idle_thresh")) {
    state.idle_thresh = server.arg("idle_thresh").toInt();
    updated = true;
    msg += "idle_thresh ";
  }

  // --- Line mode ---
  if (server.hasArg("line_nodeDist")) {
    state.line_nodeDist = server.arg("line_nodeDist").toInt();
    updated = true;
    msg += "line_nodeDist ";
  }

  if (server.hasArg("line_alignTol")) {
    state.line_alignTol = server.arg("line_alignTol").toInt();
    updated = true;
    msg += "line_alignTol ";
  }

  // --- Polygon mode ---
  if (server.hasArg("polygon_sides")) {
    state.polygon_sides = server.arg("polygon_sides").toInt();
    updated = true;
    msg += "polygon_sides ";
  }

  if (server.hasArg("polygon_radius")) {
    state.polygon_radius = server.arg("polygon_radius").toInt();
    updated = true;
    msg += "polygon_radius ";
  }

  if (server.hasArg("polygon_alignTol")) {
    state.polygon_alignTol = server.arg("polygon_alignTol").toInt();
    updated = true;
    msg += "polygon_alignTol ";
  }

  if (updated)
    server.send(200, "text/plain", msg + "successfully");
  else
    server.send(400, "text/plain", "No valid parameters provided");
}

void handleMove() {
  int l = server.hasArg("l") ? server.arg("l").toInt() : 0;
  int r = server.hasArg("r") ? server.arg("r").toInt() : 0;
  int b = server.hasArg("b") ? server.arg("b").toInt() : 0;

  if (l == 0 && r == 0 && b == 0) {
    server.send(400, "text/plain", "At least one of l, r, or b must be non-zero");
    return;
  }
  if(state.mode == State::MANUAL){
    setMotorSteps(l, r, b);
    server.send(200, "text/plain",
      "Motor steps set: L=" + String(l) + ", R=" + String(r) + ", B=" + String(b));
  } else {
    server.send(200, "text/plain",
      "Robot Not In Manual Mode");
  }
}

void handleStatus() {
  JsonDocument doc;

  doc["mode"]           = state.mode;
  doc["idle_thresh"]    = state.idle_thresh;
  doc["line_nodeDist"]  = state.line_nodeDist;
  doc["line_alignTol"]  = state.line_alignTol;
  doc["polygon_radius"] = state.polygon_radius;
  doc["polygon_sides"]  = state.polygon_sides;
  doc["polygon_alignTol"]= state.polygon_alignTol;
  doc["neighbor_maxDist"]= state.neighbor_maxDist;

  JsonArray dist = doc["d"].to<JsonArray>();
  for (int i = 0; i < 6; ++i) {
    dist.add(state.distances[i]);
  }

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

//-----------------------------------------------
// Server Setup, Main Function Call, FreeRTOS Task
void setupServer() {
  // curl "http://esp32_s3.local/forward?steps=500"
  server.on("/updateState", handleUpdateState);
  server.on("/move",        handleMove);
  server.on("/status", HTTP_GET, handleStatus);

  server.begin();
}

void handleServer() {
  server.handleClient();
}

// FreeRTOS Task
void networkTask(void* parameter) {
  const TickType_t xFrequency = pdMS_TO_TICKS(10); // Check every 10ms
  
  while (true) {
    ArduinoOTA.handle();
    handleServer();
    
    vTaskDelay(xFrequency);
  }
}