#include <FS.h>                                               // This needs to be first, or it all crashes and burns
#include "defines.h"
#include <IRremoteESP8266.h>
#include <IRsend.h>
//#include <IRutils.h>
#include <ESP8266WiFi.h>
#include "config.h"
#include <ESP8266mDNS.h>                                      // Useful to access to ESP by hostname.local
#include "util.h"
#include <ArduinoJson.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoOTA.h>
#include <Ticker.h>                                           // For LED status
#include <time.h>                       // time() ctime()
#include <sys/time.h>                   // struct timeval
#include <coredecls.h>                  // settimeofday_cb()
// SYSLOG
#ifdef SYSLOG
#include <Syslog.h>
#endif
#include "swamp_cooler.h"


#ifndef ARDUINO_OTA
#include <ESP8266HTTPUpdateServer.h>
#endif

#ifndef ARDUINO_OTA
ESP8266HTTPUpdateServer httpUpdater;
#endif

#ifdef SYSLOG
WiFiUDP udpClient;
Syslog syslog(udpClient, SYSLOG_SERVER, SYSLOG_PORT, HOST_NAME, HOST_NAME, LOG_KERN);

#endif
// User settings are below here
timeval cbtime;
bool cbtime_set = false;
void time_is_set(void)
{
  gettimeofday(&cbtime, NULL);
  cbtime_set = true;
  time_t now = time(nullptr);
  logM("Time is set to " + String(ctime(&now))); 
  
}

// Pins for Transmit
const int pins1 = 4;                                          // Transmitting preset 1

// User settings are above here
const int ledpin = BUILTIN_LED;                               // Built in LED defined for WEMOS people
const char *wifi_config_name = "RB5001 IR Controller";
const char *WEATHER_ENDPOINT = "http://esp/weather";
int port = 80;

ESP8266WebServer *server = NULL;
HTTPClient http;

Ticker LEDblinker, tickerGetWeather; 

IRsend irsend1(pins1);
const char* poolServerName = "pool.ntp.org";
#ifdef FS_BROWSER
//holds the current upload
File fsUploadFile;
class {
  private:
    float min=100,max=-100,current;
    int totalReading[MAXGET];
    uint8_t index=0;
    bool sampled = false;
    float getAverage() {
      uint8_t t_max = sampled ? MAXGET: index;
      int32_t total = 0;
      for(uint8_t i = 0; i< t_max; i++) {
        total += totalReading[i];
      }
      return total / (float) t_max;
    }

  public:
    void sample() {
      totalReading[index++] = analogRead(SENSORPIN);
      if (index > MAXGET) {
        index = 0;
        sampled = true;
      }
    }
    float getTemp(float input_voltage) {
       //int reading = analogRead(SENSORPIN); 
       // measure the 3.3v with a meter for an accurate value
       //In particular if your Arduino is USB powered
       float current = getAverage() * input_voltage; 
       current /= 1024.0; 
       // now print out the temperature
       current =  (current - 0.5) * 100;
       if (current < min) 
         min = current;
       if (current > max)
         max = current;
       return current;
    }
} room_temperature;

String getContentType(String filename) {
  if (server->hasArg("download")) {
    return "application/octet-stream";
  } else if (filename.endsWith(".htm")) {
    return "text/html";
  } else if (filename.endsWith(".html")) {
    return "text/html";
  } else if (filename.endsWith(".css")) {
    return "text/css";
  } else if (filename.endsWith(".js")) {
    return "application/javascript";
  } else if (filename.endsWith(".png")) {
    return "image/png";
  } else if (filename.endsWith(".gif")) {
    return "image/gif";
  } else if (filename.endsWith(".jpg")) {
    return "image/jpeg";
  } else if (filename.endsWith(".ico")) {
    return "image/x-icon";
  } else if (filename.endsWith(".xml")) {
    return "text/xml";
  } else if (filename.endsWith(".pdf")) {
    return "application/x-pdf";
  } else if (filename.endsWith(".zip")) {
    return "application/x-zip";
  } else if (filename.endsWith(".gz")) {
    return "application/x-gzip";
  }
  return "text/plain";
}

