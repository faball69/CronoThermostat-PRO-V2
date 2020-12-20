/*
   CT2_FA cronoTermostat V2
   by Fabrizio Allevi
   fabrizio . allevi @ tiscali . it
*/

#include "main.h"

// globals
int smLevel = 0;
String sDays[MAX_DAYS] = { "Dom", "Lun", "Mar", "Mer", "Gio", "Ven", "Sab" };
String sMonth[12] = { "Gen", "Feb", "Mar", "Apr", "Mag", "Giu", "Lug", "Ago", "Set", "Ott", "Nov", "Dic" };
int smValue = 0;
bool bProgram = false;
int nProg = 0;
int arrayDT[5];

int smCursor = 0;
unsigned long msIdle = millis();
unsigned long msTemp = millis();
unsigned long msScheduler = millis();
unsigned long msLast = millis();
unsigned long msDimmer = millis();
bool bIdleState = true;
bool bForce=false;
long lastOp=0;
float fLastTemp=0.0f;
short ld[1440];

void setup() {
  memset(ld, 0, sizeof(ld));
  // Open serial communications and wait for port to open:
  if (DEBUG) {
    Serial.begin(9600);
    while (!Serial) {
      delay(1000); // wait for serial port to connect. Needed for native USB port only
    }
  }
  pinMode(CALDAIA, OUTPUT);
  digitalWrite(CALDAIA, HIGH);  // spengo rele
  initKnob();
  initOled();
  initTempService();
  lowDimmer();
  settingsPage('#');
  initWifiService();
  if(DEBUG)
    Serial.println("setup terminated!");
  lastOp=millis()-10000;
}

long lastUpd=0;
int aux=0;
int page=0; //0=idle 1=force 2=ofs 3=prog 4=setting
bool bModify=false;
bool okTime=false;
void loop() {
  long nowTime=millis();
  int h=0;
  if(now()>1000) {
    okTime=true;
  }
  if(!okTime && checkConnection(nowTime)) {
    if(!okTime)
      updateNtpTime();
    else
        h=hour();
    if(h>5 && h<23)
      runWifiService();
  }
  int dir = getDir();
  bool sw = getSwitch();
  if(!bModify) {  // volta pagina
    if(dir)
      lastOp=nowTime;
    else if(nowTime>lastOp+10000)
      aux=0;
    aux+=dir;
    if(aux<0)
      aux=0;
    if(aux>=16)
      aux=16;
    page=aux/4;
    // pages handling
    if(page==0) { // idle page
      if(nowTime>lastUpd+5000) {
        fLastTemp = readTemp();
        idlePage();
        lastUpd=nowTime;
        if(okTime)
          scheduler();
      }
    }
    else if(page==1) {  // force page
      forcePage(sto.forceData.hForce);
    }
    else if(page==2) {  // settings page
      ofsPage(sto.forceData.ofsTemp);
    }
    else if(page==3) {  // program page
      programPage();
    }
    else if(page==4) {  // settings page
      settingsPage(bOnLine?'@':'#');
    }
    if(sw && (page==1 || page==2 || page==4)) {
      bModify=true;
      lastOp=nowTime;
      if(page==1)
        aux=sto.forceData.hForce*4;
      else if(page==2)
        aux=sto.forceData.ofsTemp;
      else if(page==4)
        aux=0;
    }
  }
  else {  // modifica
    if(nowTime<lastOp+10000) {
      if(page==1) {
        if(dir) {
          aux+=dir;
          lastOp=nowTime;
        }
        else {
          int hf=aux/4;
          forcePage(hf);
          if(sw && hf!=sto.forceData.hForce) {
            sto.forceData.hForce=hf;
            if(hf<0)
              hf*=-1;
            sto.forceData.tFin=now()+hf*3600;
            page=0;
            aux=0;
            bModify=false;
          }
        }
      }
      else if(page==2) {
        if(dir) {
          aux+=dir;
          lastOp=nowTime;
        }
        ofsPage(aux);
        if(sw && aux!=sto.forceData.ofsTemp) {
          sto.forceData.ofsTemp=aux;
          saveDataFlash();  // save in storage wifi /fs/ flash
          page=0;
          aux=0;
          bModify=false;
        }
      }
      else if(page==4) {
        if(dir) {
          aux+=dir;
          lastOp=nowTime;
        }
        if(aux<0)
          aux=0;
        if(aux>=4)
          aux=4;
        if(aux<4) {
          settingsPage('S');
          if(sw) {
            if(bOnLine && sendMail())
              memset(ld, 0, sizeof(ld));
            else {
              settingsPage('X');
              delay(1000);
            }
            page=0;
            aux=0;
            bModify=false;
          }
        }
        else {
          settingsPage('R');
          if(sw) {
            settingsPage('>');
            delay(1000);
            NVIC_SystemReset(); // system reset
          }
        }
      }
    }
    else {
      page=0;
      aux=0;
      bModify=false;
    }
  }
  if(okTime)
    addLog();
}

