#pragma once
#include <stdint.h>
double readPower(int readings);
//double readADCAvg();
double readCurrent(int readings);
void setupADC();
void calibratePowerOffset();
int16_t readSample();
double computeCurrent(int16_t val);
double computeVolts(int16_t val);
bool requestReading();
bool readingReady();
int16_t rawReading();
