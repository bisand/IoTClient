#include <alttemp.h>

AltTemp::AltTemp(/* args */)
{
}

AltTemp::~AltTemp()
{
}

void AltTemp::setup()
{
  _tempData1.beta = (log(_tempData1.RT1 / _tempData1.RT2)) / ((1 / _tempData1.T1) - (1 / _tempData1.T2));
  _tempData1.Rinf = _tempData1.R0 * exp(-_tempData1.beta / _tempData1.T0);
}

float AltTemp::getTemperature(){
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

  return steinhart;
}

float AltTemp::getTemperature1()
{
  // Taken from:
  // https://www.instructables.com/id/NTC-Temperature-Sensor-With-Arduino/
  
  _tempData1.Vout = _tempData1.Vin * ((float)(analogRead(0)) / 1024.0); // calc for ntc
  _tempData1.Rout = (_tempData1.Rt * _tempData1.Vout / (_tempData1.Vin - _tempData1.Vout));

  _tempData1.TempK = (_tempData1.beta / log(_tempData1.Rout / _tempData1.Rinf)); // calc for temperature
  _tempData1.TempC = _tempData1.TempK - 273.15;

  return _tempData1.TempC;
}