void idlePage() {
  char text[100];
  clearOled();
  drawbitmap(100, 10);
  netbitmap(116, 0);
  sprintf(text, "%.1f", fLastTemp);
  printOled(text, 20, 8, 3, true);
}

void forcePage(int hour) {
  char text[100];
  clearOled();
  sprintf(text, "Force for....\n");
  printOled(text, 0, 0, 1, false);
  sprintf(text, "%d hour!", hour);
  printOled(text, 0, 10, 2, true);
}

void ofsPage(int ofs) {
  char text[100];
  clearOled();
  sprintf(text, "Set Offset...\n");
  printOled(text, 0, 0, 1, false);
  sprintf(text, "%ddeg/10!", ofs);
  printOled(text, 0, 10, 2, true);
}

void programPage() {
  char text[100];
  clearOled();
  int wd=weekday()-1;
  sprintf(text, "%s %d %s %d %.02d:%.02d\n", sDays[wd].c_str(), day(), sMonth[month()-1].c_str(), year(), hour(), minute());
  printOled(text, 0, 0, 1, false);
  int prg=sto.weekProg[wd];
  int i1, i2=-1;
  int tp, tn=hour()*60+minute();
  do {
    i2++;
    tp=(sto.progs[prg].HM[i2]>>8)*60+(sto.progs[prg].HM[i2]&0xFF);
  } while(tp<tn && i2<6);
  i1=i2-1;
  sprintf(text, "__%.1f   %.1f__\n", ((float)(sto.progs[prg].T[i1]))/10.0f, ((float)(sto.progs[prg].T[i2]))/10.0f);
  printOled(text, 18, 14, 1, false);
  sprintf(text, "__%.02d:%.02d %.02d:%.02d__\n", sto.progs[prg].HM[i1]>>8, sto.progs[prg].HM[i1]&0xFF, sto.progs[prg].HM[i2]>>8, sto.progs[prg].HM[i2]&0xFF);
  printOled(text, 18, 24, 1, true);
}

void settingsPage(char c) {
  char text[100];
  clearOled();
  sprintf(text, "ipAddr: %s\n", ipAddr.c_str());
  printOled(text, 0, 0, 1, false);
  if(c=='R') {
    sprintf(text, "click knob for Reset!");
    printOled(text, 0, 10, 1, false);
  }
  else if(c=='>') {
    sprintf(text, "reset in progress!");
    printOled(text, 0, 10, 1, false);
  }
  else {
    sprintf(text, "SSID: %s", tSSID.c_str());
    printOled(text, 0, 10, 1, false);
    sprintf(text, "RSSI: %ddBm", RSSI);
    printOled(text, 0, 20, 1, false);
  }
  sprintf(text, "%c", c);
  printOled(text, 120, 25, 1, true);
}

void addLog() {
  int hh = hour();
  int minuteNow = hh * 60 + minute();
  if(ld[minuteNow]==0) {
    ld[minuteNow]=((short)(fLastTemp*10))&0x0FFF| /*temp lowBits*/
                   (bFire?1<<15:0)| /* 0x8000 is caldaia */
                   (sto.forceData.hForce?1<<14:0)|  /* 0x4000 is force state */
                   (bOnLine?1<<13:0); /* 0x2000 is wifi state */
    /*if(ld[minuteNow+1]!=0 && sendMail())
        memset(ld, 0, sizeof(ld));*/
    ld[minuteNow+1]=0;
    if(DEBUG) {
      char text[100];
      sprintf(text, "log: %.04x", ld[minuteNow]);
      Serial.println(text);
    }
  }
}
