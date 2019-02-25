
#include <FS.h>
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DNSServer.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <ESP8266WebServer.h>

// which analog pin to connect
#define THERMISTORPIN A0
// resistance at 25 degrees C
#define THERMISTORNOMINAL 10020
// temp. for nominal resistance (almost always 25 C)
#define TEMPERATURENOMINAL 25
// how many samples to take and average, more takes longer
// but is more 'smooth'
#define NUMSAMPLES 25
// The beta coefficient of the thermistor (usually 3000-4000)
#define BCOEFFICIENT 3950
// the value of the 'other' resistor
#define SERIESRESISTOR 10040

uint16_t samples[NUMSAMPLES];
WiFiManager wifiManager;
WiFiClient espClient;
PubSubClient client(espClient);
ESP8266WebServer server(80);

String clientId;
char *mqtt_server = (char *)"";
char *mqtt_port = (char *)"1883";
char *mqtt_user = (char *)"";
char *mqtt_password = (char *)"";
char *mqtt_topic = (char *)"home/livingroom/temperature";
char *event_location = (char *)"home";
char *event_place = (char *)"livingroom";
char *event_type = (char *)"temperature";
float event_adjustment = 0.0;
bool shouldSaveConfig = false;

float readEvent()
{
  uint8_t i;
  float average;

  // take N samples in a row, with a slight delay
  for (i = 0; i < NUMSAMPLES; i++)
  {
    samples[i] = analogRead(THERMISTORPIN);
    delay(10);
  }

  // average all the samples out
  average = 0;
  for (i = 0; i < NUMSAMPLES; i++)
  {
    average += samples[i];
  }
  average /= NUMSAMPLES;

  // convert the value to resistance
  average = 1023 / average - 1;
  average = SERIESRESISTOR / average;

  float steinhart;
  steinhart = average / THERMISTORNOMINAL;          // (R/Ro)
  steinhart = log(steinhart);                       // ln(R/Ro)
  steinhart /= BCOEFFICIENT;                        // 1/B * ln(R/Ro)
  steinhart += 1.0 / (TEMPERATURENOMINAL + 273.15); // + (1/To)
  steinhart = 1.0 / steinhart;                      // Invert
  steinhart -= 273.15;                              // convert to C

  return steinhart + event_adjustment;
}

long lastMsg = 0;
float temp = 0.0;
float hum = 0.0;
float diff = 0.1;
float newTemp = 0.0;
int tempCount = 0;

bool checkBound(float newValue, float prevValue, float maxDiff)
{
  return !isnan(newValue) &&
         (newValue < prevValue - maxDiff || newValue > prevValue + maxDiff);
}

void publishTemperature()
{
  newTemp = newTemp + readEvent();
  tempCount++;

  long now = millis();
  if (now - lastMsg > 5000)
  {
    lastMsg = now;

    newTemp = newTemp / (float)tempCount;
    if (checkBound(newTemp, temp, diff))
    {
      temp = newTemp;
      Serial.print("New temperature:");
      Serial.println(String(temp).c_str());
      String tempData = "temperature,location="+String(event_location)+",place="+String(event_place)+" "+String(event_type)+"=" + String(temp);
      client.publish(mqtt_topic, tempData.c_str(), true);
    }
    newTemp = 0;
    tempCount = 0;
  }
}

void sendIndexPage(){
  float tmp = readEvent();
  server.send(200, "text/html", "<html><span>Temperature: "+String(tmp)+"</span><form action=\"/\" method=\"POST\">Adjustment: <input type=\"input\" name=\"event_adjustment\" value=\""+String(event_adjustment)+"\"/><br/><input type=\"submit\" value=\"Save\"/></form></html>");
}

void SaveConfigCallback()
{
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void readConfigFile(){
  if (SPIFFS.begin())
  {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/cfg.json"))
    {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/cfg.json", "r");
      if (configFile)
      {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject &json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success())
        {
          Serial.println("\nparsed json");

          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
          strcpy(mqtt_user, json["mqtt_user"]);
          strcpy(mqtt_password, json["mqtt_password"]);
          strcpy(mqtt_topic, json["mqqt_topic"]);
          event_adjustment = json.get<float>("event_adjustment");
          strcpy(event_type, json["event_type"]);
          strcpy(event_location, json["event_location"]);
          strcpy(event_place, json["event_place"]);
        }
        else
        {
          Serial.println("failed to load json config");
        }
        configFile.close();
      }
    }
  }
  else
  {
    Serial.println("failed to mount FS");
  }
  //end read
}

