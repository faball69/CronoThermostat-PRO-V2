/*
  WEBService: gestione WEBServer
  by Fabrizio Allevi
  fabrizio . allevi @ tiscali . it
*/

#include <SPI.h>
#include "main.h"
#include <ArduinoOTA.h>
#include "secret.h"

// forward reference
void page0(WiFiClient &client);
void page1(WiFiClient &client);
void page2(WiFiClient &client);
void page3(WiFiClient &client);

WiFiServer WEBserver(80);  // HTTP service

bool bFirst=true;
void initWeb() {
  if(bFirst) { // ??? is possible to re-run ???
    ArduinoOTA.begin(WiFi.localIP(), OTA_USER, OTA_PSWD, InternalStorage);
    WEBserver.begin();
    bFirst=false;
  }
}
int pageL=0; // 0=main 1=programs 2=settings 3=weekprog
String url="";
char buffer[5000];
void runWifiService() {
  // check connection
  ArduinoOTA.poll();
  // listen for incoming clients
  WiFiClient client = WEBserver.available();
  if (client) {
    // an http request ends with a blank line
    while (client.connected()) {
      if(DEBUG)
        Serial.println("\nvvvvvv HTTP vvvvvv");
      if (client.available()) {
        memset(buffer, 0, sizeof(buffer));
        client.read((uint8_t*)buffer, sizeof(buffer));
        String text=buffer;
        if(text.indexOf("GET /programs ")!=-1)
          pageL=1;
        else if(text.indexOf("GET /settings ")!=-1)
          pageL=2;
        else if(text.indexOf("GET /weekprog ")!=-1)
          pageL=3;
        else if(text.indexOf("GET / ")!=-1)
          pageL=0;
        if(DEBUG)
          Serial.print(text);
        int i1=text.indexOf("Referer:");
        if(i1!=-1) {
          url=text.substring(i1+9);
          url=url.substring(0,url.indexOf("\n")-1);
        }
        if(DEBUG)
          Serial.println("url="+url);
        int ini=text.indexOf("?");
        int fin=text.indexOf(" HTTP");
        if(ini!=-1 && fin!=-1) {  // ricezione dati cambiati
          bool bChangeFH=false;
          buffer[fin]='\0'; // termino stringa fin dove mi serve
          char *ptr=strtok(buffer+ini+1, "&");
          while(ptr!=NULL) {
            String s=ptr;
            ini = s.indexOf("=");
            fin = s.length();
            String var=s.substring(0,ini);
            String val=s.substring(ini+1,fin);
            if(DEBUG) {
              Serial.println("var="+var);
              Serial.println("val="+val);
            }
            if(var[0]=='T') {
              i1=var.substring(1,3).toInt();
              sto.progs[i1/6].T[i1%6]=val.toInt();
            }
            else if(var[0]=='H') {
              i1=var.substring(2,4).toInt();
              sto.progs[i1/6].HM[i1%6]=(int)strtol(val.c_str(), 0, 16);
            }
            else if(var[0]=='W') {
              i1=var.substring(2,3).toInt();
              sto.weekProg[i1]=val.toInt();
            }
            else if(var[0]=='F') {
              if(var[1]=='H') {
                if(sto.forceData.hForce!=val.toInt()) {
                  float hf=val.toInt();
                  sto.forceData.hForce=hf;
                  if(hf<0)
                    hf*=-1;
                  sto.forceData.tFin=now()+hf*3600;
                  bChangeFH=true;
                }
              }
              else if(var[1]=='O') {
                sto.forceData.ofsTemp=val.toInt();
              }
              else if(var[1]=='M') {
                sto.forceData.maxTemp=val.toInt();
              }
              else if(var[1]=='Y') {
                sto.forceData.hysteresisTemp=val.toInt();
              }
              else if(var[1]=='P') {
                sto.forceData.bOFF=val.toInt();
              }
            }
            ptr = strtok(NULL, "&");  // takes a list of delimiters
          }
          if(!bChangeFH)
            saveDataFlash();  // save in storage wifi /fs/ flash
        }
        // create page
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: text/html");
        client.println("Connection: close");  // the connection will be closed after completion of the response
        //client.println("Refresh: 5");  // refresh the page automatically every 5 sec
        client.println();
        if(pageL==0)
          page0(client);
        else if(pageL==1)
          page1(client);
        else if(pageL==2)
          page2(client);
        else if(pageL==3)
          page3(client);
        if(DEBUG)
          Serial.println("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^");
        break;
      }
    }
    // give the web browser time to receive the data
    delay(1);
  // close the connection:
    client.stop();
  }
}

