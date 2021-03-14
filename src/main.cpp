/**
 * Example for the ESP32 HTTP(S) Webserver
 *
 * IMPORTANT NOTE:
 * To run this script, your need to
 *  1) Enter your WiFi SSID and PSK below this comment
 *  2) Make sure to have certificate data available. You will find a
 *     shell script and instructions to do so in the library folder
 *     under extras/
 *
 * This script will install an HTTPS Server on your ESP32 with the following
 * functionalities:
 *  - Show simple page on web server root
 *  - 404 for everything else
 */

// TODO: Configure your WiFi here
//#define byte uint8_t

#define DIR_PUBLIC "/public"

#include <Arduino.h>
//#include <ArduinoJson.h>
#include <ArduinoJson.h>
#include "creds.h"
// Include certificate data (see note above)
#include "cert.h"
#include "private_key.h"

// Binary data for the favicon
#include "favicon.h"

// We will use wifi
#include <WiFi.h>

// Includes for the server
#include <HTTPSServer.hpp>
#include <SSLCert.hpp>
#include <HTTPRequest.hpp>
#include <HTTPResponse.hpp>
#include <SPIFFS.h>
#include <WebsocketHandler.hpp>
#include <CircularBuffer.h>
#include <sstream>
#include <string.h>
#include <math.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include "time.h"
#include "cbor.h"
#include "measure.h"

#define MAX_CLIENTS 4

// The HTTPS Server comes in a separate namespace. For easier use, include it here.
using namespace httpsserver;

// Create an SSL certificate object from the files included above
SSLCert cert = SSLCert(
  example_crt_DER, example_crt_DER_len,
  example_key_DER, example_key_DER_len
);

char contentTypes[][2][32] = {
  {".html", "text/html"},
  {".css",  "text/css"},
  {".js",   "application/javascript"},
  {".json", "application/json"},
  {".png",  "image/png"},
  {".jpg",  "image/jpg"},
  {"", ""}
};

// Create an SSL-enabled server that uses the certificate
// The contstructor takes some more parameters, but we go for default values here.
HTTPSServer secureServer = HTTPSServer(&cert);

// Declare some handler functions for the various URLs on the server
// The signature is always the same for those functions. They get two parameters,
// which are pointers to the request data (read request body, headers, ...) and
// to the response data (write response, set status code, ...)
void handleRoot(HTTPRequest * req, HTTPResponse * res);
void handleFavicon(HTTPRequest * req, HTTPResponse * res);
void handle404(HTTPRequest * req, HTTPResponse * res);
void handlePowerMeterRequest(HTTPRequest * req, HTTPResponse * res);
void handleZeroRequest(HTTPRequest * req, HTTPResponse * res);
void handleSPIFFS(HTTPRequest * req, HTTPResponse * res);
void handlePowerSlopeRequest(HTTPRequest * req, HTTPResponse * res);
void initSPIFFS();
void setupOTA();
void setupMDNS();
void printLocalTime();
long secondsElapsed(long oldTime);
double energyTotal(CircularBuffer<double> *powerBuffer, int samples, double delayBetweenSamples)
double avgPower(CircularBuffer<double> *powerBuffer, int samples, double delayBetweenSamples)


const char* ntpServer = "pool.ntp.org";

CircularBuffer<double, 60> secondBuffer;
CircularBuffer<double, 60> minuteBuffer;
CircularBuffer<double, 24*4> hourBuffer;
CircularBuffer<double, 60> dayBuffer;
long lastMinuteTime;
long lastHourTime;
long lastDayTime;

const int adcReadInterval = 1000;
long unsigned lastRead = 0;
double power = 0;
double energyConsumption = 0;
extern double rawPower;
int sendAllTo = -1;
extern double ampsPerVolt;

// As websockets are more complex, they need a custom class that is derived from WebsocketHandler
class ClientHandler : public WebsocketHandler {
public:
  // This method is called by the webserver to instantiate a new handler for each
  // client that connects to the websocket endpoint
  static WebsocketHandler* create();

  // This method is called when a message arrives
  void onMessage(WebsocketInputStreambuf * input);

