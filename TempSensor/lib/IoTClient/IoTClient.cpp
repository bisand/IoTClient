#include "IoTClient.h"

bool IoTClient::_save_config = false;

// Constructor
IoTClient::IoTClient(IoTConfig &config, float (*readEvent)(void))
{
  _iotConfig = config;
  readEventInternal = readEvent;
  IoTClient::_save_config = false;
  _espClient = WiFiClient();
  _client = PubSubClient(_espClient);
  _server = new ESP8266WebServer(80);
}

// Constructor
IoTClient::IoTClient(float (*readEvent)(void))
{
  setDefaultConfig();
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

void IoTClient::setDefaultConfig()
{
  _iotConfig.mqtt_server = "";
  _iotConfig.mqtt_port = "1883";
  _iotConfig.mqtt_user = "";
  _iotConfig.mqtt_password = "";
  _iotConfig.mqtt_topic = "home/livingroom/temperature";
  _iotConfig.event_location = "home";
  _iotConfig.event_place = "livingroom";
  _iotConfig.event_type = "temperature";
  _iotConfig.event_adjustment = 0.0;
}

void IoTClient::sendIndexPage()
{
  float event = readEventInternal();
  _server->send(200, "text/html", "<html><body style=\"font-family:Tahoma,Geneva,sans-serif;\"><span>" + _iotConfig.event_type + ": " + String(event) + "</span><form action=\"/\" method=\"POST\">Adjustment: <input type=\"input\" name=\"event_adjustment\" value=\"" + _iotConfig.event_adjustment + "\"/><br/><input type=\"submit\" value=\"Save\"/></form></body></html>");
}

void IoTClient::handleRootGET()
{
  sendIndexPage();
}

void IoTClient::handleRootPOST()
{
  if (!_server->hasArg("event_adjustment") || _server->arg("event_adjustment") == NULL)
  {
    _server->send(400, "text/plain", "400: Invalid Request");
    return;
  }
  _iotConfig.event_adjustment = _server->arg("event_adjustment").toFloat();
  writeConfigFile();
  sendIndexPage();
}

void IoTClient::handleReset()
{
  Serial.println("Reset WiFi settings.");
  _server->send(200, "text/html", "<html><body style=\"font-family:Tahoma,Geneva,sans-serif;\"><span>The IoT device will now reset. Please connect to internal WiFi hotspot to reconfigure.</span></body></html>");
  ESP.eraseConfig();
  delay(3000);
  ESP.reset();
  delay(1000);
}

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

void IoTClient::saveConfigCallback()
{
  Serial.println("Should save config");
  _save_config = true;
}

void IoTClient::readConfigFile()
{
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

          _iotConfig.mqtt_server = json.get<String>("mqtt_server");
          _iotConfig.mqtt_port = json.get<String>("mqtt_port");
          _iotConfig.mqtt_user = json.get<String>("mqtt_user");
          _iotConfig.mqtt_password = json.get<String>("mqtt_password");
          _iotConfig.mqtt_topic = json.get<String>("mqtt_topic");
          _iotConfig.event_adjustment = json.get<float>("event_adjustment");
          _iotConfig.event_type = json.get<String>("event_type");
          _iotConfig.event_location = json.get<String>("event_location");
          _iotConfig.event_place = json.get<String>("event_place");
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

void IoTClient::writeConfigFile()
{
  Serial.println("saving config");
  DynamicJsonBuffer jsonBuffer;
  JsonObject &json = jsonBuffer.createObject();
  json["mqtt_server"] = _iotConfig.mqtt_server;
  json["mqtt_port"] = _iotConfig.mqtt_port;
  json["mqtt_user"] = _iotConfig.mqtt_user;
  json["mqtt_password"] = _iotConfig.mqtt_password;
  json["mqtt_topic"] = _iotConfig.mqtt_topic;
  json["event_location"] = _iotConfig.event_location;
  json["event_place"] = _iotConfig.event_place;
  json["event_type"] = _iotConfig.event_type;
  json["event_adjustment"] = _iotConfig.event_adjustment;

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

void IoTClient::setup_wifi()
{
  //read configuration from FS json
  Serial.println("mounting FS...");

  readConfigFile();

  String mac = WiFi.macAddress();
  mac.replace(":", "");
  clientId = "IoT_" + mac.substring(6);
  Serial.println("Chip Id:   " + String(ESP.getChipId()));
  Serial.println("Client Id: " + clientId);
  Serial.println("MAC:       " + mac);

  WiFiManagerParameter custom_mqtt_server("server", "MQTT server", _iotConfig.mqtt_server.c_str(), 64);
  WiFiManagerParameter custom_mqtt_port("port", "MQTT port", _iotConfig.mqtt_port.c_str(), 6);
  WiFiManagerParameter custom_mqtt_user("user", "MQTT user", _iotConfig.mqtt_user.c_str(), 32);
  WiFiManagerParameter custom_mqtt_password("password", "MQTT password", _iotConfig.mqtt_password.c_str(), 32);
  WiFiManagerParameter custom_mqtt_topic("topic", "MQTT topic", _iotConfig.mqtt_topic.c_str(), 128);
  WiFiManagerParameter custom_event_adjustment("adjustment", "Temp adjustment", String(_iotConfig.event_adjustment).c_str(), 6);
  WiFiManagerParameter custom_event_type("eventType", "Event type", _iotConfig.event_type.c_str(), 64);
  WiFiManagerParameter custom_event_location("eventLocation", "Event location", _iotConfig.event_location.c_str(), 64);
  WiFiManagerParameter custom_event_place("eventPlace", "Event place", _iotConfig.event_place.c_str(), 64);

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

  _iotConfig.mqtt_server = custom_mqtt_server.getValue();
  _iotConfig.mqtt_port = custom_mqtt_port.getValue();
  _iotConfig.mqtt_user = custom_mqtt_user.getValue();
  _iotConfig.mqtt_password = custom_mqtt_password.getValue();
  _iotConfig.mqtt_topic = custom_mqtt_topic.getValue();
  _iotConfig.event_type = custom_event_type.getValue();
  _iotConfig.event_location = custom_event_location.getValue();
  _iotConfig.event_place = custom_event_place.getValue();
  _iotConfig.event_adjustment = String(custom_event_adjustment.getValue()).toFloat();

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("MQTT Server: ");
  Serial.println(_iotConfig.mqtt_server);
  Serial.print("MQTT Topic: ");
  Serial.println(_iotConfig.mqtt_topic);

  //save the custom parameters to FS
  if (IoTClient::_save_config)
  {
    writeConfigFile();
  }
}

void IoTClient::reconnect()
{
  int reconnectCount = 0;
  // Loop until we're reconnected
  while (!_client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    // If you do not want to use a username and password, change next line to
    // if (_client.connect("ESP8266Client")) {
    if (_client.connect(clientId.c_str(), _iotConfig.mqtt_user.c_str(), _iotConfig.mqtt_password.c_str()))
    {
      Serial.println("connected");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(_client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      for (size_t i = 0; i < 10; i++)
      {
        _server->handleClient();
        delay(500);
      }

      reconnectCount++;
      if (reconnectCount > 10)
      {
      }
    }
  }
}

bool IoTClient::checkBound(float newValue, float prevValue, float maxDiff)
{
  return !isnan(newValue) &&
         (newValue < prevValue - maxDiff || newValue > prevValue + maxDiff);
}

void IoTClient::publishEvent()
{
  newEventValue = newEventValue + readEventInternal();
  eventCount++;

  long now = millis();
  if (now - lastMsg > 5000)
  {
    lastMsg = now;

    newEventValue = newEventValue / (float)eventCount;
    if (checkBound(newEventValue, eventValue, eventDiff))
    {
      eventValue = newEventValue;
      Serial.print("Publishing event: " + _iotConfig.event_type + " -> ");
      Serial.println(String(eventValue).c_str());
      String tempData = _iotConfig.event_type + ",location=" + _iotConfig.event_location + ",place=" + _iotConfig.event_place + " " + _iotConfig.event_type + "=" + String(eventValue);
      _client.publish(_iotConfig.mqtt_topic.c_str(), tempData.c_str(), true);
    }
    newEventValue = 0;
    eventCount = 0;
  }
}

void IoTClient::setup()
{
  Serial.begin(115200);
  setup_wifi();
  _client.setServer(_iotConfig.mqtt_server.c_str(), _iotConfig.mqtt_port.toInt());

  _server->on("/", HTTP_GET, std::bind(&IoTClient::handleRootGET, this));
  _server->on("/", HTTP_POST, std::bind(&IoTClient::handleRootPOST, this));
  _server->on("/reset", HTTP_GET, std::bind(&IoTClient::handleReset, this));
  _server->onNotFound(std::bind(&IoTClient::handleNotFound, this));

  _server->begin();
  Serial.println("HTTP server started");
}

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
