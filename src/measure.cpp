#include "measure.h"
#include <Wire.h>
#include <Arduino.h>
#include <math.h>
#include <EEPROM.h>
#include <Adafruit_ADS1X15.h>

#define ads1110 0x48

//Adafruit_ADS1115 adc;

const int offsetAddress = 10;
//double ampsPerVolt = 7.706108759;
double ampsPerVolt = 26.677;
double voltsOffset = -0.006816517411;
double volts = 240;
const int numReadings = 10;
double lastAvgReading;
const uint16_t config = RATE_ADS1115_860SPS|GAIN_FOUR|ADS1X15_REG_CONFIG_CQUE_NONE|ADS1X15_REG_CONFIG_MODE_SINGLE;
void setupADC()
{
  //Wire.setClock(3400000L);
  Wire.setClock(400000L);
  /*
  adc.setGain(GAIN_FOUR);
  adc.begin(0x48,&Wire);
  adc.setDataRate(RATE_ADS1115_860SPS);
  */
  Wire.begin();
  Wire.beginTransmission(ads1110);
  Wire.write(ADS1X15_REG_POINTER_CONFIG);
  
  Wire.write((config>>8) & 0xff);
  Wire.write(config & 0xff);
  Wire.endTransmission();
  
  //Wire.begin();
  /*
  EEPROM.begin(512);
  if( EEPROM.readByte(offsetAddress) != 0xFF ){
    voltsOffset = EEPROM.readDouble(offsetAddress);
  };
  */

}

double readPower(int readings){
  //double voltage = readADCAvg();
  double power = max(0.,readCurrent(readings)*volts);
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

/*
double readADCAvg(){
  
  double readingsTotal = 0;
  for(int i=0;i<numReadings;i++){
    readingsTotal += readCurrent();
  }
  lastAvgReading = readingsTotal/numReadings;
  return lastAvgReading;
}
*/

/*int16_t readSample()
{
  return adc.readADC_Differential_0_1();
}*/


int16_t readSample()
{
  return rawReading();
}

double readCurrent(int readings)
{
  //long capturePeriodEndTime = millis()+200;
  int count = 0;
  double sum = 0;
  long timeout = millis()+250L;
  while(readings>0){ //millis()<capturePeriodEndTime
    Serial.print(".");
    while(!requestReading()){
      if(millis()>timeout){
        return -1;
      }
      yield();
    }
    while(!readingReady()){
      if(millis()>timeout){
        return -1;
      }
      yield();
    }
    int16_t val = readSample();
    double current = computeCurrent(val);
    sum+=current*current;
    count++;
    readings--;
  }
  Serial.println("");
  double current = sqrt(sum/count); 
  return current;
}

double computeCurrent(int16_t val)
{
  return ampsPerVolt * computeVolts( val );
}

double computeVolts(int16_t val)
{
  return val*1.024/32768;
}

bool requestReading()
{
  Wire.beginTransmission(ads1110);
  Wire.write(ADS1X15_REG_POINTER_CONFIG);
  int _config = ADS1X15_REG_CONFIG_OS_SINGLE | config;
  Wire.write((_config>>8) & 0xff);
  Wire.write(_config & 0xff);
  int error=Wire.endTransmission();
  return error==0;
}

int16_t rawReading()
{
  byte lowbyte=0, highbyte=0, configRegister=0;
  int16_t data;
  
  Wire.beginTransmission(ads1110);
  Wire.write(0);
  int error=Wire.endTransmission();
  if(error){
    return -1;
  }
  Wire.requestFrom(ads1110, 2);
  //uint8_t sendData[] = {ads1110,0};
  //Wire.write((uint8_t*)(&sendData),2);
  
  highbyte = Wire.read(); // high byte * B11111111
  lowbyte = Wire.read(); // low byte
  //configRegister = Wire.read();
  // Serial.print(responseLength);
  // Serial.print(" ");
  // Serial.print(highbyte,16);
  // Serial.print(" ");
  // Serial.println(lowbyte,16);
  
  // ensure all the data comes in
  
  data = lowbyte | (highbyte << 8) ;
  
  // if 0xFFFF then a reading went wrong I think
  return data;
}

bool readingReady()
{
  Wire.beginTransmission(ads1110);
  Wire.write(ADS1X15_REG_POINTER_CONFIG);
  int error=Wire.endTransmission();

  Wire.requestFrom(ads1110, 2);
  if(Wire.available()==2){
    byte highbyte = Wire.read();
    byte lowbyte = Wire.read();
    int16_t config = lowbyte | (highbyte << 8) ;
    return ( config & ADS1X15_REG_CONFIG_OS_NOTBUSY )!=0;
  }
  return false;
}

void calibratePowerOffset(){
  voltsOffset = -lastAvgReading;
  EEPROM.writeDouble(offsetAddress,voltsOffset);
  EEPROM.commit();
}

