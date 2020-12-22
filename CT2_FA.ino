/*
   CT2_FA cronoTermostat V2
   by Fabrizio Allevi
   fabrizio . allevi @ tiscali . it
*/

#include "main.h"

// globals
String sDays[MAX_DAYS] = { "Dom", "Lun", "Mar", "Mer", "Gio", "Ven", "Sab" };
String sMonth[12] = { "Gen", "Feb", "Mar", "Apr", "Mag", "Giu", "Lug", "Ago", "Set", "Ott", "Nov", "Dic" };
long lastOp;
float fLastTemp=0.0f;
short ld[1440];
// forward ref
void settingsPage(const char *t, int par=0);
void datePage(int var, int val);

void setup() {
  // init proc
  memset(ld, 0, sizeof(ld));
  //pinMode(LED_BUILTIN, OUTPUT);
  //digitalWrite(LED_BUILTIN, LOW);
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
  settingsPage("init");
  initWifiService();
  if(DEBUG)
    Serial.println("setup terminated!");
  lastOp=millis()-10000;
}

long lastUpd=0;
int aux=16; // start in page==4
int page=0; //0=idle 1=force 2=ofs 3=prog 4=setting
int subPage=-1;
bool bModify=false;
bool okTime=false;
tmElements_t tmMan;
void loop() {
  long nowTime=millis();
  //knob
  int dir = getDir();
  bool sw = getSwitch();
  // temp update
  if(nowTime>lastUpd+5000) {
    fLastTemp = readTemp();
    lastUpd=nowTime;
    if(okTime)
      scheduler();
  }
  // Time?
  int h=0;
  if(year()>=2020) {
    okTime=true;
  }
  // wifi
  if(okTime) {
    h=hour();
    if(h>5 && h<23) {
      if(checkWifiStatus())
        runWifiService();
      else
        tryConnection(nowTime);
    }
    else if(wifiStatus==WL_CONNECTED)
      resetWifi(1);
  }
  else {
    if(checkWifiStatus())
      updateNtpTime();
    else
      tryConnection(nowTime);
  }
  // menus
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
      idlePage();
    }
    else if(page==1) {  // force page
      forcePage(sto.forceData.hForce);
    }
    else if(page==2) {  // ofs page
      ofsPage(sto.forceData.ofsTemp);
    }
    else if(page==3) {  // program page
      if(okTime)
        programPage();
      else {
       datePage(-1,-2);
       memset(&tmMan, 0, sizeof(tmMan));
       tmMan.Year=30;
       subPage=-1;
     }
    }
    else if(page==4) {  // settings page
      lastOp=nowTime;
      settingsPage("net");
    }
    if(sw) {
      bModify=true;
      lastOp=nowTime;
      if(page==1)
        aux=sto.forceData.hForce*4;
      else if(page==2)
        aux=sto.forceData.ofsTemp;
      else
        aux=0;
    }
  }
  else {  // modifica
    if(nowTime<lastOp+10000) {
      if(page==1) { // force plant
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
            aux=0;
            bModify=false;
          }
        }
      }
      else if(page==2) {  // set offset temp
        if(dir) {
          aux+=dir;
          lastOp=nowTime;
        }
        ofsPage(aux);
        if(sw && aux!=sto.forceData.ofsTemp) {
          sto.forceData.ofsTemp=aux;
          saveDataFlash();  // save in storage wifi /fs/ flash
          aux=0;
          bModify=false;
        }
      }
      else if(page==3) {  // set date manual
        if(dir) {
          aux+=dir;
          lastOp=nowTime;
        }
        if(subPage==-1) {
          if(aux<0)
            aux=0;
          if(aux>19)
            aux=19;
          datePage(aux/4, -2);
          if(sw) {
            subPage=aux/4;
          }
        }
        else {
          datePage(subPage, dir);
          if(sw) {
            if(subPage==4) {
              setTime(makeTime(tmMan));
              aux=0;
              bModify=false;
              okTime=true;
            }
            else {
              aux=subPage*4;
              subPage=-1;
            }
          }
        }
      }
      else if(page==4) {  // settings pars
        if(dir) {
          aux+=dir;
          lastOp=nowTime;
        }
        if(aux<0)
          aux=0;
        if(aux>=12)
          aux=12;
        if(aux<3) {
          settingsPage(">", 3);
          if(sw) {
            if(wifiStatus==WL_CONNECTED && sendMail()) {
              memset(ld, 0, sizeof(ld));
              settingsPage("ok", 3);
            }
            else
              settingsPage("ko", 3);
            delay(1000);
            aux=16; // force page==4
            bModify=false;
          }
        }
        else if(aux<6) {
          settingsPage(">", 4);
          if(sw) {
            resetWifi(1);
            settingsPage("ok", 4);
            delay(1000);
            aux=16; // force page==4
            bModify=false;
          }
        }
        else if(aux<9) {
          settingsPage("rst", 1);
          if(sw) {
            settingsPage("ok", 2);
            delay(1000);
            NVIC_SystemReset(); // system reset
          }
        }
        else {
          if(wifiStatus!=WL_NO_MODULE) {
            settingsPage("ON", 5);
            if(sw) {
              resetWifi(2);
              delay(1000);
              aux=0;
              bModify=false;
            }
          }
          else
            settingsPage("OFF", 5);
        }
      }
    }
    else {
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
  fireBitmap(100, 10, bFire);
  netBitmap(116, 0, wifiStatus==WL_CONNECTED);
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

void datePage(int var, int val) {
  char text[100];
  String si="";
  if(val!=-2)
    si="set ";
  else
    val=0;
  clearOled();
  if(var==0) {
    tmMan.Year+=val;
    sprintf(text, "%sYear=%d", si.c_str(), tmMan.Year+1970);
    printOled(text, 0, 0, 1, true);
  }
  else if(var==1) {
    tmMan.Month+=val;
    sprintf(text, "%sMonth=%d", si.c_str(), tmMan.Month);
    printOled(text, 0, 0, 1, true);
  }
  else if(var==2) {
    tmMan.Day+=val;
    sprintf(text, "%sDay=%d", si.c_str(), tmMan.Day);
    printOled(text, 0, 0, 1, true);
  }
  else if(var==3) {
    tmMan.Hour+=val;
    sprintf(text, "%sHour=%d", si.c_str(), tmMan.Hour);
    printOled(text, 0, 0, 1, true);
  }
  else if(var==4) {
    tmMan.Minute+=val;
    sprintf(text, "%sMin=%d", si.c_str(), tmMan.Minute);
    tmMan.Second=0;
    printOled(text, 0, 0, 1, true);
  }
  else {
    printOled("Time is not setted!\nPlease push knob!", 0, 0, 1, true);
  }
}

void settingsPage(const char *t, int par/*=0*/) {
  char text[100];
  clearOled();
  if(par==0) {
    sprintf(text, "SSID: %s", tSSID.c_str());
    printOled(text, 0, 0, 1, false);
    sprintf(text, "ipAddr: %s\n", ipAddr.c_str());
    printOled(text, 0, 10, 1, false);
    sprintf(text, "RSSI:%ddBm S:%d", RSSI, wifiStatus);
    printOled(text, 0, 20, 1, false);
  }
  else if(par==1) {
    sprintf(text, "click for rst board!");
    printOled(text, 0, 10, 1, false);
  }
  else if(par==2) {
    sprintf(text, "reset in progress!");
    printOled(text, 0, 10, 1, false);
  }
  else if(par==3) {
    sprintf(text, "click for email logs!");
    printOled(text, 0, 10, 1, false);
  }
  else if(par==4) {
    sprintf(text, "click for cnct wifi!");
    printOled(text, 0, 10, 1, false);
  }
  else if(par==5) {
    sprintf(text, "click for %s wifi!", (wifiStatus!=WL_NO_MODULE?"dis":"en"));
    printOled(text, 0, 10, 1, false);
  }
  sprintf(text, "%s", t);
  printOled(text, 100, 25, 1, true);
}

void addLog() {
  int hh = hour();
  int minuteNow = hh * 60 + minute();
  if(ld[minuteNow]==0) {
    ld[minuteNow]=((short)(fLastTemp*10))&0x0FFF| /*temp lowBits*/
                   (bFire?1<<15:0)| /* 0x8000 is caldaia */
                   (sto.forceData.hForce?1<<14:0)|  /* 0x4000 is force state */
                   (wifiStatus==WL_CONNECTED?1<<13:0); /* 0x2000 is wifi state */
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
