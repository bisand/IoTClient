#include "IoTClient.h"

bool IoTClient::_save_config = false;

// Constructor
IoTClient::IoTClient(IoTConfig &config, float (*readEvent)(void))
{
  isDebug = false;
  iotConfig = config;
  readEventInternal = readEvent;
  IoTClient::_save_config = false;
  _espClient = WiFiClient();
  _client = PubSubClient(_espClient);
  _server = new ESP8266WebServer(80);
}

// Constructor
IoTClient::IoTClient(float (*readEvent)(void))
{
  isDebug = false;
  init();
  readEventInternal = readEvent;
  IoTClient::_save_config = false;
  _espClient = WiFiClient();
  _client = PubSubClient(_espClient);
  _server = new ESP8266WebServer(80);
}

// Destructor
IoTClient::~IoTClient()
{
  _client.disconnect();
  delete _server;
}

void IoTClient::init()
{
  iotConfig.mqtt_server = "";
  iotConfig.mqtt_port = "1883";
  iotConfig.mqtt_user = "";
  iotConfig.mqtt_password = "";
  iotConfig.mqtt_topic = "home/livingroom/temperature";
  iotConfig.event_location = "home";
  iotConfig.event_place = "livingroom";
  iotConfig.event_type = "temperature";
  iotConfig.event_adjustment = 0.0;
  iotConfig.event_publish_interval = 5000;
}

// Html index page
void IoTClient::sendIndexPage()
{
  float event = readEventInternal();
  _server->send(200, "text/html", "<html><body style=\"font-family:Tahoma,Geneva,sans-serif;\"><h1>"+clientId+"</h1><span>" + iotConfig.event_type + ": " + String(event) + "</span><form action=\"/\" method=\"POST\">Adjustment: <input type=\"input\" name=\"event_adjustment\" value=\"" + iotConfig.event_adjustment + "\"/><br/><input type=\"submit\" value=\"Save\"/></form></body></html>");
}

// Handle GET request for root
void IoTClient::handleRootGET()
{
  sendIndexPage();
}

// Handle POST request for root
void IoTClient::handleRootPOST()
{
  if (!_server->hasArg("event_adjustment") || _server->arg("event_adjustment") == NULL)
  {
    _server->send(400, "text/plain", "400: Invalid Request");
    return;
  }
  iotConfig.event_adjustment = _server->arg("event_adjustment").toFloat();
  writeConfigFile();
  sendIndexPage();
}

// Handle reset request
void IoTClient::handleReset()
{
  if(isDebug) Serial.println("Reset WiFi settings.");
  _server->send(200, "text/html", "<html><body style=\"font-family:Tahoma,Geneva,sans-serif;\"><span>The IoT device will now reset. Please connect to internal WiFi hotspot to reconfigure.</span></body></html>");
  ESP.eraseConfig();
  delay(3000);
  ESP.reset();
  delay(1000);
}

// Handle 404 page
void IoTClient::handleNotFound()
{
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += _server->uri();
  message += "\nMethod: ";
  message += (_server->method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += _server->args();
  message += "\n";
  for (uint8_t i = 0; i < _server->args(); i++)
  {
    message += " " + _server->argName(i) + ": " + _server->arg(i) + "\n";
  }
  _server->send(404, "text/plain", message);
}

// Callback to set the save config flag.
void IoTClient::saveConfigCallback()
{
  _save_config = true;
}

// Read config file.
void IoTClient::readConfigFile()
{
  if (SPIFFS.begin())
  {
    if(isDebug) Serial.println("mounted file system");
    if (SPIFFS.exists("/cfg.json"))
    {
      //file exists, reading and loading
      if(isDebug) Serial.println("reading config file");
      File configFile = SPIFFS.open("/cfg.json", "r");
      if (configFile)
      {
        if(isDebug) Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject &json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success())
        {
          if(isDebug) Serial.println("\nparsed json");

          iotConfig.mqtt_server = json.get<String>("mqtt_server");
          iotConfig.mqtt_port = json.get<String>("mqtt_port");
          iotConfig.mqtt_user = json.get<String>("mqtt_user");
          iotConfig.mqtt_password = json.get<String>("mqtt_password");
          iotConfig.mqtt_topic = json.get<String>("mqtt_topic");
          iotConfig.event_adjustment = json.get<float>("event_adjustment");
          iotConfig.event_type = json.get<String>("event_type");
          iotConfig.event_location = json.get<String>("event_location");
          iotConfig.event_place = json.get<String>("event_place");
        }
        else
        {
          if(isDebug) Serial.println("failed to load json config");
        }
        configFile.close();
      }
    }
  }
  else
  {
    if(isDebug) Serial.println("failed to mount FS");
  }
  //end read
}

