#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         // https://github.com/tzapu/WiFiManager
//#include <TimeLib.h>
#include <WiFiUdp.h>
#include <TimeAlarms.h>

#include "DHTesp.h"

#define DHTPIN            14


DHTesp dht;
float lastTemp = 0;


// Set web server port number to 80
WiFiServer server(80);

// Variable to store the HTTP request
String header;


static const char ntpServerName[] = "asia.pool.ntp.org";
const int timeZone = 3;     // Central European Time

WiFiUDP Udp;
unsigned int localPort = 8888;
time_t getNtpTime();
void digitalClockDisplay();
void printDigits(int digits);
String prepDigits(int digits, bool dot);
void sendNTPpacket(IPAddress &address);

const char* mDNS_name = "kroom";
String LatestTime = "";
String offHH;
String offMM;
String offSS;
String alarmType;
// Auxiliar variables to store the current output state

String output4State = "off";

// Assign output variables to GPIO pins

const int output4 = 4;

void setup() {
  Serial.begin(115200);

  // Initialize the output variables as outputs

  pinMode(output4, OUTPUT);
  // Set outputs to LOW

  digitalWrite(output4, LOW);

  // WiFiManager
  // Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  // Uncomment and run it once, if you want to erase all the stored information
  //wifiManager.resetSettings();

  // set custom ip for portal
  //wifiManager.setAPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

  // fetches ssid and pass from eeprom and tries to connect
  // if it does not connect it starts an access point with the specified name
  // here  "AutoConnectAP"
  // and goes into a blocking loop awaiting configuration
  wifiManager.autoConnect("AutoConnectAP");
  // or use this for auto generated name ESP + ChipID
  //wifiManager.autoConnect();

  // if you get here you have connected to the WiFi
  Serial.println("Connected.");

  if (MDNS.begin(mDNS_name, WiFi.localIP())) { //Start mDNS
    Serial.println("MDNS started");
  }

  server.begin();

  Serial.println("Starting UDP");
  Udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(Udp.localPort());
  Serial.println("waiting for sync");
  setSyncProvider(getNtpTime);
  while (String(year()) < "2000")
  {
    getNtpTime();
  }
  setSyncInterval(18000);
  //  adjustTime(-438);

  dht.setup(14);
  delay(dht.getMinimumSamplingPeriod());
  lastTemp = dht.getTemperature();
  Serial.println("Current Temp:");
  Serial.print(lastTemp);
  Serial.println(" ");
}

time_t prevDisplay = 0;

void loop() {

  Alarm.delay(1000);

  delay(dht.getMinimumSamplingPeriod());
  float currentTemp = dht.getTemperature();
  if (dht.getStatusString() == "OK" ) {
    if ((lastTemp < currentTemp - 2) || (lastTemp > currentTemp + 2)) {
      Serial.println(currentTemp);
      getTempInfo();
      lastTemp = dht.getTemperature();
    }
  }

  WiFiClient client = server.available();   // Listen for incoming clients

  if (client) {                             // If a new client connects,
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();


            // turns the GPIOs on and off
            if (header.indexOf("GET /4/on") >= 0) {
              Serial.println("GPIO 4 on");
              output4State = "on";
              digitalWrite(output4, HIGH);
            } else if (header.indexOf("GET /4/off") >= 0) {
              Serial.println("GPIO 4 off");
              output4State = "off";
              digitalWrite(output4, LOW);
            }
            else if (header.indexOf("GET /setoff") >= 0) {
              getParamsVal(header);
            }
            digitalClockDisplay();
            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style the on/off buttons
            // Feel free to change the background-color and font-size attributes to fit your preferences
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #195B6A; border: none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println(".button2 {background-color: #77878A;}</style></head>");

            // Web Page Heading
            client.println("<body><h1>Zizo Room</h1>");

            // Display current state, and ON/OFF buttons for GPIO 4
            client.println("<p>GPIO 4 - State " + output4State + "</p>");
            // If the output4State is off, it displays the ON button
            if (output4State == "off") {
              client.println("<p><a href=\"/4/on\"><button class=\"button\">ON</button></a></p>");
            } else {
              client.println("<p><a href=\"/4/off\"><button class=\"button button2\">OFF</button></a></p>");
            }
            client.println("<p><h3>Indoor Temp</h3><label>");
            client.println(String(lastTemp));
            client.println("</label></p>");
            client.println("<label>Off time</label>");
            client.println("<input type=\"time\" id=\"offTime\" name=\"offTime\" min=\"9:00\" max=\"18:00\"></input>");
            client.println("</body></html>");

            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }

}
const char * days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"} ;
const char * months[] = {"Jan", "Feb", "Mar", "Apr", "May", "June", "July", "Aug", "Sep", "Oct", "Nov", "Dec"} ;

