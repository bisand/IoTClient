#include <alttemp.h>

alttemp::alttemp(/* args */)
{
}

alttemp::~alttemp()
{
}

void alttemp::setup() {
  beta=(log(RT1/RT2))/((1/T1)-(1/T2));
  Rinf=R0*exp(-beta/T0);
}

void alttemp::loop()
{
  Vout=Vin*((float)(analogRead(0))/1024.0); // calc for ntc
  Rout=(Rt*Vout/(Vin-Vout));

  TempK=(beta/log(Rout/Rinf)); // calc for temperature
  TempC=TempK-273.15;

  Serial.println("Alt. Temp: " + String(TempC));
}

