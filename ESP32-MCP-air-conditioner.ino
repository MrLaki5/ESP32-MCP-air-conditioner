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

void sendAC(bool on, int temp = 24) {
  if (on) {
    ac.on();
    ac.setMode(kCoolixCool);    // Use Coolix enum directly
    ac.setTemp(temp);           // Set temperature
    ac.setFan(kCoolixFanAuto);  // Coolix auto‐fan
  }
  else {
    ac.off();
  }
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
    tool["description"] = "Toggle the air conditioner state and set the temperature";
    JsonObject inputSchema = tool.createNestedObject("inputSchema");
    inputSchema["type"] = "object";
    // State property
    inputSchema["properties"]["state"]["type"] = "string";
    JsonArray enumArr = inputSchema["properties"]["state"].createNestedArray("enum");
    enumArr.add("on");
    enumArr.add("off");
    JsonArray required = inputSchema.createNestedArray("required");
    required.add("state");
    inputSchema["properties"]["state"]["description"] = "State of the air conditioner, either 'on' or 'off'";
    // Temperature property
    inputSchema["properties"]["temperature"]["type"] = "number";
    inputSchema["properties"]["temperature"]["minimum"] = 17;
    inputSchema["properties"]["temperature"]["maximum"] = 30;
    inputSchema["properties"]["temperature"]["default"] = 24;
    inputSchema["properties"]["temperature"]["description"] = "Temperature in degrees Celsius, use the default value if not specified";

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
      int temperature = req["params"]["arguments"]["temperature"].as<int>();
      bool state_b = state == "on";
      sendAC(state_b, temperature);
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
