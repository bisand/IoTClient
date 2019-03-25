#include <Arduino.h>
#include <IoTClient.h>
#include <alttemp.h>

bool isDebug = true;
IoTClient *iotClient;
AltTemp *altTemp;

float readEvent()
{
  float temp1 = altTemp->getTemperature();
  if (isDebug) Serial.println("Temperature: " + String(temp1 + iotClient->iotConfig.event_adjustment));
  return temp1 + iotClient->iotConfig.event_adjustment;
}

void setup()
{
  IoTConfig config;
  config.mqtt_server = "";
  config.mqtt_port = "1883";
  config.mqtt_user = "";
  config.mqtt_password = "";
  config.mqtt_topic = "home/livingroom/temperature";
  config.event_location = "home";
  config.event_place = "livingroom";
  config.event_type = "temperature";
  config.event_adjustment = 0.0;
  config.event_publish_interval = 60000; // publish every 60 second.

  iotClient = new IoTClient(config, readEvent);
  iotClient->isDebug = isDebug;
  iotClient->setup();

  altTemp = new AltTemp();
  altTemp->setup();
}

void loop()
{
  iotClient->loop();
  delay(1000);
}
