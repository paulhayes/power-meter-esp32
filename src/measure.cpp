#include "measure.h"
#include <Wire.h>
#include <Arduino.h>


#define ads1110 0x48
const double voltsToAmps = 7.706108759;
const double voltsToAmpsOffset = -0.006816517411;

void setupADC()
{
  Wire.begin();
}

double amps = 0;
double readADC()
{
  byte lowbyte=0, highbyte=0, configRegister=0;
  long data;
  double voltage;
  double power;
  
  Wire.requestFrom(ads1110, 3);
  while(Wire.available()) // ensure all the data comes in
  {
    highbyte = Wire.read(); // high byte * B11111111
    lowbyte = Wire.read(); // low byte
    configRegister = Wire.read();
  }

 data = lowbyte | (highbyte << 8) ;
 voltage = data * 2.048 ;
 voltage = voltage / 32768.0;
 amps += 0.1*((voltage * voltsToAmps + voltsToAmpsOffset) - amps);
 power = amps*240;
 # ifdef DEBUG
 Serial.print("Data=");
 Serial.print(data, DEC);
 Serial.print(", Voltage=");

 Serial.print(voltage, DEC);
 Serial.print("v");
 Serial.print(", current=");
 Serial.print(amps);
 Serial.print("A, power=");
 Serial.print(power);
 Serial.println("W");
 #endif
 return power;
}