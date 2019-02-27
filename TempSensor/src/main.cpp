#include <Arduino.h>
#include <IoTClient.h>

// which analog pin to connect
#define THERMISTORPIN A0
// resistance at 25 degrees C
#define THERMISTORNOMINAL 10000
// temp. for nominal resistance (almost always 25 C)
#define TEMPERATURENOMINAL 25
// how many samples to take and average, more takes longer
// but is more 'smooth'
#define NUMSAMPLES 25
// The beta coefficient of the thermistor (usually 3000-4000)
#define BCOEFFICIENT 3950
// the value of the 'other' resistor
#define SERIESRESISTOR 10000

uint16_t samples[NUMSAMPLES];
bool isDebug = false;

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

  if(isDebug) Serial.println("Temperature reading complete: " + String(steinhart));
  return steinhart;
}

IoTClient *iotClient;

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
  config.event_publish_interval = 30000; // publish every 30 second.

  iotClient = new IoTClient(config, readEvent);
  iotClient->isDebug = isDebug;
  iotClient->setup();
}

void loop()
{
  iotClient->loop();
  delay(1000);
}
