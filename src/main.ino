#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include "SoftwareSerial.h"
#include <ESP8266mDNS.h>
#include <ESPAsyncWebServer.h>
#include "LittleFS.h"

#define USE_SERIAL Serial
#define mp3RxPin D1
#define mp3TxPin D5
#define i2cSDAPin D6
#define i2cSCLPin D7

ESP8266WiFiMulti wifiMulti;
const uint32_t connectTimeoutMs = 5000;

SoftwareSerial mySoftwareSerial(D1, D5); // RX, TX

AsyncWebServer server(80);

#define CHIPID "chipId"
#define LOG "log"



void readFile(String chipId)
{
  File file = LittleFS.open("/" + chipId + ".txt", "r");
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
    File file = LittleFS.open("/" + chipId + ".txt", "a");
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

  mySoftwareSerial.begin(9600);
  Serial.begin(9600);
  Serial.println("\n\nStartring Device Log Collector");

  Serial.println("\nESP8266 Multi WiFi example");

  // Set WiFi to station mode
  WiFi.mode(WIFI_STA);

  // Register multi WiFi networks
  wifiMulti.addAP("Szurdokinet", "32Elemekcsb");

  server.on("/sendLog", HTTP_GET, sendLog);

  server.begin();
  Serial.println("HTTP server started");

  LittleFS.begin();
}

void loop()
{
  //Maintain WiFi connection
  wifiMulti.run(connectTimeoutMs);
  delay(1000);
}