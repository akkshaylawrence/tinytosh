#include "WebServerService.h"

#include <ArduinoJson.h>
#include <ESPmDNS.h>

#include "JsonSerializer.h"
#include "PcMonitorService.h"
#include "WebPanel.h"

WebServerService::WebServerService(int port, ConfigSaveCallback callback) : 
  server(port), saveCallback(callback) {}

void WebServerService::setAppState(AppState* appState) {
  state = appState;
}

void WebServerService::begin() {
  server.on("/", HTTP_GET, [this](){ this->handleRoot(); }); 
  server.on("/save", HTTP_POST, [this](){ this->handleSave(); });
  server.on("/update", HTTP_GET, [this](){ this->handleUpdate(); }); 
  server.on("/pc-stats", HTTP_POST, [this](){ this->handlePcStats(); });
  
  server.begin();
  Serial.println("WebServerService: HTTP Server started."); 
  
  String uniqueName = state->config.device_id;

  if (MDNS.begin(uniqueName.c_str())) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("WebServerService: mDNS Responder Started: http://%s.local\n", uniqueName.c_str());
  }
}

void WebServerService::handleClient() {
    server.handleClient();
}

void WebServerService::handleRoot() {
  server.sendHeader("Content-Encoding", "gzip");
  server.send_P(HTTP_OK, "text/html", (const char*)WEB_PANEL_GZ, WEB_PANEL_GZ_LEN);
}

void WebServerService::handleSave() {
  if (!server.hasArg("plain")) {
    server.send(HTTP_BAD_REQUEST, "text/plain", "Body not received");
    return;
  }
  
  String body = server.arg("plain");
  
  if (JsonSerializer::parseConfig(body.c_str(), *state)) {
    if (saveCallback) saveCallback();
    server.send(HTTP_OK, "text/plain", "Saved");
  } else {
    server.send(HTTP_BAD_REQUEST, "text/plain", "Invalid JSON");
  }
}

void WebServerService::handleUpdate() {
  String jsonResponse = JsonSerializer::buildAppStateJson(*state);
  server.send(HTTP_OK, "application/json", jsonResponse);
}

void WebServerService::handlePcStats() {
  if (!server.hasArg("plain")) {
    server.send(HTTP_BAD_REQUEST, "application/json", "{\"status\":\"error\", \"message\":\"Body not received\"}");
    return;
  }
  
  String body = server.arg("plain");
  
  DynamicJsonDocument doc(1536);
  DeserializationError error = deserializeJson(doc, body);
  
  if (error) {
    server.send(HTTP_BAD_REQUEST, "application/json", "{\"status\":\"error\", \"message\":\"Invalid JSON\"}");
    return;
  }

  String incoming_pc_id = doc["pc_id"] | "";
  if (incoming_pc_id == "") {
    server.send(HTTP_BAD_REQUEST, "application/json", "{\"status\":\"error\", \"message\":\"Missing PC ID\"}");
    return;
  }

  if (millis() - state->pc.last_update > PC_DATA_TIMEOUT_MS || state->config.active_pc_id == incoming_pc_id || state->config.active_pc_id == "") {
    
    String reply = "{\"status\":\"ok\"";
    if (PcMonitorService::applyStats(doc, *state, true)) reply += ",\"presence\":" + JsonSerializer::buildPresenceJson(state->presence);
    server.send(HTTP_OK, "application/json", reply + "}");
  } else {
    server.send(HTTP_FORBIDDEN, "application/json", "{\"status\":\"error\", \"message\":\"Device is already paired to another PC\"}");
  }
}