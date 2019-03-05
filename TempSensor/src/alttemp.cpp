#include <alttemp.h>

AltTemp::AltTemp(/* args */)
{
}

AltTemp::~AltTemp()
{
}

void AltTemp::setup()
{
  _tempData.beta = (log(_tempData.RT1 / _tempData.RT2)) / ((1 / _tempData.T1) - (1 / _tempData.T2));
  _tempData.Rinf = _tempData.R0 * exp(-_tempData.beta / _tempData.T0);
}

float AltTemp::getTemperature(){
  // Taken from:
  // https://www.instructables.com/id/NTC-Temperature-Sensor-With-Arduino/
  
  _tempData.Vout = _tempData.Vin * ((float)(analogRead(0)) / 1024.0); // calc for ntc
  _tempData.Rout = (_tempData.Rt * _tempData.Vout / (_tempData.Vin - _tempData.Vout));

  _tempData.TempK = (_tempData.beta / log(_tempData.Rout / _tempData.Rinf)); // calc for temperature
  _tempData.TempC = _tempData.TempK - 273.15;

  return _tempData.TempC;
}
