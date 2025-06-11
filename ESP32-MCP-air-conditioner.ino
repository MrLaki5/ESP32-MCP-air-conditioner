#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_Coolix.h>

const char* ssid = "";
const char* password = "";

WebServer server(80);
const uint16_t kIrLed = 15;

IRsend irsend(kIrLed);
IRCoolixAC ac(kIrLed);

void sendAC(bool on) {
  if (on) ac.on();
  else    ac.off();

  ac.setMode(kCoolixCool);    // use Coolix enum directly
  ac.setTemp(24);                 // 24 °C
  ac.setFan(kCoolixFanAuto);      // Coolix auto‐fan

  ac.send(); 
  Serial.println(F("Send AC (Coolix) triggered!"));
}

void handleRoot() {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Only POST allowed");
    return;
  }

  StaticJsonDocument<256> req;
  DeserializationError err = deserializeJson(req, server.arg("plain"));
  if (err) {
    server.send(400, "application/json", "{\"error\":\"invalid_json\"}");
    return;
  }

  // Extract the 'id' from the request
  long id = req["id"].as<long>();
  String rpcMethod = req["method"].as<String>();

  Serial.print("Root ID: ");
  Serial.println(id);
  Serial.print("Method: ");
  Serial.println(rpcMethod);

  // Build the response including the same 'id'
  StaticJsonDocument<512> res;
  res["id"] = id;
  JsonArray tools = res.createNestedArray("tools");
  JsonObject tool = tools.createNestedObject();
  tool["jsonrpc"] = "2.0";

  // Initialize MCP method
  if (rpcMethod == "initialize") {
    res["result"]["protocolVersion"] = "2025-03-26";
    res["result"]["capabilities"]["tools"]["listChanged"] = false;
    res["result"]["serverInfo"]["name"] = "ESP32 Air Condition MCP Server";
    res["result"]["serverInfo"]["version"] = "0.0.1";
  }
  // Tools list MCP method
  else if (rpcMethod == "tools/list") {
    JsonArray tools = res["result"].createNestedArray("tools");
    JsonObject tool = tools.createNestedObject();
    tool["name"] = "toggle_aircondition";
    tool["description"] = "Turns the air conditioner on or off";
    JsonObject inputSchema = tool.createNestedObject("inputSchema");
    inputSchema["type"] = "object";
    inputSchema["properties"]["state"]["type"] = "string";
    JsonArray enumArr = inputSchema["properties"]["state"].createNestedArray("enum");
    enumArr.add("on");
    enumArr.add("off");
    JsonArray required = inputSchema.createNestedArray("required");
    required.add("state");
    inputSchema["additionalProperties"] = false;
    inputSchema["$schema"] = "http://json-schema.org/draft-07/schema#";
    JsonObject annotations = tool.createNestedObject("annotations");
    annotations["title"] = "Toggle Aircondition";
  }
  // Tools call MCP method
  else if (rpcMethod == "tools/call") {
    String toolName = req["params"]["name"].as<String>();
    Serial.print("Tool name: ");
    Serial.println(toolName);
    if (toolName == "toggle_aircondition") {
      String state = req["params"]["arguments"]["state"].as<String>();
      bool state_b = state == "on";
      sendAC(state_b);
      JsonArray content = res["result"].createNestedArray("content");
      JsonObject content_1 = content.createNestedObject();
      content_1["type"] = "text";
      content_1["text"] = "Action finished successfully!";
    }
  }

  String output;
  serializeJson(res, output);
  server.send(200, "application/json", output);
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  Serial.println(WiFi.localIP());

  irsend.begin();
  ac.begin(); 

  server.on("/", handleRoot);
  server.begin();
}

void loop() {
  server.handleClient();
}
