#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoOTA.h>
#include "basicOTA.h"
#include "secrets.h"

// thingspeak
#include "ThingSpeak.h"
unsigned long myChannelNumber = SECRET_CH_ID;
const char *myWriteAPIKey = SECRET_WRITE_APIKEY;
WiFiClient client;

// GPIO where the DS18B20 is connected to
#define ONE_WIRE_PIN D7

#define LED_RELAY D6
#define RELAY_PIN D1

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_PIN);

// Pass our oneWire reference to Dallas Temperature sensor
DallasTemperature sensors(&oneWire);

// wifi + webserver
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
const char *ssid = SECRET_SSID;
const char *password = SECRET_PASS;

ESP8266WebServer server(80);

bool relayOn = false;
bool previousRelayOn = false;
float temp;

unsigned long lastTime = 0;
unsigned long timerDelay = 20000;

void handleNotFound()
{
  String message = "File Not Found";
  server.send(404, "text/plain", message);
}

void handleRoot()
{
  // use https://www.textfixer.com/tools/paragraph-to-lines.php to convert html to single line then replace "  with \"

  String html = "<!DOCTYPE html> <html> <head> <title>Fan Control</title> <link rel=\"stylesheet\" href=\"https://cdnjs.cloudflare.com/ajax/libs/bootswatch/5.2.3/superhero/bootstrap.min.css\" integrity=\"sha512-OIkcyk7xM5npH4qAW0dlLVzXsAzumZZnHfOB3IL17cXO6bNIK4BpYSh0d63R1llgUcrWZ709bCJhenNrEsno0w==\" crossorigin=\"anonymous\" referrerpolicy=\"no-referrer\" /> </head> <body> <div class=\"container\"> <div class=\"row\"> <div class=\"col-lg-12\"> <div> <h2>&nbsp;</h2> <h1>Loft Temperature</h1>";


  html.concat("<div align=\"center\"><br/><br/><iframe width=\"450\" height=\"260\" style=\"border: 1px solid #cccccc;\" src=\"https://thingspeak.com/channels/2073508/charts/1?bgcolor=%23000000&color=%23d62020&dynamic=true&results=600&timescale=10&type=line\"></iframe></div>");

  html.concat("</div> </div> </div> <div class=\"row\"> <div class=\"col-lg-12\"> <div> <h2>&nbsp;</h2> <h1>Fan Control</h1> </div> </div> </div> <div class=\"row\"> <div class=\"col-lg-12\"> <h2 class=\"text-info\">");

  html.concat(relayOn ? "Running" : "Fan is off");

  html.concat("</h2> <h2>&nbsp;</h2> <div class=\"row\"> <div class=\"col-6\"> <div class=\"bs-component\"> <div class=\"d-grid\"> <a href=\"/on\" class=\"btn btn-lg btn-success\" type=\"button\">ON</a> </div> </div> </div> <div class=\"col-6\"> <div class=\"bs-component\"> <div class=\"d-grid\"> <a href=\"/off\" class=\"btn btn-lg btn-danger\" type=\"button\">OFF</a> </div> </div> </div> </div> </div> </div> </div> </body> </html>");

  server.send(200, "text/html", html);
}

void handleOn()
{
  relayOn = true;
  handleRoot();
}

void handleOff()
{
  relayOn = false;
  handleRoot();
}

void digitalToggle(byte pin)
{
  digitalWrite(pin, !digitalRead(pin));
}

void setup()
{
  // Start the serial for debug
  Serial.begin(115200);

  // Start the DS18B20 sensor
  sensors.begin();
  // LEDS
  pinMode(LED_RELAY, OUTPUT);
  digitalWrite(LED_RELAY, LOW);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  // RELAY
  pinMode(RELAY_PIN, OUTPUT);

  WiFi.begin(ssid, password);

  ThingSpeak.begin(client);

  // Wait for connection
  Serial.print("Connecting to wifi ");
  while (WiFi.status() != WL_CONNECTED)
  {
    digitalToggle(LED_RELAY);

    delay(100);

    Serial.print(".");
  }
  digitalWrite(LED_RELAY, LOW);
  Serial.println("Connected");
  Serial.println(WiFi.localIP());

  MDNS.begin("fanrelay.local");

  server.on("/", handleRoot);
  server.on("/on", handleOn);
  server.on("/off", handleOff);
  server.onNotFound(handleNotFound);
  server.begin();

  // Setup Firmware update over the air (OTA)
  setup_OTA();
}

void loop()
{
  server.handleClient();

  if (relayOn != previousRelayOn)
  {
    previousRelayOn = relayOn;
    digitalWrite(LED_RELAY, relayOn ? HIGH : LOW);
    digitalWrite(RELAY_PIN, relayOn ? HIGH : LOW);
  }

  if ((millis() - lastTime) > timerDelay)
  {
    lastTime = millis();
    sensors.requestTemperatures();
    temp = sensors.getTempCByIndex(0);

    ThingSpeak.setField(1, temp);
    ThingSpeak.setField(2, relayOn ? 1 : 0);

    ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);

    Serial.println(temp);
    digitalToggle(LED_BUILTIN);
  }

  // Check for OTA updates
  ArduinoOTA.handle();
}