  // Handler function on connection close
  void onClose();
};

ClientHandler* activeClients[MAX_CLIENTS];

void setup() {
  // For logging
  Serial.begin(115200);
  initSPIFFS();
  // Connect to WiFi
  Serial.println("Setting up WiFi");
  WiFi.setHostname("power-meter");
  WiFi.begin(WIFI_SSID, WIFI_PSK);
  
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.print("Connected. IP=");
  Serial.println(WiFi.localIP());

  configTime(0, 3600, ntpServer);
  printLocalTime();

  // For every resource available on the server, we need to create a ResourceNode
  // The ResourceNode links URL and HTTP method to a handler function
  ResourceNode * nodeRoot    = new ResourceNode("/", "GET", &handleRoot);
  ResourceNode * nodeFavicon = new ResourceNode("/favicon.ico", "GET", &handleFavicon);
  //ResourceNode * node404     = new ResourceNode("", "GET", &handle404);
  
  // Add a handler that serves the current system uptime at GET /api/uptime
  ResourceNode * powerMeterRequestNode = new ResourceNode("/api/measurePower", "GET", &handlePowerMeterRequest);
  secureServer.registerNode(powerMeterRequestNode);

  ResourceNode * powerSlopeNode = new ResourceNode("/api/slope", "PUT", &handlePowerSlopeRequest);
  secureServer.registerNode(powerSlopeNode);

  ResourceNode * handleZeroRequestNode = new ResourceNode("/api/zero", "GET", &handleZeroRequest);
  secureServer.registerNode(handleZeroRequestNode);

  for(int i = 0; i < MAX_CLIENTS; i++) activeClients[i] = nullptr;
  WebsocketNode * clientConnectionNode = new WebsocketNode("/live", &ClientHandler::create);
  // Adding the node to the server works in the same way as for all other nodes
  secureServer.registerNode(clientConnectionNode);

  // Add the root node to the server
  secureServer.registerNode(nodeRoot);

  // Add the favicon
  secureServer.registerNode(nodeFavicon);

  // Add the 404 not found node to the server.
  // The path is ignored for the default node.
  //secureServer.setDefaultNode(node404);

  ResourceNode * spiffsNode = new ResourceNode("", "", &handleSPIFFS);
  secureServer.setDefaultNode(spiffsNode);

  Serial.println("Starting server...");
  secureServer.start();
  if (secureServer.isRunning()) {
    Serial.println("Server ready.");
  }

  setupMDNS();
  setupADC();
  setupOTA();
}

void setupMDNS()
{
    if (!MDNS.begin("power-meter")) {
      Serial.println("Error setting up MDNS responder!");
      while(1) {
          delay(1000);
      }
  }
  Serial.println("mDNS responder started");
}

void setupOTA()
{


  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
  ArduinoOTA.setHostname("power-meter");
  ArduinoOTA.setPort(3232);
  ArduinoOTA.begin();
}


void loop() {
  ArduinoOTA.handle();
  // This call will let the server do its work
  secureServer.loop();
  long elapsed = millis() - (long)lastRead;
  double delta = elapsed / 1000.0;
  if(elapsed>=adcReadInterval){
    double lastRead = power;
    power = readPower();
    lastRead = millis();
    secondBuffer.push(power);

    time_t currentTime;
    time(&currentTime);
    if(secondsElapsed(lastMinuteTime)>60 && secondBuffer.size()>60){      
      minuteBuffer.push( avgPower(secondsBuffer,60,adcReadInterval/1000.0) );      
      lastMinuteTime = (long)time_t;     
    }
    if(secondsElapsed(lastHourTime)>60*60 && minuteBuffer.size()>60){
      hourBuffer.push( avgPower(minuteBuffer,60,60L*adcReadInterval/1000.0) );
      lastHourTime = (long)time_t;     
    }
    if(secondsElapsed(hourBuffer)>24*60*60 && hourBuffer.size()>24){
      dayBuffer.push( avgPower(minuteBuffer,60,60L*60*adcReadInterval/1000.0) );
      lastDayTime = (long)time_t;
    }
    energyConsumption += delta * 0.5 * ( power + lastPower );
  
    
    for(int i = 0; i < MAX_CLIENTS; i++) {

      if (activeClients[i] != nullptr) {
        
        if(i==sendAllTo){
          std::ostringstream msgStream;
          for(decltype(secondBuffer)::index_t j=max(buffer.size()-60,0);j<secondBuffer.size();j++){          
            msgStream << secondBuffer[j];
            msgStream << "\n";
            //activeClients[i]->send(msgStream.str(),WebsocketHandler::SEND_TYPE_TEXT);        
            
          }
          activeClients[i]->send(msgStream.str(),WebsocketHandler::SEND_TYPE_TEXT);
          msgStream.clear();   
          sendAllTo = -1;
        }
        else {
          std::ostringstream msgStream;
          msgStream << power;
          msgStream << "\n";
          activeClients[i]->send(msgStream.str(), WebsocketHandler::SEND_TYPE_TEXT);
          msgStream.clear();   
        }
      
      }
      
    }
  }
  // Other code would go here...
  delay(1);
}

