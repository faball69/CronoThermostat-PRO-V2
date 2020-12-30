 /*
   wifiService: gestione wifi
   by Fabrizio Allevi
   fabrizio . allevi @ tiscali . it
*/

#include <SPI.h>
#include "main.h"

#include "secret.h"
String ssid[MAX_NETWORKS]={SECRET_SSID_1, SECRET_SSID_0};
String pass[MAX_NETWORKS]={SECRET_PASS_1, SECRET_PASS_0};

// exported
String ipAddr="?.?.?.?";
String tSSID="????";
long RSSI=0;
bool bOnLine=false;

// storage
stStorage preSto =  {true,
                    {{{ 210, 195, 210, 195, 210, 195 }, { 0x071E, 0x091E, 0x0B00, 0x0D00, 0x0F00, 0x1700 }},
                    {{ 210, 195, 210, 195, 000, 000 }, { 0x071E, 0x091E, 0x0F00, 0x1700, 0x0000, 0x0000 }},
                    {{ 210, 195, 210, 195, 000, 000 }, { 0x051E, 0x0800, 0x0F00, 0x1700, 0x0000, 0x0000 }},
                    {{ 170, 170, 000, 000, 000, 000 }, { 0x0500, 0x173B, 0x0000, 0x0000, 0x0000, 0x0000 }}},
                    { 0, 2, 2, 2, 2, 2, 1 },
                    { 0, 0, -25, 220, 15, false}};
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

int wifiStatus, lastStatus;
void initWifiService() {
  // check for the WiFi module:
  wifiStatus=lastStatus=WiFi.status();
  if (wifiStatus == WL_NO_MODULE) {
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

int portNTP=2390;
bool updateNtpTime() {
  const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
  byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
  WiFiUDP Udp;
  if(Udp.begin(portNTP++)) {
    delay(100);
    while (Udp.parsePacket() > 0) ; // discard any previously received packets
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
    //IPAddress timeServer(129, 6, 15, 28); // time.nist.gov NTP server
    IPAddress timeServer(193,204,114,105);  // time.inrim.it
    Udp.beginPacket(timeServer, 123); //NTP requests are to port 123
    Udp.write(packetBuffer, NTP_PACKET_SIZE);
    Udp.endPacket();
    if(DEBUG)
      Serial.println("Transmit NTP Request");
      //-------------------------------------------------
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
    if(DEBUG)
      Serial.println("No NTP response :-(");
  }
  else {
    if(DEBUG)
      Serial.println("No UDP service :-(");
    resetWifi(0);
  }
  return false;
}
int wifiTimeout=10000;
long msLastConn=millis();
long msLastStatus=millis();
int tryCount=0;
void resetWifi(int level) {  // 0=reset 1=reset+cnt+tmt 2=disableWifi
  WiFi.end(); // clean before retry
  bOnLine=false;
  ipAddr="?.?.?.?";
  if(level==1) {
    tryCount=0;
    wifiTimeout=10000;
    lastStatus=wifiStatus=WL_DISCONNECTED;
  }
  else if(level==2) {
    lastStatus=wifiStatus=WL_NO_MODULE;
  }
}
bool checkWifiStatus() {
  if(wifiStatus==WL_NO_MODULE)
    return false;
  long msNow=millis();
  if(msNow>msLastStatus+1000) {
    lastStatus=wifiStatus;
    wifiStatus=WiFi.status();
    msLastStatus=msNow;
    if(DEBUG && lastStatus!=wifiStatus) {
      Serial.print("new wifi Status=");
      Serial.println(wifiStatus);
    }
  }
  if(lastStatus==WL_CONNECTED && wifiStatus!=WL_CONNECTED && bOnLine) {
    resetWifi(0);
  }
  else if(lastStatus!=WL_CONNECTED && wifiStatus==WL_CONNECTED && !bOnLine) {
    if(DEBUG)
      Serial.println("Connected to wifi");
    // print wifi infos
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
    initWeb();
    if(DEBUG)
      Serial.println("Starting connection to server...");
    bOnLine=true;
    wifiTimeout=10000;
  }
  return wifiStatus==WL_CONNECTED;
}
bool tryConnection(long msNow) {
  // attempt to connect to Wifi network:
  if(wifiStatus!=WL_CONNECTED && wifiStatus!=WL_NO_MODULE) {
    if(msNow>msLastConn+wifiTimeout) {
      if(DEBUG) {
        Serial.print("Attempting to connect to SSID: ");
        Serial.println(ssid[tryCount%MAX_NETWORKS]);
      }
      /*wifiStatus =*/ WiFi.begin(ssid[tryCount%MAX_NETWORKS].c_str(), pass[tryCount%MAX_NETWORKS].c_str());
      tSSID=ssid[tryCount%MAX_NETWORKS];
      tryCount++;
      msLastConn=msNow;
      if(tryCount>24)
        wifiTimeout+=3600000; // hourly
      else if(tryCount>12)
        wifiTimeout+=60000; // minutely
      else
        wifiTimeout=10000;  // ten second
      return true;
    }
  }
  return false;
}

// MAIL sender -----------------------------------------------------------------
#include "Base64.h"
byte response(WiFiSSLClient client);
void encode64(String InputString, char *res);

bool dataMail() {
  char buffer[1500];
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
    client.println(F("Subject: CT2_FA data"));
    float f;
    int maxpkt=80;
    int npkt=0;
    while(npkt<1440) {
      memset(buffer, 0, sizeof(buffer));
      maxpkt=(1440-npkt<maxpkt)?1440-npkt:80;
      int wc=0;
      for(int i=0; i<maxpkt; i++) {
        int d=i+npkt;
        f=ld[d]&0x0FFF;
        f/=10;
        wc+=sprintf(buffer+wc, "%.04d,%.1f,%d,%d,%d,%d\n", d, f, ld[d]&0x8000?1:0, ld[d]&0x4000?1:0, ld[d]&0x2000?1:0, ld[d]&0x1000?1:0);
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

bool resetMail() {
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
    client.println(F("Subject: CT2_FA reset"));

    // data payload
    char text[100];
    clearOled();
    int wd=weekday()-1;
    sprintf(text, "\n%.04d-%.02d-%.02d %.02d:%.02d\ntemp=%.1f\nbFire=%d\nwifi=%d\n", year(), month(), day(), hour(), minute(), fLastTemp, (int)bFire, wifiStatus);
    if(DEBUG)
      Serial.print(text);
    client.print(F(text));
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