//<script>if(typeof window.history.pushState == 'function') {window.history.pushState({}, \"Hide\", \"%s\");}</script>
void page0(WiFiClient &client) {
  memset(buffer, 0, sizeof(buffer));
  sprintf(buffer, "\
    <!DOCTYPE HTML>\
    <html><body>\
    <h1>CT2_FA</h1>\
    <h2>Current temperature: %.1f&#x2103; %s</h2>\
    <form method=get>\
      Force for hours: <input type=number size=1 name=FH value=%d> <input type=submit value=Force>\
    </form>\
    <h3><a href=\"/programs\">programs</a> \
    <a href=\"/weekprog\">weekprog</a>\
    <a href=\"/settings\">settings</a></h3>\
    </body></html>\n", fLastTemp, (bFire?"ON":"OFF"), sto.forceData.hForce);
  if(DEBUG) {
      Serial.println(buffer);
      Serial.println(strlen(buffer));
  }
  client.println(buffer);
}

void page1(WiFiClient &client) {
  memset(buffer, 0, sizeof(buffer));
  sprintf(buffer, "\
    <!DOCTYPE HTML>\
    <html><body>\
    <h1>CT2_FA</h1>\
    <h2>Programs: T[Deg*10] HM[0xhhmm]</h2>\
    <form method=get>\
      <p style=\"text-align:right;\">AllDay:<input type=text size=1 name=T00 value=%d><input type=text size=4 name=HM00 value=0x%.04x>\
      <input type=text size=1 name=T01 value=%d><input type=text size=4 name=HM01 value=0x%.04x>\
      <input type=text size=1 name=T02 value=%d><input type=text size=4 name=HM02 value=0x%.04x>\
      <input type=text size=1 name=T03 value=%d><input type=text size=4 name=HM03 value=0x%.04x>\
      <input type=text size=1 name=T04 value=%d><input type=text size=4 name=HM04 value=0x%.04x>\
      <input type=text size=1 name=T05 value=%d><input type=text size=4 name=HM05 value=0x%.04x></p>\
      <p style=\"text-align:right;\">Morning+Evening:<input type=text size=1 name=T06 value=%d><input type=text size=4 name=HM06 value=0x%.04x>\
      <input type=text size=1 name=T07 value=%d><input type=text size=4 name=HM07 value=0x%.04x>\
      <input type=text size=1 name=T08 value=%d><input type=text size=4 name=HM08 value=0x%.04x>\
      <input type=text size=1 name=T09 value=%d><input type=text size=4 name=HM09 value=0x%.04x>\
      <input type=text size=1 name=T10 value=%d><input type=text size=4 name=HM10 value=0x%.04x>\
      <input type=text size=1 name=T11 value=%d><input type=text size=4 name=HM11 value=0x%.04x></p>\
      <p style=\"text-align:right;\">Early+Evening:<input type=text size=1 name=T12 value=%d><input type=text size=4 name=HM12 value=0x%.04x>\
      <input type=text size=1 name=T13 value=%d><input type=text size=4 name=HM13 value=0x%.04x>\
      <input type=text size=1 name=T14 value=%d><input type=text size=4 name=HM14 value=0x%.04x>\
      <input type=text size=1 name=T15 value=%d><input type=text size=4 name=HM15 value=0x%.04x>\
      <input type=text size=1 name=T16 value=%d><input type=text size=4 name=HM16 value=0x%.04x>\
      <input type=text size=1 name=T17 value=%d><input type=text size=4 name=HM17 value=0x%.04x></p>\
      <p style=\"text-align:right;\">maiNtenance:<input type=text size=1 name=T18 value=%d><input type=text size=4 name=HM18 value=0x%.04x>\
      <input type=text size=1 name=T19 value=%d><input type=text size=4 name=HM19 value=0x%.04x>\
      <input type=text size=1 name=T20 value=%d><input type=text size=4 name=HM20 value=0x%.04x>\
      <input type=text size=1 name=T21 value=%d><input type=text size=4 name=HM21 value=0x%.04x>\
      <input type=text size=1 name=T22 value=%d><input type=text size=4 name=HM22 value=0x%.04x>\
      <input type=text size=1 name=T23 value=%d><input type=text size=4 name=HM23 value=0x%.04x></p>\
      <input type=submit value=Change+Store>\
    </form>\
    </body></html>\n",
    sto.progs[0].T[0], sto.progs[0].HM[0],
    sto.progs[0].T[1], sto.progs[0].HM[1],
    sto.progs[0].T[2], sto.progs[0].HM[2],
    sto.progs[0].T[3], sto.progs[0].HM[3],
    sto.progs[0].T[4], sto.progs[0].HM[4],
    sto.progs[0].T[5], sto.progs[0].HM[5],
    sto.progs[1].T[0], sto.progs[1].HM[0],
    sto.progs[1].T[1], sto.progs[1].HM[1],
    sto.progs[1].T[2], sto.progs[1].HM[2],
    sto.progs[1].T[3], sto.progs[1].HM[3],
    sto.progs[1].T[4], sto.progs[1].HM[4],
    sto.progs[1].T[5], sto.progs[1].HM[5],
    sto.progs[2].T[0], sto.progs[2].HM[0],
    sto.progs[2].T[1], sto.progs[2].HM[1],
    sto.progs[2].T[2], sto.progs[2].HM[2],
    sto.progs[2].T[3], sto.progs[2].HM[3],
    sto.progs[2].T[4], sto.progs[2].HM[4],
    sto.progs[2].T[5], sto.progs[2].HM[5],
    sto.progs[3].T[0], sto.progs[3].HM[0],
    sto.progs[3].T[1], sto.progs[3].HM[1],
    sto.progs[3].T[2], sto.progs[3].HM[2],
    sto.progs[3].T[3], sto.progs[3].HM[3],
    sto.progs[3].T[4], sto.progs[3].HM[4],
    sto.progs[3].T[5], sto.progs[3].HM[5]);
  if(DEBUG) {
      Serial.println(buffer);
      Serial.println(strlen(buffer));
  }
  client.println(buffer);
}

