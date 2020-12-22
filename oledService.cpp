/*
 * gestione OLED per programma CT2_FA
 * by Fabrizio Allevi
 * fabrizio . allevi @ tiscali . it
 */

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "main.h"

// oled init----------------------------------------------------------------
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define LOGO_HEIGHT   16
#define LOGO_WIDTH    16
static const unsigned char PROGMEM fire_bmp[] =
{ B00000001, B00000000,
  B00000001, B10000000,
  B00000011, B11100000,
  B00000111, B00110000,
  B00001110, B00011000,
  B00011100, B00001100,
  B00111000, B00000110,
  B01110000, B00000011,
  B11100000, B10000011,
  B11100000, B01000011,
  B11100000, B00100011,
  B11000010, B00100011,
  B01100100, B00100011,
  B00100010, B01000110,
  B00010001, B10001000,
  B00001000, B00010000 };
static const unsigned char PROGMEM off_bmp[] =
{ B00000000, B00000000,
  B00000000, B00000000,
  B00000000, B00000000,
  B00000000, B00000000,
  B00000000, B00000000,
  B00000000, B00000000,
  B00000000, B00000000,
  B00000000, B00000000,
  B00000000, B00000000,
  B00000000, B00000000,
  B00000000, B00000000,
  B00000000, B00000000,
  B00000000, B00000000,
  B11000001, B10000011,
  B11000001, B10000011,
  B11111111, B11111111 };
  static const unsigned char PROGMEM wifi_bmp[] =
  { B01111110,
    B11000011,
    B00000000,
    B00111100,
    B01100110,
    B00000000,
    B00011000,
    B00011000};
  static const unsigned char PROGMEM nowifi_bmp[] =
  { B11000011,
    B01100110,
    B00100100,
    B00011000,
    B00011000,
    B00100100,
    B01100110,
    B11000011};

void initOled() {
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
    if(DEBUG)
      Serial.println(F("SSD1306 allocation failed"));
    while (true);
  }
  // Clear the buffer
  display.clearDisplay();
  display.display();
}


int dimmer=0;
int delta=1;
void changeDimmer() {
  dimmer+=delta;
  if(dimmer>=2)
    delta=-1;
  else if(dimmer<=0)
    delta=1;
  display.ssd1306_command(SSD1306_SETCONTRAST);
  display.ssd1306_command(dimmer);
}

void setDimmer(int d) {
  if(d>=0x8F)
    d=0x8F;
  else if(d<0)
    d=0;
  display.ssd1306_command(SSD1306_SETCONTRAST);
  display.ssd1306_command(d);
}

void lowDimmer() {
  display.ssd1306_command(SSD1306_SETCONTRAST);
  display.ssd1306_command(0x01);
}

void normalDimmer() {
  display.ssd1306_command(SSD1306_SETCONTRAST);
  display.ssd1306_command(0x20);
}

void printOled(const char *text, int posx, int posy, int size, bool bShow) {  // 6x8 12x16 18x24
  display.setTextSize(size);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(posx,posy);
  display.println(text);
  if(bShow)
    display.display();
}

void setCursorOled(int row, int col) {
  display.setCursor(row*9,col*7);
}

void clearOled() {
  display.clearDisplay();
}

void blinkOled(bool bOn) {
}

void scrollOled(bool bOn) {
  if(bOn)
    display.startscrollright(0x00, 0x0F);
  else
    display.stopscroll();
}

void fireBitmap(int posx, int posy, bool b) {
  //display.drawBitmap((display.width()-LOGO_WIDTH )/2, (display.height()-LOGO_HEIGHT)/2, fire_bmp, LOGO_WIDTH, LOGO_HEIGHT, 1);
  if(b)
    display.drawBitmap(posx, posy, fire_bmp, LOGO_WIDTH, LOGO_HEIGHT, 1);
  else
    display.drawBitmap(posx, posy, off_bmp, LOGO_WIDTH, LOGO_HEIGHT, 1);
}

void netBitmap(int posx, int posy, bool b) {
  if(b)
    display.drawBitmap(posx, posy, wifi_bmp, 8, 8, 1);
  else
    display.drawBitmap(posx, posy, nowifi_bmp, 8, 8, 1);
}
