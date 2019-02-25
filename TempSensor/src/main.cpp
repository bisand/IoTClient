
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

const char *clientId;
char *mqtt_server = "";
char *mqtt_port = "";
char *mqtt_user = "";
char *mqtt_password = "";
char *mqtt_topic = "";
bool shouldSaveConfig = false;
float tempAdjustment = 0.0;

float readTemp()
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

  return steinhart + tempAdjustment;
}

void handleRoot() {
  float tmp = readTemp();
  server.send(200, "text/plain", "Current temperature: " + String(tmp));
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

void SaveConfigCallback()
{
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void setup_wifi()
{
  //read configuration from FS json
  Serial.println("mounting FS...");

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
  clientId = String("IoT" + ESP.getChipId()).c_str();
  Serial.println("Client Id: " + String(clientId));
  WiFiManagerParameter custom_mqtt_server("server", "MQTT server", mqtt_server, 64);
  WiFiManagerParameter custom_mqtt_port("port", "MQTT port", mqtt_port, 6);
  WiFiManagerParameter custom_mqtt_user("user", "MQTT user", mqtt_user, 32);
  WiFiManagerParameter custom_mqtt_password("password", "MQTT password", mqtt_password, 32);
  WiFiManagerParameter custom_mqtt_topic("topic", "MQTT topic", mqtt_topic, 64);
  WiFiManagerParameter custom_temp_adjustment("temp", "Temp adjustment", String(tempAdjustment).c_str(), 6);

  wifiManager.setSaveConfigCallback(SaveConfigCallback);
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_mqtt_user);
  wifiManager.addParameter(&custom_mqtt_password);
  wifiManager.addParameter(&custom_mqtt_topic);
  wifiManager.addParameter(&custom_temp_adjustment);

  wifiManager.autoConnect(clientId);

  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(mqtt_user, custom_mqtt_user.getValue());
  strcpy(mqtt_password, custom_mqtt_password.getValue());
  strcpy(mqtt_topic, custom_mqtt_topic.getValue());
  tempAdjustment = String(custom_mqtt_topic.getValue()).toFloat();

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
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject &json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
    json["mqtt_user"] = mqtt_user;
    json["mqtt_password"] = mqtt_password;
    json["mqtt_topic"] = mqtt_topic;

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
    if (client.connect(clientId, mqtt_user, mqtt_password))
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

bool checkBound(float newValue, float prevValue, float maxDiff)
{
  return !isnan(newValue) &&
         (newValue < prevValue - maxDiff || newValue > prevValue + maxDiff);
}

void setup()
{
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, (int)mqtt_port);

  server.on("/", handleRoot);

  server.on("/reset", handleReset);

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");
}

long lastMsg = 0;
float temp = 0.0;
float hum = 0.0;
float diff = 0.1;
float newTemp = 0.0;
int tempCount = 0;

void publishTemperature()
{
  newTemp = newTemp + readTemp();
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
      String tempData = "temperature,location=bogenhuset,room=livingroom temperature=" + String(temp);
      client.publish(mqtt_topic, tempData.c_str(), true);
    }
    newTemp = 0;
    tempCount = 0;
  }
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
