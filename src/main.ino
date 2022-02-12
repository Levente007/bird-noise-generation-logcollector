#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include "SoftwareSerial.h"
#include <ESP8266mDNS.h>
#include <ESPAsyncWebServer.h>
#include "LittleFS.h"
#include "secrets.h"
#include "SDFS.h"
Secrets sec;

#define USE_SERIAL Serial

ESP8266WiFiMulti wifiMulti;
const uint32_t connectTimeoutMs = 5000;

AsyncWebServer server(80);

#define CHIPID "chipId"
#define LOG "log"

const char* APSSID = "logcollector-access-point";
const char* APPASS = "testpass"; //change this to an actual secure pass if testing is done

const char* SSID = sec.SSID;
const char* PASS = sec.pass;

//static fs::FS FILESYSTEM= SDFS; //if the development device doesnt have SD card, comment this out and use littleFS
static fs::FS FILESYSTEM= LittleFS;

void readFile(String chipId)
{
  File file = FILESYSTEM.open("/" + chipId + ".txt", "r");
  while (file.available())
  {
    String Line = file.readString();
    Serial.print(Line);
  }
  Serial.println("");
}

void sendLog(AsyncWebServerRequest *request)
{
  String chipId;
  String log;
  if (request->hasParam(LOG) & request->hasParam(CHIPID))
  {
    chipId = request->getParam(CHIPID)->value();
    log = request->getParam(LOG)->value();
    File file = FILESYSTEM.open("/" + chipId + ".txt", "a");
    file.print(log + "\r\n");
    Serial.print("Writed: " + chipId + ".txt");
    Serial.print(" ");
    Serial.print("Wrote: " + log);
    Serial.println("");
    file.flush();
    file.close();
    readFile(chipId);
    request->send(200, "text/json", "");
    return;
  }
  else
  {
    request->send(400, "text/json", "ERROR! PLEASE SEND CHIPID AND LOG");
    return;
  }
}

void setup()
{
  Serial.begin(9600);
  Serial.println("\n\nStartring Device Log Collector");

  Serial.println("\nESP8266 Multi WiFi example");

  // Set WiFi to station mode
  WiFi.mode(WIFI_STA);

  //set access point
  WiFi.softAP(APSSID, APPASS);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  // Register multi WiFi networks
  wifiMulti.addAP(SSID, PASS);
  wifiMulti.run(connectTimeoutMs);
  Serial.println(WiFi.SSID());

  //set server what to listen to
  server.on("/sendLog", HTTP_GET, sendLog);

  //start web server
  server.begin();
  Serial.println("HTTP server started");

  FILESYSTEM.begin();
}

void loop()
{
  
}