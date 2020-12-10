/*
   gestione Encoder rotativo
   by Fabrizio Allevi
   fabrizio . allevi @ tiscali . it
*/

#include <Encoder.h>
#include "main.h"
#define PIN_SWITCH 7

Encoder knob(5, 6);
void switchState();
long oldPosition;

void initKnob() {
  pinMode(PIN_SWITCH, INPUT_PULLUP); // switch
  attachInterrupt(digitalPinToInterrupt(PIN_SWITCH), switchState, FALLING);
  oldPosition=knob.read();
}

long getPos() {
  long newPosition=knob.read();
  if(newPosition!=oldPosition) {
    if(DEBUG) {
        Serial.println(newPosition);
    }
    oldPosition=newPosition;
  }
  return newPosition;
}

int getDelta() {
  long newPosition=knob.read();
  int delta=newPosition-oldPosition;
  oldPosition=newPosition;
  if(delta!=0 && DEBUG) {
      Serial.println(delta);
  }
  return delta;
}

int getDir() {
  int dir=0;
  long newPosition=knob.read();
  if(oldPosition>newPosition)
    dir=-1;
  else if(oldPosition<newPosition)
    dir=1;
  oldPosition=newPosition;
  return dir;
}

long lLast=millis();
bool bOn=false;
bool getSwitch() {
  if(bOn) {
    bOn=false;
    if(DEBUG)
        Serial.println("switch ON!");
    return true;
  }
  return false;
}

void switchState() {
  long lNow=millis();
  if(!bOn && lNow>lLast+1000)
    bOn=true, lLast=lNow;
}