void initSPIFFS()
{
  if (!SPIFFS.begin(false)) {
    // If SPIFFS does not work, we wait for serial connection...
    while(!Serial);
    delay(1000);

    // Ask to format SPIFFS using serial interface
    Serial.print("Mounting SPIFFS failed. Try formatting? (y/n): ");
    while(!Serial.available());
    Serial.println();

    // If the user did not accept to try formatting SPIFFS or formatting failed:
    if (Serial.read() != 'y' || !SPIFFS.begin(true)) {
      Serial.println("SPIFFS not available. Stop.");
      while(true);
    }
    Serial.println("SPIFFS has been formated.");
  }
  Serial.println("SPIFFS has been mounted.");
}

void handleRoot(HTTPRequest * req, HTTPResponse * res) {
  // Status code is 200 OK by default.
  // We want to deliver a simple HTML page, so we send a corresponding content type:
  res->setHeader("Content-Type", "text/html");
  res->setStatusCode(302);
  res->setHeader("Location","/index.html");
  
  // The response implements the Print interface, so you can use it just like
  // you would write to Serial etc.
  res->println("<!DOCTYPE html>");
  res->println("<html>");
  res->println("<head><title>Hello World!</title></head>");
  res->println("<body>");
  res->print("<p>Your server is running for ");
  // A bit of dynamic data: Show the uptime
  res->print((int)(millis()/1000), DEC);
  res->println(" seconds.</p>");
  res->println("</body>");
  res->println("</html>");
}

void handleFavicon(HTTPRequest * req, HTTPResponse * res) {
  // Set Content-Type
  res->setHeader("Content-Type", "image/vnd.microsoft.icon");
  // Write data from header file
  res->write(FAVICON_DATA, FAVICON_LENGTH);
}

void handle404(HTTPRequest * req, HTTPResponse * res) {
  // Discard request body, if we received any
  // We do this, as this is the default node and may also server POST/PUT requests
  req->discardRequestBody();

  // Set the response status
  res->setStatusCode(404);
  res->setStatusText("Not Found");

  // Set content type of the response
  res->setHeader("Content-Type", "text/html");

  // Write a tiny HTTP page
  res->println("<!DOCTYPE html>");
  res->println("<html>");
  res->println("<head><title>Not Found</title></head>");
  res->println("<body><h1>404 Not Found</h1><p>The requested resource was not found on this server.</p></body>");
  res->println("</html>");
}


/**
 * This function will return the uptime in seconds as JSON object:
 * {"uptime": 42}
 */
void handlePowerMeterRequest(HTTPRequest * req, HTTPResponse * res) {
  // Create a buffer of size 1 (pretty simple, we have just one key here)
  StaticJsonDocument<JSON_OBJECT_SIZE(4)> obj;
  // Set the uptime key to the uptime in seconds
  obj["uptime"] = millis()/1000;
  obj["power"] = power;
  obj["slope"] = ampsPerVolt;
  obj["energy"] = energyConsumption;
  // Set the content type of the response
  res->setHeader("Content-Type", "application/json");
  // As HTTPResponse implements the Print interface, this works fine. Just remember
  // to use *, as we only have a pointer to the HTTPResponse here:
  serializeJson(obj, *res);
}

