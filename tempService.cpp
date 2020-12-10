/*
   TempSensor: gestione lettura della temperatura da sensore MCP9808
   by Fabrizio Allevi
   fabrizio . allevi @ tiscali . it
*/

#include <Adafruit_MCP9808.h>
#include "main.h"

Adafruit_MCP9808 tempSensor = Adafruit_MCP9808();

void initTempService() {
  if (!tempSensor.begin(0x18) && DEBUG)
    Serial.println("Couldn't find MCP9808! Check your connections and verify the address is correct.");
  tempSensor.setResolution(3); // sets the resolution 0-0.5°C-30ms | 1-0.25°C-65ms | 2-0.125°C-130ms | 3-0.0625°C-250ms
}


float readTemp() {
  tempSensor.wake();   // wake up, ready to read!
  float ntr=tempSensor.readTempC();
  float ofs=(float)sto.forceData.ofsTemp;
  ofs/=10.0f;
  float nto=ntr+ofs;
  if (DEBUG) {
    Serial.print("TempRead: ");
    Serial.print(ntr, 4);
    Serial.print("°C - TempOfs: ");
    Serial.print(nto, 4);
    Serial.println("°C");
  }
  tempSensor.shutdown_wake(1); // shutdown MSP9808 - power consumption ~0.1 mikro Ampere, stops temperature sampling
  return nto;
}
