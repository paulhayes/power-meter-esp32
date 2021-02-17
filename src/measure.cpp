#include "measure.h"
#include <Wire.h>
#include <Arduino.h>
#include <math.h>
#include <EEPROM.h>
#define ads1110 0x48

const int offsetAddress = 10;
double ampsPerVolt = 7.706108759;
double voltsOffset = -0.006816517411;
double volts = 240;
const int numReadings = 10;
double lastAvgReading;

void setupADC()
{
  Wire.begin();
  EEPROM.begin(512);
  if( EEPROM.readByte(offsetAddress) != 0xFF ){
    voltsOffset = EEPROM.readDouble(offsetAddress);
  };

}

double readPower(){
  double voltage = readADCAvg();
  double amps = (voltage+voltsOffset) * ampsPerVolt;
  double power = max(0.,amps*volts);
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

double readADCAvg(){
  
  double readingsTotal = 0;
  for(int i=0;i<numReadings;i++){
    readingsTotal += readADC();
  }
  lastAvgReading = readingsTotal/numReadings;
  return lastAvgReading;
}


double readADC()
{
  byte lowbyte=0, highbyte=0, configRegister=0;
  long data;
  double voltage;
  
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
  return voltage;
}

void calibratePowerOffset(){
  voltsOffset = -lastAvgReading;
  EEPROM.writeDouble(offsetAddress,voltsOffset);
  EEPROM.commit();
}