void page2(WiFiClient &client) {
  memset(buffer, 0, sizeof(buffer));
  sprintf(buffer, "\
  <!DOCTYPE HTML>\
  <html><body>\
  <h1>CT2_FA</h1>\
  <h2>Settings: T[Deg*10]</h2>\
  <form method=get>\
    <p>ofsTemp <input type=text size=1 name=FO value=%d>\
    maxTemp <input type=text size=1 name=FM value=%d>\
    hysTemp <input type=text size=1 name=FY value=%d>\
    plantOFF <input type=text size=1 name=FP value=%d></p>\
    <input type=submit value=Change+Store>\
  </form>\
  </body></html>\n", sto.forceData.ofsTemp, sto.forceData.maxTemp, sto.forceData.hysteresisTemp, sto.forceData.bOFF);
  if(DEBUG) {
      Serial.println(buffer);
      Serial.println(strlen(buffer));
  }
  client.println(buffer);
}

void page3(WiFiClient &client) {
  memset(buffer, 0, sizeof(buffer));
  sprintf(buffer, "\
  <!DOCTYPE HTML>\
  <html><body>\
  <h1>CT2_FA</h1>\
  <h2>WeekProg: WP[0=AllDay, 1=M+E, 2=E+E, 3=mNt]</h2>\
  <form method=get>\
    <p>sun <input type=text size=1 name=WP0 value=%d>\
    mon <input type=text size=1 name=WP1 value=%d>\
    tue <input type=text size=1 name=WP2 value=%d>\
    wed <input type=text size=1 name=WP3 value=%d>\
    thu <input type=text size=1 name=WP4 value=%d>\
    fri <input type=text size=1 name=WP5 value=%d>\
    sat <input type=text size=1 name=WP6 value=%d></p>\
    <input type=submit value=Change+Store>\
  </form>\
  </body></html>\n", sto.weekProg[0], sto.weekProg[1], sto.weekProg[2], sto.weekProg[3], sto.weekProg[4], sto.weekProg[5], sto.weekProg[6]);
  if(DEBUG) {
      Serial.println(buffer);
      Serial.println(strlen(buffer));
  }
  client.println(buffer);
}
