#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include "SoftwareSerial.h"
#include <ESP8266mDNS.h>
#include <ESPAsyncWebServer.h>
#include "LittleFS.h"
#include "secrets.h"
#include "SDFS.h"
#include "config.h"
Secrets sec;
Config config;

static String name = config.name;
static String ver = "1_1";

#define USE_SERIAL Serial
  
ESP8266WiFiMulti WiFiMulti;
const uint32_t connectTimeoutMs = 5000;

AsyncWebServer server(80);

#define CHIPID "chipId"
#define LOG "log"

const char *APSSID = "logcollector-access-point";
const char *APPASS = "testpass"; // change this to an actual secure pass if testing is done

const String server_url = config.server_url;

static fs::FS FILESYSTEM = SDFS; // if the development device doesnt have SD card, comment this out and use littleFS
// static fs::FS FILESYSTEM = LittleFS;

  WiFiClient client;
  HTTPClient http;

int POSTTask(String url, String payload) // Make a post request
{
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

void readFile(String chipId) // Read a chipId file every line of it not to necessary
{
  File file = FILESYSTEM.open("/logs/" + chipId + ".txt", "r");
  while (file.available())
  {
    String Line = file.readString();
    Serial.print(Line);
  }
  Serial.println("");
}

void sendLog(AsyncWebServerRequest *request) // Welcome a request with ChipId and a log and save it down
{
  String chipId;
  String log;
  if (request->hasParam(LOG) & request->hasParam(CHIPID))
  {
    //get the params
    chipId = request->getParam(CHIPID)->value();
    log = request->getParam(LOG)->value();

    //save the params
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

void SendLogToServer(File file, File unsentLogs, String name)// This function will actually read a file and send that to the birdNoise server
{ //                                //FIXME: rename variables for clarity
  while (file.available())
  {
    String payload;
    payload.reserve(1000);

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

    name.replace(".txt", ""); //FIXME: Check or remove .txt, rename variable to chipId
    String url = server_url + "/deviceLog/save?chipId=" + name;
    payload = "{\"timestamp\":" + String(timestamp) + ", \"messageCode\":\"" + String(messageCode) + "\", \"additional\":\"" + additional + "\"}";
    Serial.println(payload);
    int returned = POSTTask(url, payload);
    if (returned > 399 || returned < 0)
    {
      // if send fails, append the line to unsentlogs
      Serial.println("log send failed");
      unsentLogs.print(timestamp + "#" + String(messageCode) + "#" + additional + "\n");
      unsentLogs.flush();
    }
  }
}

void postLogs() // this function will search for sendable logs
{
    Dir dir = FILESYSTEM.openDir("/logs/");
    Dir unsentLogs = FILESYSTEM.openDir("/unsentlogs/");
    while (dir.next())
    {
      Serial.println("Posting logs for: " + dir.fileName());
      if (dir.fileSize())
      {
        File file = dir.openFile("r");
        File unsentlogs = FILESYSTEM.open("/unsentlogs/" + dir.fileName(), "a");

        String fileName = dir.fileName();
        fileName.remove('.txt');
        SendLogToServer(file, unsentlogs, fileName);

        unsentlogs.close();
        file.close();
        FILESYSTEM.remove("/logs/" + dir.fileName());
      }
    }
    while (unsentLogs.next())
    {
      Serial.println("Reposting logs for: " + unsentLogs.fileName());
      if (unsentLogs.fileSize())
      {
        File file = unsentLogs.openFile("r");
        File fileToAppend = FILESYSTEM.open("/logs/" + unsentLogs.fileName(), "a");
        String fileName = unsentLogs.fileName();
        fileName.remove('.txt');
        SendLogToServer(file, fileToAppend, fileName);

        file.close();
        fileToAppend.close();
        FILESYSTEM.remove("/unsentlogs/" + unsentLogs.fileName());
      }
    }
}


/**********************************************
             webupdate functions - only works with WiFi

***********************************************/
void updateFunc(String Name, String Version) // TODO: documentation
{
  HTTPClient http;

  String url = sec.update_server + "/check?" + "name=" + Name + "&ver=" + Version;
  USE_SERIAL.print("[HTTP] check at " + url);
  if (http.begin(client, url))
  { // HTTP

    USE_SERIAL.print("[HTTP] GET...\n");
    // start connection and send HTTP header
    int httpCode = http.GET();
    delay(10000); // wait for bootup of the server
    httpCode = http.GET();
    // httpCode will be negative on error
    if (httpCode > 0)
    {
      // HTTP header has been send and Server response header has been handled
      USE_SERIAL.printf("[HTTP] GET... code: %d\n", httpCode);

      // file found at server
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
      {
        String payload = http.getString();
        USE_SERIAL.println(payload);
        if (payload.indexOf("bin") > 0)
        {
          LittleFS.end();
          httpUpdateFunc(sec.update_server + payload);
        }
      }
    }
    else
    {
      USE_SERIAL.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
  }
}

void httpUpdateFunc(String update_url) // this is from the core example
{
  if ((WiFiMulti.run() == WL_CONNECTED))
  {

    // The line below is optional. It can be used to blink the LED on the board during flashing
    // The LED will be on during download of one buffer of data from the network. The LED will
    // be off during writing that buffer to flash
    // On a good connection the LED should flash regularly. On a bad connection the LED will be
    // on much longer than it will be off. Other pins than LED_BUILTIN may be used. The second
    // value is used to put the LED on. If the LED is on with HIGH, that value should be passed
    ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW);

    // Add optional callback notifiers
    ESPhttpUpdate.onStart(update_started);
    ESPhttpUpdate.onEnd(update_finished);
    ESPhttpUpdate.onProgress(update_progress);
    ESPhttpUpdate.onError(update_error);

    t_httpUpdate_return ret = ESPhttpUpdate.update(client, update_url);
    // Or:
    // t_httpUpdate_return ret = ESPhttpUpdate.update(client, "server", 80, "file.bin");

    switch (ret)
    {
    case HTTP_UPDATE_FAILED:
      USE_SERIAL.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
      break;

    case HTTP_UPDATE_NO_UPDATES:
      USE_SERIAL.println("HTTP_UPDATE_NO_UPDATES");
      break;

    case HTTP_UPDATE_OK:
      USE_SERIAL.println("HTTP_UPDATE_OK");
      break;
    }
  }
}

void update_started()
{
  USE_SERIAL.println("CALLBACK:  HTTP update process started");
}

void update_finished()
{
  USE_SERIAL.println("CALLBACK:  HTTP update process finished");
}

void update_progress(int cur, int total)
{
  USE_SERIAL.printf("CALLBACK:  HTTP update process at %d of %d bytes...\n", cur, total);
}

void update_error(int err)
{
  USE_SERIAL.printf("CALLBACK:  HTTP update fatal error code %d\n", err);
}
/**************end of section********************/


void setup()
{
  Serial.begin(9600);
  Serial.println("\n\nStartring Device Log Collector");

  Serial.println();
  Serial.print(F("name: "));
  Serial.print(name);
  Serial.print(F(" ver: "));
  Serial.println(ver);

  // Set WiFi to station mode
  WiFi.mode(WIFI_STA);

  // set access point
  WiFi.softAP(APSSID, APPASS);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  // Register multi WiFi networks
  WiFiMulti.addAP(sec.SSID_1,sec.pass_1);
  WiFiMulti.addAP(sec.SSID_2,sec.pass_2);
  WiFiMulti.run(connectTimeoutMs);
  Serial.println(WiFi.SSID());

  // set server what to listen to
  server.on("/sendLog", HTTP_GET, sendLog);

  // start web server
  server.begin();
  Serial.println("HTTP server started");
  delay(1000);

  FILESYSTEM.begin();


}

void loop()
{
  delay(3000);

  int numberOfNetworks = WiFi.scanNetworks();
  delay(100);
  if (numberOfNetworks > 0)
  {
    for (int i = 0; i < numberOfNetworks; i++)
    {
      Serial.println(WiFi.SSID(i));
    }
  }
  if (WiFiMulti.run() == WL_CONNECTED)
  {
  updateFunc(name,ver);
  postLogs();
  }
  else{
    Serial.println("WiFi not connected");
  }
}