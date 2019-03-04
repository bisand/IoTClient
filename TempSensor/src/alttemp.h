#include <Arduino.h>

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

struct TempData1
{
    float Vin = 3.3;   // [V]
    float Rt = 10000;  // Resistor t [ohm]
    float R0 = 9630;  // value of rct in T0 [ohm]
    float T0 = 298.15; // use T0 in Kelvin [K]
    float Vout = 0.0;  // Vout in A0
    float Rout = 0.0;  // Rout in A0
    // use the datasheet to get this data.
    float T1 = 273.15; // [K] in datasheet 0º C
    float T2 = 373.15; // [K] in datasheet 100° C
    float RT1 = 26000; // [ohms]  resistence in T1
    float RT2 = 983; // [ohms]   resistence in T2
    float beta = 0.0;  // initial parameters [K]
    float Rinf = 0.0;  // initial parameters [ohm]
    float TempK = 0.0; // variable output
    float TempC = 0.0; // variable output
};

class AltTemp
{
  private:
    TempData1 _tempData1;
    uint16_t samples[NUMSAMPLES];


  public:
    AltTemp(/* args */);
    ~AltTemp();

    void setup();
    float getTemperature();
    float getTemperature1();
};
