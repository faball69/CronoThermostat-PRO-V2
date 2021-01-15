/*
   scheduler per programma CT2_FA
   by Fabrizio Allevi
   fabrizio . allevi @ tiscali . it
*/
#include <Wire.h>
#include "main.h"

bool bFire = false;
void scheduler() {
  if(okTime) {
    int hh = hour();
    int minuteNow = hh*60+minute();
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
      int i1, i2;
      findNextCheckPoint(idProg, i1, i2);
      float setPtemp = sto.progs[idProg].T[i1]; // setpoint di temperatura da tenere in questa sequenza
      setPtemp /= 10.0;
      if(setPtemp>fmt)
        setPtemp=fmt;
      if ((!bFire && fLastTemp < setPtemp) || (bFire && fLastTemp < (setPtemp + fht)))
        bFire = true;
      else
        bFire = false;
    }
  }
  else
    bFire=false;
  // caldaia
  if (bFire && !sto.forceData.bOFF) {
    digitalWrite(CALDAIA, LOW);
  }
  else {
    digitalWrite(CALDAIA, HIGH);
  }
}

void findNextCheckPoint(int prg, int &i1, int &i2) {
  i1=0; i2=1;
  int tp, tn=hour()*60+minute();
  for(i2=0; i2<MAX_SEQ; i2++) {
    tp=(sto.progs[prg].HM[i2]>>8)*60+(sto.progs[prg].HM[i2]&0xFF);
    if(tp>tn || sto.progs[prg].T[i2]==0)
      break;
  }
  if(i2==0)
    i1=MAX_SEQ-1;
  else if(i2==MAX_SEQ) {
    i1=MAX_SEQ-1;
    i2=0;
  }
  else {
    i1=i2-1;
    if(sto.progs[prg].T[i2]==0)
      i2=0;
  }
  if (DEBUG) {
    Serial.print("i1=");
    Serial.print(i1);
    Serial.print(" i2=");
    Serial.print(i2);
    Serial.print(" temp=");
    Serial.println(sto.progs[prg].T[i1]);
  }
}