void writeConfigFile(){
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject &json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
    json["mqtt_user"] = mqtt_user;
    json["mqtt_password"] = mqtt_password;
    json["mqtt_topic"] = mqtt_topic;
    json["event_location"] = event_location;
    json["event_place"] = event_place;
    json["event_type"] = event_type;
    json["event_adjustment"] = event_adjustment;

    File configFile = SPIFFS.open("/cfg.json", "w");
    if (!configFile)
    {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
}

void handleRootGET() {
  sendIndexPage();
}

void handleRootPOST() {
  if( !server.hasArg("event_adjustment") || server.arg("event_adjustment") == NULL) {
    server.send(400, "text/plain", "400: Invalid Request");
    return;
  }
  event_adjustment = server.arg("event_adjustment").toFloat();
  writeConfigFile();
  sendIndexPage();
}

void handleReset(){
  Serial.println("Reset WiFi settings.");
  ESP.eraseConfig();
  delay(3000);
  ESP.reset();
  delay(1000);
}

void handleNotFound()
{
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void setup_wifi()
{
  //read configuration from FS json
  Serial.println("mounting FS...");

  readConfigFile();

  String mac = WiFi.macAddress();
  mac.replace(":", "");
  clientId = "IoT_" + mac;
  Serial.println("Chip Id:   " + String(ESP.getChipId()));
  Serial.println("Client Id: " + clientId);
  Serial.println("MAC:       " + mac);

  WiFiManagerParameter custom_mqtt_server("server", "MQTT server", mqtt_server, 64);
  WiFiManagerParameter custom_mqtt_port("port", "MQTT port", mqtt_port, 6);
  WiFiManagerParameter custom_mqtt_user("user", "MQTT user", mqtt_user, 32);
  WiFiManagerParameter custom_mqtt_password("password", "MQTT password", mqtt_password, 32);
  WiFiManagerParameter custom_mqtt_topic("topic", "MQTT topic", mqtt_topic, 64);
  WiFiManagerParameter custom_event_adjustment("adjustment", "Temp adjustment", String(event_adjustment).c_str(), 6);
  WiFiManagerParameter custom_event_type("eventType", "Event type", event_type, 64);
  WiFiManagerParameter custom_event_location("eventLocation", "Event location", event_location, 64);
  WiFiManagerParameter custom_event_place("eventPlace", "Event place", event_place, 64);

  wifiManager.setSaveConfigCallback(SaveConfigCallback);
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_mqtt_user);
  wifiManager.addParameter(&custom_mqtt_password);
  wifiManager.addParameter(&custom_mqtt_topic);
  wifiManager.addParameter(&custom_event_location);
  wifiManager.addParameter(&custom_event_place);
  wifiManager.addParameter(&custom_event_type);
  wifiManager.addParameter(&custom_event_adjustment);

  wifiManager.autoConnect(clientId.c_str());

  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(mqtt_user, custom_mqtt_user.getValue());
  strcpy(mqtt_password, custom_mqtt_password.getValue());
  strcpy(mqtt_topic, custom_mqtt_topic.getValue());
  strcpy(event_type, custom_event_type.getValue());
  strcpy(event_location, custom_event_location.getValue());
  strcpy(event_place, custom_event_place.getValue());
  event_adjustment = String(custom_event_adjustment.getValue()).toFloat();

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("MQTT Server: ");
  Serial.println(mqtt_server);
  Serial.print("MQTT Topic: ");
  Serial.println(mqtt_topic);

  //save the custom parameters to FS
  if (shouldSaveConfig)
  {
    writeConfigFile();
  }
}

void reconnect()
{
  int reconnectCount = 0;
  // Loop until we're reconnected
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    // If you do not want to use a username and password, change next line to
    // if (client.connect("ESP8266Client")) {
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_password))
    {
      Serial.println("connected");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      for(size_t i = 0; i < 10; i++)
      {
        server.handleClient();
        delay(500);
      }

      reconnectCount++;
      if(reconnectCount > 10){

      }
    }
  }
}

void setup()
{
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, (int)mqtt_port);

  server.on("/", HTTP_GET, handleRootGET);
  server.on("/", HTTP_POST, handleRootPOST);
  server.on("/reset", HTTP_GET, handleReset);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");
}

void loop()
{
  server.handleClient();

  if (!client.connected())
  {
    reconnect();
  }
  client.loop();

  publishTemperature();
}