void handlePowerSlopeRequest(HTTPRequest * req, HTTPResponse * res){
  char buffer[200];
  int len = req->readChars(buffer,200);
  StaticJsonDocument<200> doc;
  deserializeJson(doc, buffer, len);
  ampsPerVolt = doc["slope"];
}

void handleZeroRequest(HTTPRequest * req, HTTPResponse * res) {
  StaticJsonDocument<JSON_OBJECT_SIZE(1)>obj;

  obj["status"] = "okay";
  res->setHeader("Content-Type", "application/json");
  serializeJson(obj, *res);
  calibratePowerOffset();
}

void handleSPIFFS(HTTPRequest * req, HTTPResponse * res) {
	
  // We only handle GET here
  if (req->getMethod() == "GET") {
    // Redirect / to /index.html
    std::string reqFile = req->getRequestString()=="/" ? "/index.html" : req->getRequestString();

    // Try to open the file
    std::string filename = std::string(DIR_PUBLIC) + reqFile;

    // Check if the file exists
    if (!SPIFFS.exists(filename.c_str())) {
      // Send "404 Not Found" as response, as the file doesn't seem to exist
      res->setStatusCode(404);
      res->setStatusText("Not found");
      res->println("404 Not Found");
      return;
    }

    File file = SPIFFS.open(filename.c_str());

    // Set length
    res->setHeader("Content-Length", httpsserver::intToString(file.size()));

    // Content-Type is guessed using the definition of the contentTypes-table defined above
    int cTypeIdx = 0;
    do {
      if(reqFile.rfind(contentTypes[cTypeIdx][0])!=std::string::npos) {
        res->setHeader("Content-Type", contentTypes[cTypeIdx][1]);
        break;
      }
      cTypeIdx+=1;
    } while(strlen(contentTypes[cTypeIdx][0])>0);

    // Read the file and write it to the response
    uint8_t buffer[256];
    size_t length = 0;
    do {
      length = file.read(buffer, 256);
      res->write(buffer, length);
    } while (length > 0);

    file.close();
  } else {
    // If there's any body, discard it
    req->discardRequestBody();
    // Send "405 Method not allowed" as response
    res->setStatusCode(405);
    res->setStatusText("Method not allowed");
    res->println("405 Method not allowed");
  }
}

void ClientHandler::onMessage(WebsocketInputStreambuf * inbuf) {
  
}

WebsocketHandler * ClientHandler::create() {
  Serial.println("Client Connecting");
  ClientHandler * handler = new ClientHandler();
  for(int i = 0; i < MAX_CLIENTS; i++) {
    if (activeClients[i] == nullptr) {
      activeClients[i] = handler; 
      sendAllTo = i;     
      break;
    }
  }
  

  
  return handler;
}

// When the websocket is closing, we remove the client from the array
void ClientHandler::onClose() {
  for(int i = 0; i < MAX_CLIENTS; i++) {
    if (activeClients[i] == this) {
      activeClients[i] = nullptr;
    }
  }
}

void printLocalTime()
{
  struct tm timeinfo;
  
  time_t cTime;
  time(&cTime);
  Serial.println(cTime);
  
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }



  clock_
  
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

double energyTotal(CircularBuffer<double> *powerBuffer, int samples, double delayBetweenSamples)
{
  int len = powerBuffer->size();
  double sum;
  for(int i=1;i<len;i++){
    sum += delayBetweenSamples * 0.5 * (powerBuffer[i-1] + powerBuffer[i]);    
  }
  return sum;
}

double avgPower(CircularBuffer<double> *powerBuffer, int samples, double delayBetweenSamples)
{
  return energyTotal(powerBuffer,samples,delayBetweenSamples)/(samples*delayBetweenSamples);
}

long secondsElapsed(long oldTime, time_t t)
{
  long currentTime = (long)t;
  return currentTime - oldTime;
} 