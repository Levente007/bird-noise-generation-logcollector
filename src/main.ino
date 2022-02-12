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
#include "config.h"
Secrets sec;
Config config;

#define USE_SERIAL Serial

ESP8266WiFiMulti wifiMulti;
const uint32_t connectTimeoutMs = 5000;

AsyncWebServer server(80);

#define CHIPID "chipId"
#define LOG "log"

const char *APSSID = "logcollector-access-point";
const char *APPASS = "testpass"; // change this to an actual secure pass if testing is done

const char *SSID = sec.SSID;
const char *PASS = sec.pass;

const String server_url = config.server_url;

// static fs::FS FILESYSTEM= SDFS; //if the development device doesnt have SD card, comment this out and use littleFS
static fs::FS FILESYSTEM = LittleFS;

int POSTTask(String url, String payload)
{
  WiFiClient client;
  HTTPClient http;
  if (http.begin(client, url))
  {
    Serial.print(F("[HTTPS] POST "));
    Serial.print(url);
    Serial.print(" --> ");
    Serial.println(payload);
    http.addHeader(F("Content-Type"), F("application/json"));

    int httpCode = http.POST(payload);

    // httpCode will be negative on error
    if (httpCode > 0)
    {
      // HTTP header has been send and Server response header has been handled
      Serial.print(F("[HTTPS] POST... code: "));
      Serial.println(httpCode);

      // file found at server
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
      {
        String payload = http.getString();
        http.end();
        return httpCode;
      }
    }
    else
    {
      Serial.print(F("[HTTPS] POST... failed, error: "));
      Serial.println(httpCode);
      Serial.println(" " + http.getString());
      http.end();
      return httpCode;
    }

    http.end();
  }
  else
  {
    Serial.println(F("[HTTPS] Unable to connect"));
    return 999;
  }
  return 999;
};

void readFile(String chipId)
{
  File file = FILESYSTEM.open("/logs/" + chipId + ".txt", "r");
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
    File file = FILESYSTEM.open("/logs/" + chipId + ".txt", "a");
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

void postLogs()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    Dir dir = FILESYSTEM.openDir("/logs/");
    while (dir.next())
    {
      Serial.println("Posting logs for: " + dir.fileName());
      if (dir.fileSize())
      {
        File file = dir.openFile("r");
        File unsentlogs = FILESYSTEM.open("/unsentlogs/" + dir.fileName(), "a");
        String payload;
        payload.reserve(1000);
        while (file.available())
        {
          // read up logline
          unsigned long timestamp = file.readStringUntil('#').toInt();
          Serial.print("timestamp: " + String(timestamp));
          int messageCode = file.readStringUntil('#').toInt();
          Serial.print(", messageCode: " + String(messageCode));
          String additional;
          additional.reserve(30);
          additional = file.readStringUntil('\n');
          additional.trim();
          Serial.println(", additional: " + additional);

          // send logline to server
          String name = dir.fileName();
          name.replace(".txt", "");
          String url = server_url + "/deviceLog/save?chipId=" + name;
          payload = "{\"timestamp\":" + String(timestamp) + ", \"messageCode\":\"" + String(messageCode) + "\", \"additional\":\"" + additional + "\"}";
          Serial.println(payload);
          int returned = POSTTask(url, payload);
          if (returned > 399 || returned < 0)
          {
            // if send fails, append the line to unsentlogs
            Serial.println("log send failed");
            unsentlogs.print(timestamp + "#" + String(messageCode) + "#" + additional + "\n");
            unsentlogs.flush();
          }
        }
        unsentlogs.close();
        file.close();
        FILESYSTEM.remove("/logs/" + dir.fileName());
      }
    }
  }
  Dir unsentLogs = FILESYSTEM.openDir("/unsentlogs/");
  while (unsentLogs.next())
  {
    Serial.println("Reposting logs for: " + unsentLogs.fileName());
    if (unsentLogs.fileSize())
    {
      File file = unsentLogs.openFile("r");
      String payload;
      payload.reserve(1000);
      while (file.available())
      {
        // read up logline
        unsigned long timestamp = file.readStringUntil('#').toInt();
        Serial.print("timestamp: " + String(timestamp));
        int messageCode = file.readStringUntil('#').toInt();
        Serial.print(", messageCode: " + String(messageCode));
        String additional;
        additional.reserve(30);
        additional = file.readStringUntil('\n');
        additional.trim();
        Serial.println(", additional: " + additional);

        // send logline to server
        String url = server_url + "/deviceLog/save?chipId=" + ESP.getChipId();
        payload = "{\"timestamp\":" + String(timestamp) + ", \"messageCode\":\"" + String(messageCode) + "\", \"additional\":\"" + additional + "\"}";
        Serial.println(payload);
        int returned = POSTTask(url, payload);
        if (returned > 399 || returned < 0)
        {
          // if send fails, append the line to unsentlogs
          Serial.println("log resend failed");
        }
      }
    }
  }
}
  void setup()
  {
    Serial.begin(9600);
    Serial.println("\n\nStartring Device Log Collector");

    Serial.println("\nESP8266 Multi WiFi example");

    // Set WiFi to station mode
    WiFi.mode(WIFI_STA);

    // set access point
    WiFi.softAP(APSSID, APPASS);
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);

    // Register multi WiFi networks
    wifiMulti.addAP(SSID, PASS);
    wifiMulti.run(connectTimeoutMs);
    Serial.println(WiFi.SSID());

    // set server what to listen to
    server.on("/sendLog", HTTP_GET, sendLog);

    // start web server
    server.begin();
    Serial.println("HTTP server started");
    delay(1000);

    FILESYSTEM.begin();
    postLogs();
  }

  void loop()
  {
  }