// Save config file.
void IoTClient::writeConfigFile()
{
  if(isDebug) Serial.println("saving config");
  DynamicJsonBuffer jsonBuffer;
  JsonObject &json = jsonBuffer.createObject();
  json["mqtt_server"] = iotConfig.mqtt_server;
  json["mqtt_port"] = iotConfig.mqtt_port;
  json["mqtt_user"] = iotConfig.mqtt_user;
  json["mqtt_password"] = iotConfig.mqtt_password;
  json["mqtt_topic"] = iotConfig.mqtt_topic;
  json["event_location"] = iotConfig.event_location;
  json["event_place"] = iotConfig.event_place;
  json["event_type"] = iotConfig.event_type;
  json["event_adjustment"] = iotConfig.event_adjustment;

  File configFile = SPIFFS.open("/cfg.json", "w");
  if (!configFile)
  {
    if(isDebug) Serial.println("failed to open config file for writing");
  }

  json.printTo(Serial);
  json.printTo(configFile);
  configFile.close();
  //end save
}

// Perform WiFi setup
void IoTClient::setup_wifi()
{
  //read configuration from FS json
  if(isDebug) Serial.println("mounting FS...");

  readConfigFile();

  String mac = WiFi.macAddress();
  mac.replace(":", "");
  clientId = "IoT_" + mac.substring(6);
  if(isDebug) Serial.println("Chip Id:   " + String(ESP.getChipId()));
  if(isDebug) Serial.println("Client Id: " + clientId);
  if(isDebug) Serial.println("MAC:       " + mac);

  WiFiManagerParameter custom_mqtt_server("server", "MQTT server", iotConfig.mqtt_server.c_str(), 64);
  WiFiManagerParameter custom_mqtt_port("port", "MQTT port", iotConfig.mqtt_port.c_str(), 6);
  WiFiManagerParameter custom_mqtt_user("user", "MQTT user", iotConfig.mqtt_user.c_str(), 32);
  WiFiManagerParameter custom_mqtt_password("password", "MQTT password", iotConfig.mqtt_password.c_str(), 32);
  WiFiManagerParameter custom_mqtt_topic("topic", "MQTT topic", iotConfig.mqtt_topic.c_str(), 128);
  WiFiManagerParameter custom_event_adjustment("adjustment", "Temp adjustment", String(iotConfig.event_adjustment).c_str(), 6);
  WiFiManagerParameter custom_event_type("eventType", "Event type", iotConfig.event_type.c_str(), 64);
  WiFiManagerParameter custom_event_location("eventLocation", "Event location", iotConfig.event_location.c_str(), 64);
  WiFiManagerParameter custom_event_place("eventPlace", "Event place", iotConfig.event_place.c_str(), 64);

  WiFiManager wifiManager;
  wifiManager.setSaveConfigCallback(saveConfigCallback);
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

  iotConfig.mqtt_server = custom_mqtt_server.getValue();
  iotConfig.mqtt_port = custom_mqtt_port.getValue();
  iotConfig.mqtt_user = custom_mqtt_user.getValue();
  iotConfig.mqtt_password = custom_mqtt_password.getValue();
  iotConfig.mqtt_topic = custom_mqtt_topic.getValue();
  iotConfig.event_type = custom_event_type.getValue();
  iotConfig.event_location = custom_event_location.getValue();
  iotConfig.event_place = custom_event_place.getValue();
  iotConfig.event_adjustment = String(custom_event_adjustment.getValue()).toFloat();

  if(isDebug) Serial.println("");
  if(isDebug) Serial.println("WiFi connected");
  if(isDebug) Serial.println("IP address: ");
  if(isDebug) Serial.println(WiFi.localIP());
  if(isDebug) Serial.print("MQTT Server: ");
  if(isDebug) Serial.println(iotConfig.mqtt_server);
  if(isDebug) Serial.print("MQTT Topic: ");
  if(isDebug) Serial.println(iotConfig.mqtt_topic);

  //save the custom parameters to FS
  if (IoTClient::_save_config)
  {
    writeConfigFile();
  }
}

