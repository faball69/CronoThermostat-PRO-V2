/*
 * main include file
 * by Fabrizio Allevi
 * fabrizio . allevi @ tiscali . it
 */

// defines
//#define DEBUG true
#define DEBUG false

#define CALDAIA 4 // digital pin out 4
#define LEVEL_ZERO 0
#define LEVEL_PROGRAMS 1
#define LEVEL_DAYS 2
#define LEVEL_DATETIME 3
#define LEVEL_OFSTEMP 4
#define LEVEL_FORCE 5
#define LEVEL_OFF 6
#define MAX_MENU 7
#define MAX_PROGS 4
#define MAX_DAYS 7
#define MAX_SEQ 6

#include <Arduino.h>
#include <TimeLib.h>
#include <WiFiNINA.h>


// wifi + store
void initWifiService();
void runWifiService();
void saveDataFlash();
bool sendMail();
bool updateNtpTime();
bool tryConnection(long msNow);
bool checkWifiStatus();
void initWeb();
void resetWifi(int level);  // 0=reset 1=reset+cnt+tmt 2=stopWifi
// temp
void initTempService();
float readTemp();
// knob
void initKnob();
long getPos();
int getDelta();
int getDir();
bool getSwitch();
// oled
void printOled(const char *text, int posx, int posy, int size, bool bShow);
void setCursorOled(int row, int col);
void initOled();
void clearOled();
void blinkOled(bool bOn);
void scrollOled(bool bOn);
void lowDimmer();
void normalDimmer();
void changeDimmer();
void setDimmer(int d);
void fireBitmap(int posx, int posy, bool b);
void netBitmap(int posx, int posy, bool b);
// scheduler
void scheduler();

// globals temp in °C/10
struct stForce {
  int hForce;
  unsigned long tFin;
  int ofsTemp;
  int maxTemp;
  int hysteresisTemp;   // in °C/100
  bool bOFF;
};
struct stProgram {
  int T[MAX_SEQ];  // in decimi di grado
  int HM[MAX_SEQ]; // hhmm nei 16bit hh=partealta mm=partebassa
};
struct stStorage {
  bool bValue;
  stProgram progs[MAX_PROGS];
  int weekProg[MAX_DAYS];
  stForce forceData;
};
extern stStorage sto;
extern bool bFire;
extern String ipAddr;
extern String tSSID;
extern long RSSI;
extern float fLastTemp;
extern short ld[1440];
extern int wifiStatus;
