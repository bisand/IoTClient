
#include <FS.h>
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DNSServer.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <ESP8266WebServer.h>

struct IoTConfig
{
public:
  String mqtt_server;
  String mqtt_port;
  String mqtt_user;
  String mqtt_password;
  String mqtt_topic;
  String event_location;
  String event_place;
  String event_type;
  float event_adjustment;
};

class IoTClient
{
public:
  IoTClient(IoTConfig &config, float (*readEvent)(void));
  IoTClient(float (*readEvent)(void));
  ~IoTClient();

  void setup();
  void loop();

private:
  static bool _save_config;

  WiFiClient _espClient;
  PubSubClient _client;
  ESP8266WebServer *_server;

  String clientId;
  IoTConfig _iotConfig;

  long lastMsg = 0;
  float eventValue = 0.0;
  float newEventValue = 0.0;
  float eventDiff = 0.1;
  int eventCount = 0;

  static void saveConfigCallback();
  float (*readEventInternal)();

  void setDefaultConfig();
  void sendIndexPage();
  void readConfigFile();
  void writeConfigFile();
  void handleRootGET();
  void handleRootPOST();
  void handleReset();
  void handleNotFound();
  void setup_wifi();
  void reconnect();

  bool checkBound(float newValue, float prevValue, float maxDiff);
  void publishEvent();
};