// Reconnect MQTT connection
void IoTClient::reconnect()
{
  // Loop until we're reconnected
  while (!_client.connected())
  {
    if(isDebug) Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    // If you do not want to use a username and password, change next line to
    // if (_client.connect("ESP8266Client")) {
    if (_client.connect(clientId.c_str(), iotConfig.mqtt_user.c_str(), iotConfig.mqtt_password.c_str()))
    {
      if(isDebug) Serial.println("connected");
    }
    else
    {
      if(isDebug) Serial.print("failed, rc=");
      if(isDebug) Serial.print(_client.state());
      if(isDebug) Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying and handle http requests while waiting.
      for (size_t i = 0; i < 10; i++)
      {
        _server->handleClient();
        delay(500);
      }
    }
  }
}

// Check if event change is within bounds
bool IoTClient::checkBound(float newValue, float prevValue, float maxDiff)
{
  return !isnan(newValue) &&
         (newValue < prevValue - maxDiff || newValue > prevValue + maxDiff);
}

// Publish the event.
void IoTClient::publishEvent()
{
  newEventValue = newEventValue + readEventInternal();
  eventCount++;

  long now = millis();
  if (now - lastMsg > iotConfig.event_publish_interval)
  {
    lastMsg = now;

    newEventValue = newEventValue / (float)eventCount;
    if (checkBound(newEventValue, eventValue, eventDiff))
    {
      eventValue = newEventValue;
      if(isDebug) Serial.print("Publishing event: " + iotConfig.event_type + " -> ");
      if(isDebug) Serial.println(String(eventValue).c_str());
      String tempData = iotConfig.event_type + ",location=" + iotConfig.event_location + ",place=" + iotConfig.event_place + " " + iotConfig.event_type + "=" + String(eventValue);
      _client.publish(iotConfig.mqtt_topic.c_str(), tempData.c_str(), true);
    }
    newEventValue = 0;
    eventCount = 0;
  }
}

// Setup method to be called from containing Setup method
void IoTClient::setup()
{
  if(isDebug) Serial.begin(115200);
  setup_wifi();
  _client.setServer(iotConfig.mqtt_server.c_str(), iotConfig.mqtt_port.toInt());

  _server->on("/", HTTP_GET, std::bind(&IoTClient::handleRootGET, this));
  _server->on("/", HTTP_POST, std::bind(&IoTClient::handleRootPOST, this));
  _server->on("/reset", HTTP_GET, std::bind(&IoTClient::handleReset, this));
  _server->onNotFound(std::bind(&IoTClient::handleNotFound, this));

  _server->begin();
  if(isDebug) Serial.println("HTTP server started");
}

// Loop method to be called from containing Loop method
void IoTClient::loop()
{
  _server->handleClient();

  if (!_client.connected())
  {
    reconnect();
  }
  _client.loop();

  publishEvent();
}