bool handleFileRead(String path) {
  DBG_OUTPUT_PORT.println("handleFileRead: " + path);
  if (path.endsWith("/")) {
    path += "index.htm";
  }
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) {
    if (SPIFFS.exists(pathWithGz)) {
      path += ".gz";
    }
    File file = SPIFFS.open(path, "r");
    server->streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

void handleFileUpload() {
  if (server->uri() != "/edit") {
    return;
  }
  HTTPUpload& upload = server->upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith("/")) {
      filename = "/" + filename;
    }
    DBG_OUTPUT_PORT.print("handleFileUpload Name: "); DBG_OUTPUT_PORT.println(filename);
    fsUploadFile = SPIFFS.open(filename, "w");
    filename = String();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    //DBG_OUTPUT_PORT.print("handleFileUpload Data: "); DBG_OUTPUT_PORT.println(upload.currentSize);
    if (fsUploadFile) {
      fsUploadFile.write(upload.buf, upload.currentSize);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile) {
      fsUploadFile.close();
    }
    DBG_OUTPUT_PORT.print("handleFileUpload Size: "); DBG_OUTPUT_PORT.println(upload.totalSize);
  }
}

void handleFileDelete() {
  if (server->args() == 0) {
    return server->send(500, "text/plain", "BAD ARGS");
  }
  String path = server->arg(0);
  DBG_OUTPUT_PORT.println("handleFileDelete: " + path);
  if (path == "/") {
    return server->send(500, "text/plain", "BAD PATH");
  }
  if (!SPIFFS.exists(path)) {
    return server->send(404, "text/plain", "FileNotFound");
  }
  SPIFFS.remove(path);
  server->send(200, "text/plain", "");
  path = String();
}

void handleFileCreate() {
  if (server->args() == 0) {
    return server->send(500, "text/plain", "BAD ARGS");
  }
  String path = server->arg(0);
  DBG_OUTPUT_PORT.println("handleFileCreate: " + path);
  if (path == "/") {
    return server->send(500, "text/plain", "BAD PATH");
  }
  if (SPIFFS.exists(path)) {
    return server->send(500, "text/plain", "FILE EXISTS");
  }
  File file = SPIFFS.open(path, "w");
  if (file) {
    file.close();
  } else {
    return server->send(500, "text/plain", "CREATE FAILED");
  }
  server->send(200, "text/plain", "");
  path = String();
}

void handleFileList() {
  if (!server->hasArg("dir")) {
    server->send(500, "text/plain", "BAD ARGS");
    return;
  }

  String path = server->arg("dir");
  DBG_OUTPUT_PORT.println("handleFileList: " + path);
  Dir dir = SPIFFS.openDir(path);
  path = String();

  String output = "[";
  while (dir.next()) {
    File entry = dir.openFile("r");
    if (output != "[") {
      output += ',';
    }
    bool isDir = false;
    output += "{\"type\":\"";
    output += (isDir) ? "dir" : "file";
    output += "\",\"name\":\"";
    output += String(entry.name()).substring(1);
    output += "\"}";
    entry.close();
  }

  output += "]";
  server->send(200, "text/json", output);
}
#endif

void getWeather() {
  http.begin(WEATHER_ENDPOINT);
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
      StaticJsonBuffer<200> jsonBuffer;
      JsonObject& root = jsonBuffer.parseObject(http.getString());
      if (root.success()) {
        uint32_t temperature = root["RawTemp"];
        uint32_t humidity = root["Humidity"];
        logM("Got outside weather " + String(temperature) + "Humidity " + String(humidity));
      }
  } else {
    logM("Failed to get WEATHER", LOG_ERR);
  }
}

//+=============================================================================
// Setup web server and IR receiver/blaster
//
void setup() {
  // Initialize serial
#ifdef SERIAL_DEBUG
  Serial.begin(115200);
  Serial.println("");
  Serial.println("ESP8266 IR Controller");
#endif

  SPIFFS.begin();
  settimeofday_cb(time_is_set);
  pinMode(ledpin, OUTPUT);
  // setup WIFI
  WiFi.hostname(HOST_NAME);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WiFiSSID, WiFiPSK);
#ifdef SERIAL_DEBUG
  Serial.print("Connecting");
#endif
  LEDblinker.attach(0.5, [](){
    int state = digitalRead(ledpin);
    digitalWrite(ledpin, !state);
  });
  //tickerGetWeather.attach(60, getWeather);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
#ifdef SERIAL_DEBUG
    Serial.print(".");
#endif
  }
  LEDblinker.detach();
  // keep LED off
  digitalWrite(ledpin, HIGH);

  server = new ESP8266WebServer(port);

#ifdef SERIAL_DEBUG
  Serial.print("Local IP: ");
  Serial.println(WiFi.localIP().toString());
#endif
  configTime(TZ_SEC,DST_SEC, poolServerName);
  logM( "irled connected to " + WiFi.localIP().toString());

