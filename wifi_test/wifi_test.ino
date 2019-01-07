
#include <cactus_io_BME280_I2C.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <TimeLib.h>
#include <WiFiUdp.h>

char ssid[] = "Nazwa";                 // Network Name
char pass[] = "KaktusNaOknie13";                 // Network Password
byte mac[6];

String username = "Gmina_skawina_zelczyna";
String password = "test123";

WiFiServer server(80);
IPAddress ip(192, 168, 0, 5);
IPAddress ntpIP(198, 60, 22, 240);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);

WiFiClient client;
static const char ntpServerName[] = "us.pool.ntp.org"; 
static const int timeZone = 1;
time_t t;
WiFiUDP Udp;
unsigned int localPort = 1111;
static String auth_token = "";
HTTPClient http;

BME280_I2C bme;

void setup() {

  Serial.begin(9600);

  setup_BME280();
  
  connectWiFi();

  setup_time();


}

void loop() {

  
  if(WiFi.status() == WL_CONNECTED) {
    if(auth_token == "") {
      Serial.println("No token. Attempting to generate...");
      obtain_token();
    } else {
      send_JSON_data();
      delay(180000); //1800 sec
    }
  } else {
    Serial.println("WiFi connection lost! Please restart the program and/or check your router");
  }
}


void obtain_token() {
  StaticJsonBuffer<300> JSONbuffer;
  StaticJsonBuffer<300> jsonBuffer;
  JsonObject& JSONencoder = JSONbuffer.createObject();
  JSONencoder["username"] = username;
  JSONencoder["password"] = password; 
  char JSONMessageBuffer[300];
  JSONencoder.prettyPrintTo(JSONMessageBuffer, sizeof(JSONMessageBuffer));
  Serial.println(JSONMessageBuffer);
  
  http.begin("http://192.168.0.164:8000/obtain_token/");
  http.addHeader("Content-Type", "application/json");
  int httpCode = http.POST(JSONMessageBuffer);
  String Payload = http.getString();
  JsonObject& result = jsonBuffer.parseObject(Payload);
  if(result.success()) {
    auth_token = result["token"].asString();
  }
  http.end();

  Serial.print("Code: ");
  Serial.println(httpCode);
  Serial.print("Payload: ");
  Serial.println(Payload);
  Serial.println(auth_token);
}


void send_JSON_data() {
  t = now();
  bme.readSensor(); 
  Serial.print(bme.getPressure_HP()); Serial.print(" hPa\t"); // Pressure in millibars 
  Serial.print(bme.getHumidity()); Serial.print(" %\t\t"); 
  Serial.print(bme.getTemperature_C()); Serial.print(" *C\t"); 
  
  StaticJsonBuffer<500> JSONbuffer;
  JsonObject& JSONencoder = JSONbuffer.createObject();
  JSONencoder["Time_of_measurements"] = sqlTimestamp();
  JSONencoder["temperature_value"] = bme.getTemperature_C();
  JSONencoder["temperature_unit"] = "Celsius";
  JSONencoder["pressure_value"] = bme.getPressure_HP();
  JSONencoder["pressure_unit"] ="Pa";
  JSONencoder["humidity_value"] = bme.getHumidity();
  JSONencoder["humidity_unit"] = "%";

  char JSONMessageBuffer[500];
  JSONencoder.prettyPrintTo(JSONMessageBuffer, sizeof(JSONMessageBuffer));
  Serial.println(JSONMessageBuffer);
  
  http.begin("http://192.168.0.164:8000/send/");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Token " + String(auth_token));
  int httpCode = http.POST(JSONMessageBuffer);
  String payload = http.getString();
  http.end();

  Serial.print("Code: ");
  Serial.println(httpCode);
  Serial.print("Payload: ");
  Serial.println(payload);
}

String formatDigits(int digits) {
 String s = String(digits);
 if(digits <10) {
  return String(0) + s;
 }
 return s;
}

void setup_BME280() {
  Serial.println("Trying to connect BME280 - if it takes more then a few seconds - check wiring");
  if(!bme.begin()) {
    Serial.println(".");
  }
}
String sqlTimestamp() {
  String s = String(year(t)) + String("-") + String(month(t)) + String("-") + String(day(t)) + String(" ") + String(hour(t)) + String(":") + formatDigits(minute(t)) + String(":") + formatDigits(second(t));
  return s;
}

void setup_time() {
  Serial.print("IP number assigned by DHCP is ");
  Serial.println(WiFi.localIP());
  Serial.println("Starting UDP");
  Udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(Udp.localPort());
  Serial.println("waiting for sync");
  while(year() < 2017) {
    setSyncProvider(getNtpTime);
    }
  setSyncInterval(300);
}


const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets
time_t getNtpTime()
{

  IPAddress ntpServerIP; // NTP server's ip address
  
  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  ntpServerIP = ntpIP;
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

void connectWiFi() {
  Serial.println("Initialising connection");
  Serial.print(F("Setting static ip to : "));
  Serial.println(ip);

  Serial.println("");
  Serial.println("");
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.config(ip, gateway, subnet); 
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi Connected");

  WiFi.macAddress(mac);
  Serial.print("MAC: ");
  Serial.print(mac[5],HEX);
  Serial.print(":");
  Serial.print(mac[4],HEX);
  Serial.print(":");
  Serial.print(mac[3],HEX);
  Serial.print(":");
  Serial.print(mac[2],HEX);
  Serial.print(":");
  Serial.print(mac[1],HEX);
  Serial.print(":");
  Serial.println(mac[0],HEX);
  Serial.println("");
  Serial.print("Assigned IP: ");
  Serial.print(WiFi.localIP());
  Serial.println("");
}
