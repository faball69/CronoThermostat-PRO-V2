 /*
   wifiService: gestione wifi
   by Fabrizio Allevi
   fabrizio . allevi @ tiscali . it
*/

#include <SPI.h>
#include <WiFiNINA.h>
#include <ArduinoOTA.h>
#include "main.h"

#include "secret.h"
String ssid[MAX_NETWORKS]={SECRET_SSID_P, SECRET_SSID_0, SECRET_SSID_1, SECRET_SSID_2};
String pass[MAX_NETWORKS]={SECRET_PASS_P, SECRET_PASS_0, SECRET_PASS_1, SECRET_PASS_2};

// NTP
unsigned int localPort = 2390;      // local port to listen for UDP packets
//IPAddress timeServer(129, 6, 15, 28); // time.nist.gov NTP server
IPAddress timeServer(193,204,114,105);  // time.inrim.it
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

// exported
String ipAddr="?.?.?.?";
String tSSID="????";
long RSSI=0;
bool bOnLine=false;
// forward reference
void page0(WiFiClient &client);
void page1(WiFiClient &client);
void page2(WiFiClient &client);
void page3(WiFiClient &client);

WiFiServer WEBserver(80);  // HTTP service

// storage
stStorage preSto =  {true,
                    {{{ 200, 190, 200, 190, 200, 190 }, { 0x071E, 0x091E, 0x0B00, 0x0D00, 0x0F00, 0x1700 }},
                    {{ 200, 190, 200, 190, 190, 190 }, { 0x071E, 0x091E, 0x0F00, 0x1700, 0x0000, 0x0000 }},
                    {{ 200, 190, 200, 190, 190, 190 }, { 0x051E, 0x0800, 0x0F00, 0x1700, 0x0000, 0x0000 }},
                    {{ 170, 170, 170, 170, 170, 170 }, { 0x0000, 0x173B, 0x0000, 0x0000, 0x0000, 0x0000 }}},
                    { 0, 2, 2, 2, 2, 2, 1 },
                    { 0, 0, -25, 210, 15, false}};
stStorage sto;
void saveDataFlash() {
  WiFiStorageFile s = WiFiStorage.open("/fs/CT2_Storage");
  if(s) {
    s.erase();
  }
  s.write(&sto, sizeof(stStorage));
  if(DEBUG)
    Serial.println("flash update storage!");
}
void initFlash() {
  bool bInit=false;
  // init storage
  WiFiStorageFile s = WiFiStorage.open("/fs/CT2_Storage");
  if (s) {
    while (s.available()) {
      int ret = s.read(&sto, sizeof(stStorage));
      if(ret<sizeof(stStorage) || !sto.bValue) {
        bInit=true;
        if(DEBUG)
          Serial.println("file storage is not equal!");
      }
      else {
        if(DEBUG)
          Serial.println("file storage read WELL!");
      }
    }
  }
  else {
    if(DEBUG)
      Serial.println("storage IS NOT VALID!");
    bInit=true;
  }
  if(bInit) {
    memcpy(&sto, &preSto, sizeof(stStorage));
    saveDataFlash();
  }
}

void initWifiService() {
  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    if(DEBUG)
      Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true);
  }
  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    if(DEBUG)
      Serial.println("Please upgrade the firmware");
  }
  initFlash();
}