#ifdef ARDUINO_OTA
  // Configure OTA Update
  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname(HOST_NAME);
  ArduinoOTA.onStart([]() {
#ifdef SERIAL_DEBUG
    Serial.println("Start");
#endif
  });
  ArduinoOTA.onEnd([]() {
#ifdef SERIAL_DEBUG
    Serial.println("\nEnd");
#endif
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
#ifdef SERIAL_DEBUG
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
#endif
  });
  ArduinoOTA.onError([](ota_error_t error) {
#ifdef SERIAL_DEBUG
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
#endif
  });
  ArduinoOTA.begin();
#ifdef SERIAL_DEBUG
  Serial.println("ArduinoOTA started");
#endif
#endif
  // Configure mDNS
  MDNS.addService("http", "tcp", port); // Announce the ESP as an HTTP service
  String tmp_str = "MDNS http service added. Hostname is set to " + String(HOST_NAME) + ".local:" + String(port);
  logM(tmp_str);
#ifdef SERIAL_DEBUG
  Serial.println(tmp_str);
#endif

#ifdef FS_BROWSER
  //SERVER INIT
  //list directory
  server->on("/list", HTTP_GET, handleFileList);
  //load editor
  server->on("/edit", HTTP_GET, [](){
    if(!handleFileRead("/edit.htm")) server->send(404, "text/plain", "FileNotFound");
  });
  //create file
  server->on("/edit", HTTP_PUT, handleFileCreate);
  //delete file
  server->on("/edit", HTTP_DELETE, handleFileDelete);
  //first callback is called after the request has ended with all parsed arguments
  //second callback handles file uploads at that location
  server->on("/edit", HTTP_POST, [](){ server->send(200, "text/plain", ""); }, handleFileUpload);

  //called when the url is not defined here
  //use it to load content from SPIFFS
  server->onNotFound([](){
    if(!handleFileRead(server->uri()))
      server->send(404, "text/plain", "FileNotFound");
  });
#endif
#ifdef SWAMP_COOLER 
  server->on("/swamp", []() {
    if (server->hasArg("device") and server->hasArg("state")) {
      String device = server->arg("device");
      device.toLowerCase();
      swampCoolerButton *button = selectButton(device);
      if (button == NULL) {
        server->send(403, "text/plain", "Device not recognize");
        return;
      }
      int state = server->arg("state").toInt();
      // if we get here we have device and state
      
      //This mode forces water pump to turn on 
      if (device == "power" and state == 2) {
        button->verifyState(1);
        button = selectButton("pump");
        button->verifyState(1);
        button = selectButton("fan");
        button->verifyState(2);
        server->send(200, "text/plan", "OK");
        return;
      }
      if (button->verifyState(state)) {
        server->send(200);
      } else {
        server->send(400, "text/plain", "Can't be set");
      }
    }
  });
  server->on("/temperature", []() {
      float voltage = server->hasArg("voltage") ? server->arg(voltage).toFloat() : 3.06;
      float celcius = room_temperature.getTemp(voltage);
      float Fehrenheit = celcius * 9.0 /5.0 + 32;
      server->send(200, "application/json", "{\"temperature\": {\"celcius\":"+String(celcius)+", \"fahrenheit\":"+String(Fehrenheit)+"}}");
  });

  server->on("/states", []() {
    if (server->hasArg("value")) {
      //StaticJsonBuffer<224> jsonBuffer;
      DynamicJsonBuffer jsonBuffer;
      logM("parsing value \"" + server->arg("value")+"\"");
      JsonArray& root = jsonBuffer.parseArray(server->arg("value"));
      if (!root.success()) {
        server->send(400, "text/plain", "JSON parsing failed");
        return;
      } else {
        for (int x = 0; x < root.size(); x++) {
          String device = root[x]["device"];
          int state = root[x]["state"];
          swampCoolerButton *button = selectButton(device);
          if (button == NULL) {
            server->send(403, "text/plain", "Device not recognize");
            return;
          }
          logM("setting device " +device + " to " + String(state));
          button->setState(state);
        }
      }
    }
    server->send(200,"application/json", returnSwampStates());
  });
#endif

#ifndef ARDUINO_OTA
  httpUpdater.setup(server, UPDATE_PATH, www_username,www_password);
#endif
  server->begin();
#ifdef SERIAL_DEBUG
  Serial.println("HTTP Server started on port " + String(port));
#endif
  irsend1.begin();

#ifdef SERIAL_DEBUG
  Serial.println("Ready to send and receive IR signals");
#endif
}
void loop() {
  server->handleClient();
  ArduinoOTA.handle();
  room_temperature.sample();
  delay(200);
}
