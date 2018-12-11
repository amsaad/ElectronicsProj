#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

//needed for library
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include <IRremoteESP8266.h>
#include <IRsend.h>

//pin 4
#define IR_SEND_PIN D2

IRsend irsend(IR_SEND_PIN);

ESP8266WebServer server(80);


void handleCommands() {
  String Message = "::Command captured\n";
  if (server.hasArg("Code") == true) {
    String cmd = server.arg("Code");
    uint32_t code = strtoul(cmd.c_str(), NULL, 16);
    /**/

    Message += "::The recieved commend is" + cmd;
    irsend.sendNEC(code, 32);
    delay(40);
    Message += "\n::Command sent over IR";
  }
  Serial.println(Message);
  server.send(200, "text/plain", Message);

}
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  //reset saved settings
  //wifiManager.resetSettings();

  wifiManager.autoConnect();
  while (WiFi.status() != WL_CONNECTED) {   //Wait for connection
    delay(500);
    Serial.println("Waiting to connect...");
  }

  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRootPath);
  server.on("/cmd", handleCommands);
  server.begin();

  if (!MDNS.begin("kroom")) {             // Start the mDNS responder for esp8266.local
    Serial.println("Error setting up MDNS responder!");
  }
  //Serial.println("mDNS responder started");
  //if you get here you have connected to the WiFi
  //Serial.println("connected...yeey :)");

  irsend.begin();
}

void handleRootPath() {
  return server.send(200, "text/plain", "Connection succeed");
}


void loop() {
  // put your main code here, to run repeatedly:
  server.handleClient();
}