bool updateNtpTime() {
  WiFiUDP Udp;
  if(Udp.begin(localPort)) {
    delay(100);
    while (Udp.parsePacket() > 0) ; // discard any previously received packets
    if(DEBUG)
      Serial.println("Transmit NTP Request");
    // send NPT packet---------------------------------
    memset(packetBuffer, 0, NTP_PACKET_SIZE);
    packetBuffer[0] = 0b11100011;   // LI, Version, Mode
    packetBuffer[1] = 0;     // Stratum, or type of clock
    packetBuffer[2] = 6;     // Polling Interval
    packetBuffer[3] = 0xEC;  // Peer Clock Precision
    // 8 bytes of zero for Root Delay & Root Dispersion
    packetBuffer[12]  = 49;
    packetBuffer[13]  = 0x4E;
    packetBuffer[14]  = 49;
    packetBuffer[15]  = 52;
    // all NTP fields have been given values, now
    // you can send a packet requesting a timestamp:
    Udp.beginPacket(timeServer, 123); //NTP requests are to port 123
    Udp.write(packetBuffer, NTP_PACKET_SIZE);
    Udp.endPacket();
    //-------------------------------------------------
    //sendNTPpacket(timeServer);
    uint32_t beginWait = millis();
    while (millis() - beginWait < 1500) {
      int size = Udp.parsePacket();
      if (size >= NTP_PACKET_SIZE) {
        if(DEBUG)
          Serial.println("Receive NTP Response");
        Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
        unsigned long secsSince1900;
        // convert four bytes starting at location 40 to a long integer
        secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
        secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
        secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
        secsSince1900 |= (unsigned long)packetBuffer[43];
        time_t nptt=secsSince1900 - 2208988800UL + 1*SECS_PER_HOUR; // +1==Central European Time
        setTime(nptt);
        return true;
      }
    }
  }
  else
    if(DEBUG)
      Serial.println("No UDP service :-(");
  return false;
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

bool bDisconnect=false;
long msLastConn=millis();
long msLastStatus=millis();
int timeout=10000;
int tryCount=0;
int status = WL_IDLE_STATUS;
bool checkConnection(long msNow) {
  if(msNow>msLastStatus+1000) {
    status=WiFi.status();
    msLastStatus=msNow;
  }
  // attempt to connect to Wifi network:
  if(status!=WL_CONNECTED) {
    bOnLine=false;
    ipAddr="?.?.?.?";
    int h=0;
    if(bOnLine)
      h=hour();
    if(msNow>msLastConn+timeout && (!bOnLine ||(h>5 && h<23))) {
      if(DEBUG) {
        Serial.println(status);
        Serial.print("Attempting to connect to SSID: ");
        Serial.println(ssid[tryCount]);
      }
      status = WiFi.begin(ssid[tryCount].c_str(), pass[tryCount].c_str());
      tSSID=ssid[tryCount];
      tryCount=(tryCount+1)%MAX_NETWORKS;
      msLastConn=msNow;
      bDisconnect=true;
      if(tryCount==0 && timeout<60000)
        timeout+=10000;
    }
  }
  else if(bDisconnect) {
    if(DEBUG)
      Serial.println("Connected to wifi");
    // print wifi status--------------------------------------------------------
    tSSID=WiFi.SSID();
    if(DEBUG) {
      Serial.print("SSID: ");
      Serial.println(tSSID);
    }
    // print your board's IP address:
    IPAddress ip = WiFi.localIP();
    if(DEBUG) {
      Serial.print("IP Address: ");
      Serial.println(ip);
    }
    char text[17];
    sprintf(text, "%d.%d.%d.%d\0", ip[0], ip[1], ip[2], ip[3]);
    ipAddr=text;
    // print the received signal strength:
    RSSI = WiFi.RSSI();
    if(DEBUG) {
      Serial.print("signal strength (RSSI):");
      Serial.print(RSSI);
      Serial.println(" dBm");
    }
    //--------------------------------------------------------------------------
    if(!okTime) { // ??? is possible to re-run ???
      ArduinoOTA.begin(WiFi.localIP(), OTA_USER, OTA_PSWD, InternalStorage);
      WEBserver.begin();
    }
    if(DEBUG)
      Serial.println("Starting connection to server...");
    bDisconnect=false;
    bOnLine=true;
    timeout=10000;
  }
  return bOnLine;
}

// MAIL sender -----------------------------------------------------------------
#include "Base64.h"
byte response(WiFiSSLClient client);
void encode64(String InputString, char *res);

bool sendMail() {
  bool bRet=true;
  const String gAcc   = SECRET_SEND_ACCOUNT, gPass  = SECRET_SEND_ACCOUNT_PASSWORD;
  int encodedLength = Base64.encodedLength(gAcc.length());
  char encodedAccount[encodedLength+1];
  encode64(gAcc, encodedAccount);
  encodedAccount[encodedLength] = '\0';

  encodedLength = Base64.encodedLength(gPass.length());
  char encodedPass[encodedLength+1];
  encode64(gPass, encodedPass);
  encodedPass[encodedLength] = '\0';

  if(DEBUG)
    Serial.println("\nConnecting to server: " + String(SMTP_SERVER) +":" +String(465));

  WiFiSSLClient client;

  if (client.connectSSL(SMTP_SERVER, 465)==1){
    if(DEBUG)
      Serial.println("Connected to server");
    if (response(client) ==-1){
      String s = SMTP_SERVER + String(" port:")+ String(465);
      if(DEBUG) {
        Serial.print("no reply on connect to ");
        Serial.println(s);
        bRet=false;
      }
    }

    if(DEBUG)
      Serial.println("Sending Extended Hello: <start>EHLO underroof.allevis.org<end>");
    client.println("EHLO underroof.allevis.org");
    if (DEBUG && response(client) ==-1){
      Serial.println("no reply EHLO underroof.allevis.org");
      bRet=false;
    }

    if(DEBUG)
      Serial.println("Sending auth login: <start>AUTH LOGIN<end>");
    client.println(F("AUTH LOGIN"));
    if (DEBUG && response(client) ==-1){
      Serial.println("no reply AUTH LOGIN");
      bRet=false;
    }

    if(DEBUG)
      Serial.println("Sending account: <start>" +String(encodedAccount) + "<end>");
    client.println(F(encodedAccount));
    if (DEBUG && response(client) ==-1){
      Serial.println("no reply to Sending User");
      bRet=false;
    }

    if(DEBUG)
      Serial.println("Sending Password: <start>" +String(encodedPass) + "<end>");
    client.println(F(encodedPass));
    if (DEBUG && response(client) ==-1){
      Serial.println("no reply Sending Password");
      bRet=false;
    }

    if(DEBUG)
      Serial.println("Sending From: <start>MAIL FROM: <fabrizio.allevi@tiscali.it><end>");
    client.println(F("MAIL FROM: <fabrizio.allevi@tiscali.it>"));
    if (DEBUG && response(client) ==-1){
      Serial.println("no reply Sending From");
      bRet=false;
    }

    if(DEBUG)
      Serial.println("Sending To: <start>RCPT To: <fabrizio.allevi@gmail.com><end>");
    client.println(F("RCPT To: <fabrizio.allevi@gmail.com>"));
    if (DEBUG && response(client) ==-1){
      Serial.println("no reply Sending To");
      bRet=false;
    }

    if(DEBUG)
      Serial.println("Sending DATA: <start>DATA<end>");
    client.println(F("DATA"));
    if (DEBUG && response(client) ==-1){
      Serial.println("no reply Sending DATA");
      bRet=false;
    }

    if(DEBUG)
      Serial.println("Sending email: <start>");
    client.println(F("To: Admin <fabrizio.allevi@gmail.com>"));

    client.println(F("From: MK1010 <fabrizio.allevi@tiscali.it>"));
    client.println(F("Subject: underroof data"));
    float f;
    int maxpkt=100;
    int npkt=0;
    while(npkt<1440) {
      memset(buffer, 0, sizeof(buffer));
      maxpkt=(1440-npkt<maxpkt)?1440-npkt:100;
      int wc=0;
      for(int i=0; i<maxpkt; i++) {
        int d=i+npkt;
        f=ld[d]&0x0FFF;
        f/=10;
        wc+=sprintf(buffer+wc, "%.1f,%d,%d,%d\n", f, ld[d]&0x8000?1:0, ld[d]&0x4000?1:0, ld[d]&0x2000?1:0);
      }
      npkt+=maxpkt;
      if(DEBUG)
        Serial.print(buffer);
      client.print(F(buffer));
    }
    if(DEBUG)
      Serial.println("");
    client.println(F(""));

    client.println(F("."));
    if (DEBUG && response(client) ==-1){
      Serial.println("no reply Sending '.'");
      bRet=false;
    }

    if(DEBUG)
      Serial.println(F("Sending QUIT"));
    client.println(F("QUIT"));
    if (DEBUG && response(client) ==-1){
      Serial.println("no reply Sending QUIT");
      bRet=false;
    }
    client.stop();
  }
  else{
    if(DEBUG)
      Serial.println("failed to connect to server");
    bRet=false;
  }
  if(DEBUG)
    Serial.println("Done.");
  return bRet;
}

byte response(WiFiSSLClient client){
  // Wait for a response for up to X seconds
  int loopCount = 0;
  while (!client.available()) {
    delay(1);
    loopCount++;
    // if nothing received for 1O00 milliseconds, timeout
    if (loopCount > 10000) {
      //client.stop();
      if(DEBUG)
        Serial.println(F("Timeout"));
      return -1;
    }
  }

  // Take a snapshot of the response code
  byte respCode = client.peek();
  if(DEBUG)
    Serial.print("response: <start>");
  while (DEBUG && client.available()){
    Serial.write(client.read());
  }
  if(DEBUG)
    Serial.println("<end>");

  if (respCode >= '4'){
    if(DEBUG) {
      Serial.print("Failed in eRcv with response: ");
      Serial.println(respCode);
    }
    return 0;
  }
  return 1;
}
void encode64(String InputString, char *res){
  int inputStringLength = InputString.length();
  const char *inputString = InputString.c_str();
  Base64.encode(res, (char*)inputString, inputStringLength);
}