void OnceOnly() {
  Serial.println("OnceOnly timer");
  int stat = digitalRead(output4);
  if (stat == 0)
    digitalWrite(output4, HIGH);
  else
    digitalWrite(output4, LOW);
}
void Repeats() {
  Serial.println("Repeats timer");
  int stat = digitalRead(output4);
  if (stat == 0)
    digitalWrite(output4, HIGH);
  else
    digitalWrite(output4, LOW);
}
void DailyAlarm() {
  digitalWrite(output4, LOW);
}
void WeeklyAlarm() {
  digitalWrite(output4, LOW);
}
void digitalClockDisplay()
{
  Serial.println("################################");
  Serial.println("## Current Date and time: ");
  // digital clock display of the time
  //  Serial.print(hourFormat12());
  //  printDigits(minute());
  //  printDigits(second());
  //  Serial.print(".");
  //  Serial.print(month());
  //  Serial.print(".");
  //  Serial.print(year());
  //  Serial.println("In Text: ");
  LatestTime = prepDigits(hour(), false);
  LatestTime += prepDigits(minute(), true);
  LatestTime += prepDigits(second(), true);
  Serial.print(LatestTime);
  Serial.print(" & ");
  String date = String(day());
  date += " ";
  date += days[weekday() - 1];
  date += ", ";
  date += months[month() - 1];
  date += " ";
  date += year();
  Serial.print(date);
  //Serial.print("## ");
  Serial.println(" ");
}

void printDigits(int digits)
{
  // utility for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if (digits < 10)
    Serial.print('0');
  Serial.print(digits);
}
String prepDigits(int digits, bool dot)
{
  String tVal = "";
  // utility for digital clock display: prints preceding colon and leading 0
  if (dot)
    tVal = ":";
  if (digits < 10)
    tVal += "0" + String(digits); // Serial.print('0');
  else
    tVal += String(digits);
  return tVal;
}
/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  Serial.println("Time synced");
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0) ; // discard any previously received packets

  Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
String getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = { 0, -1 };
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}
void getTempInfo() {
  Serial.println("Status\tHumidity (%)\tTemperature (C)\t(F)\tHeatIndex (C)\t(F)");
  float humidity = dht.getHumidity();
  float temperature = dht.getTemperature();
  Serial.print(dht.getStatusString());
  Serial.print("\t");
  Serial.print(humidity, 1);
  Serial.print("\t\t");
  Serial.print(temperature, 1);
  Serial.print("\t\t");
  Serial.print(dht.toFahrenheit(temperature), 1);
  Serial.print("\t\t");
  Serial.print(dht.computeHeatIndex(temperature, humidity, false), 1);
  Serial.print("\t\t");
  Serial.println(dht.computeHeatIndex(dht.toFahrenheit(temperature), humidity, true), 1);
}

void getParamsVal(String header) {
  int firstdot = header.indexOf('.');
  String Vars = header.substring(0, firstdot);
  int optIndx = find_text("alrm", Vars);
  Vars = Vars.substring(optIndx, Vars.length());
  Vars = getValue(Vars, ' ', 0);
  String alarmType = getValue(Vars, '&', 0);
  String timerVal, hh, mm, da;
  Serial.println("** Alarm Type");
  alarmType = getValue(alarmType, '=', 1);
  Serial.println(alarmType);
  if (alarmType == "1")
  {
    timerVal = getValue(Vars, '&', 1);
    Alarm.timerOnce(timerVal.toInt(), OnceOnly);
  }
  else if (alarmType == "2") {
    timerVal = getValue(Vars, '&', 1);
    Alarm.timerRepeat(timerVal.toInt(), Repeats);
  }
  else if (alarmType == "3") {
    String temp = getValue(Vars, '&', 1);
    hh = getValue(temp, ':', 0);
    mm = getValue(temp, ':', 1);
    Alarm.alarmRepeat(hh.toInt(), mm.toInt(), 0, DailyAlarm);
  }
  else if (alarmType == "4") {
    String temp = getValue(Vars, '&', 1);
    da = getValue(temp, ':', 0);
    hh = getValue(temp, ':', 1);
    mm = getValue(temp, ':', 2);
    String dayName = "dow";
    dayName += String(days[da.toInt()]);
    Serial.println("** Dayname");
    timeDayOfWeek_t dow = timeDayOfWeek_t(1);
    Serial.println(dow);
    Alarm.alarmRepeat(dow, hh.toInt(), mm.toInt(), 0, WeeklyAlarm);
  }
}
int find_text(String needle, String haystack) {
  int foundpos = -1;
  for (int i = 0; i <= haystack.length() - needle.length(); i++) {
    if (haystack.substring(i, needle.length() + i) == needle) {
      foundpos = i;
    }
  }
  return foundpos;
}
