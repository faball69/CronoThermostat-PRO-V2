/*
   scheduler per programma CT2_FA
   by Fabrizio Allevi
   fabrizio . allevi @ tiscali . it
*/
#include <Wire.h>
#include "main.h"

bool bFire = false;
void scheduler() {
  int hh = hour();
  int minuteNow = hh * 60 + minute();
  int idProg = sto.weekProg[weekday()-1];
  if (DEBUG) {
    Serial.print("minuteNow=");
    Serial.print(minuteNow);
    Serial.print(" idProg=");
    Serial.println(idProg);
  }
  float fmt=((float)(sto.forceData.maxTemp))/10.0f;
  float fht=((float)(sto.forceData.hysteresisTemp))/100.0f;
  // controllo force functionality
  if (sto.forceData.hForce) {
    if (sto.forceData.tFin > now()) {
      if (sto.forceData.hForce>0 && hh>7 && hh<23 && ((fLastTemp<fmt && !bFire) || (fLastTemp<fmt+fht && bFire))) // forza ma non di notte e se supero maxtemp
        bFire = true;
      else
        bFire = false;
      if (DEBUG) {
        Serial.print("forceFor=");
        Serial.print(sto.forceData.hForce);
        Serial.println(" hour");
      }
    }
    else {
      sto.forceData.hForce = 0;
      sto.forceData.tFin = 0;
      //saveData((byte*)&forceData, sizeof(forceData), EEPROM_OFSF);
    }
  }
  else {
    // check sequences
    for (int seq = 0; seq < MAX_SEQ - 1; seq++) {
      int minI = (sto.progs[idProg].HM[seq] >> 8) * 60 + (sto.progs[idProg].HM[seq] & 0xFF);
      int minF = (sto.progs[idProg].HM[seq + 1] >> 8) * 60 + (sto.progs[idProg].HM[seq + 1] & 0xFF);
      if (minI <= minuteNow && (minuteNow < minF || minF == 0)) { // trovata la seq in corso
        float setPtemp = sto.progs[idProg].T[seq]; // setpoint di temperatura da tenere in questa sequenza
        setPtemp /= 10.0;
        if (DEBUG) {
          Serial.print("seq=");
          Serial.print(seq);
          Serial.print(" minI=");
          Serial.print(minI);
          Serial.print(" minF=");
          Serial.print(minF);
          Serial.print(" temp=");
          Serial.println(setPtemp);
        }
        if ((!bFire && fLastTemp < setPtemp) || (bFire && fLastTemp < (setPtemp + fht)))
          bFire = true;
        else
          bFire = false;
        break;
      }
    }
  }
  // caldaia
  if (bFire && !sto.forceData.bOFF) {
    digitalWrite(CALDAIA, LOW);
  }
  else {
    digitalWrite(CALDAIA, HIGH);
  }
}
