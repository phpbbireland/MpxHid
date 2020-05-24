/*
    Original Source...
    (c)2018 Pawel A. Hernik
    YouTube video: https://youtu.be/GHULqZpVpz4

    Original code was for Pro Mini but as it doesn't suppport HID, it required changing...
    Also, I required biod boot support so using HID-Project.h.
    
    All other code Michaelo (c) 2020
    Code is working but in early development... 24 May 2020
*/

#include <avr/sleep.h>
#include <avr/wdt.h>
#include <EEPROM.h>
#include "HID-Project.h"

#if USESPI==1
#include <SPI.h>
#endif

//#include <Keyboard.h> // we use HID-Project.h
#include "N5110_SPI.h"
#include "c64enh_font.h"
#include "small5x7_font.h"
#include "small5x7bold_font.h"

// uncomment to write min/max to EEEPROM //#define USE_EEPROM

#define N5110_RST       9
#define N5110_CS        10
#define N5110_DC        8
#define N5110_BACKLIGHT 6

enum PinAssignments {
  encoderPinA   = 2,   // right
  encoderPinB   = 3,   // left
  encoderButton = 4    // button
};

volatile int encoderPos = 0;
volatile int rotationPos = 0;       // can be 0, -1, 1
volatile int intracks = 0;
static boolean rotating = false;    // debounce management

boolean A_set = false;
boolean B_set = false;
boolean clicked = false;

N5110_SPI lcd(N5110_RST, N5110_CS, N5110_DC); // RST,CS,DC

static unsigned int vol =  9;
static unsigned int sur = 99;
static unsigned int bas = 50;
static unsigned int tre = 50;

int numMenus = 8;
int menuStart = 0;
int numScrLines = 6;
int menuMode = -1;
int oldPos = 0;
int mint2;
int maxt2;
int started = 0;

void initEncoder()
{
  pinMode(encoderPinA,   INPUT);
  pinMode(encoderPinB,   INPUT);
  pinMode(encoderButton, INPUT);
  digitalWrite(encoderPinA, HIGH);
  digitalWrite(encoderPinB, HIGH);
  digitalWrite(encoderButton, INPUT_PULLUP);
  attachInterrupt(0, doEncoderA, CHANGE);
  attachInterrupt(1, doEncoderB, CHANGE);
}

void buttonInt() {}

void readEncoderInt()
{
  (digitalRead(encoderPinA) == digitalRead(encoderPinB)) ? encoderPos++ : encoderPos--;
}

// not used //
enum wdt_time {
  SLEEP_15MS,
  SLEEP_30MS,
  SLEEP_60MS,
  SLEEP_120MS,
  SLEEP_250MS,
  SLEEP_500MS,
  SLEEP_1S,
  SLEEP_2S,
  SLEEP_4S,
  SLEEP_8S,
  SLEEP_FOREVER
};

ISR(WDT_vect) { wdt_disable(); }

#define PIXEL_OFF 0
#define PIXEL_ON  1
#define PIXEL_XOR 2

byte scr[84 * 4]; // frame buffer
byte scrWd = 84;
byte scrHt = 4;

void clrBuf()
{
  for (int i = 0; i < scrWd * scrHt; i++) scr[i] = 0;
}

void drawPixel(int16_t x, int16_t y, uint16_t c)
{
  if ((x < 0) || (x >= scrWd) || (y < 0) || (y >= scrHt * 8)) return;
  switch (c) {
    case PIXEL_OFF: scr[x + (y / 8)*scrWd] &= ~(1 << (y & 7)); break;
    case PIXEL_ON:  scr[x + (y / 8)*scrWd] |=  (1 << (y & 7)); break;
    case PIXEL_XOR: scr[x + (y / 8)*scrWd] ^=  (1 << (y & 7)); break;
  }
}

void drawLineV(int16_t x, int16_t y0, int16_t y1, uint16_t c)
{
  if (y1 > y0)
  {
    for (int y = y0; y <= y1; y++) drawPixel(x, y, c);
  }
  else
  {
    for (int y = y1; y <= y0; y++) drawPixel(x, y, c);
  }
}

// --------------------------------------------------------------------------

char buf[25],buf2[15];
float temp,mint=1000,maxt=-1000;
float minh=1000,maxh=-1000;

char *menuTxt[] = {"Volume", "Surround", "Bass", "Treble", "Tracks", "Backlight", "Contrast", "Reboot"};

void setup()
{
  BootKeyboard.begin();
  Serial.begin(9600);

  lcd.init();
  lcd.clrScr();

  for(int i=7; i<12; i++) pinMode(i, OUTPUT);
  //pinMode(13, OUTPUT);

  #ifdef USE_EEPROM
  mint=rdFloat(0);
  maxt=rdFloat(2);
  minh=rdFloat(4);
  maxh=rdFloat(6);
  #endif

  initEncoder();
  numMenus = sizeof(menuTxt) / sizeof(char *);

  lcd.setContrast(0x24);

  pinMode(N5110_BACKLIGHT, OUTPUT);
  analogWrite(N5110_BACKLIGHT,100);

  encoderPos = 0; 
  menuMode = -1; 
  menuStart = 0; 
}

void setBacklight()
{
  if (encoderPos > 84) encoderPos = 84;

  snprintf(buf, 6, " %d ", encoderPos * 255 / 84);

  lcd.setFont(Small5x7PL);
  lcd.setCharMinWd(5);
  lcd.setDigitMinWd(5);
  lcd.printStr(ALIGN_CENTER, 1, buf);
  lcd.printStr(ALIGN_LEFT, 1, "000");
  lcd.printStr(ALIGN_RIGHT, 1, "255");
  lcd.fillWin(0, 2, encoderPos, 1, 0xfc);

  if (encoderPos < 84) lcd.fillWin(encoderPos, 2, 84 - encoderPos, 1, 0);

  analogWrite(N5110_BACKLIGHT, 255 - encoderPos * 3);
}

void setContrast()
{
  if (encoderPos > 63) encoderPos = 63;

  snprintf(buf, 6, "%0d", encoderPos * 2);

  lcd.setFont(Small5x7PL);
  lcd.setCharMinWd(5);
  lcd.setDigitMinWd(5);
  lcd.printStr(28, 1, buf);
  lcd.printStr(ALIGN_LEFT, 1, "00");
  lcd.printStr(58, 1, "7F");
  lcd.fillWin(0, 2, encoderPos, 1, 0xfe);

  if (encoderPos < 84) lcd.fillWin(encoderPos, 2, 84 - encoderPos, 1, 0);

  lcd.setContrast(encoderPos * 2);
}

void (*doReset)(void) = 0;

void reboot()
{
  if(encoderPos >= 1*2) encoderPos = 1*2;
  
  int st = encoderPos/2;
  lcd.setFont(c64enh);
  lcd.printStr(ALIGN_CENTER, 1, "Reboot?");
  lcd.setInvert(st?0:1);
  lcd.printStr(10, 3, " NO ");
  lcd.setInvert(st?1:0);
  lcd.printStr(43, 3, " YES ");
  lcd.setInvert(0);
  
  if(readButton() <= 0) return;
  
  menuMode=-1;
  
  lcd.clrScr();
  if(st>0) { // yes
    lcd.printStr(ALIGN_CENTER, 2, "Rebooting ..."); delay(500);
    lcd.clrScr();
    doReset();
  }
  encoderPos=oldPos;
}

// set levels depending on what is required //
// store encoderPos
 
void setMenu(int x)
{
  oldPos = x;
  menuMode = x + menuStart;
  
  lcd.clrScr();
  
  switch(menuStart) // set default values
  {
    case 0: encoderPos = vol; break; // volume 10
    case 1: encoderPos = sur; break; // surround sound default
    case 2: encoderPos = bas; break; // bass 100
    case 3: encoderPos = tre; break; // treble 50
    case 4: encoderPos =   1; break; // tracks 1
    case 5: encoderPos = 250; break; // backlight 250
    case 6: encoderPos =  24; break; // contrast 24
    case 7: encoderPos =   1; break; // reboot no
  }
}
void endMenu()
{
  if (readButton() > 0)
  {
    switch(menuMode) // update vars with current encoder value
    {
      case 0: vol = encoderPos; break;
      case 1: sur = encoderPos; break;
      case 2: bas = encoderPos; break;
      case 3: tre = encoderPos; break;
    }
    menuMode = -1; // reset to default //
    lcd.clrScr();
    encoderPos = oldPos; // put back encoderPos //
  }
}

void formatMenu(char *in, char *out, int num)
{
  int j = strlen(in);
  out[0] = ' ';
  strncpy(out + 1, in, j++);
  for (; j < num; j++) out[j] = ' ';
  out[j] = 0;
}

void drawMenuSlider()
{
  int n = numScrLines*encoderPos/numMenus;
  lcd.fillWin(83,0,1,6,0);
  lcd.fillWin(82,0,1,6,0xff);
  lcd.fillWin(81,0,1,6,0);
  lcd.fillWin(81,n,1,1,B01111100);
  lcd.fillWin(83,n,1,1,B01111100);
  debugReport();
}

void setLevels(void)
{
  int cper = 0;

  switch(menuMode)
  {
    case 0: if(clicked) encoderPos = vol; cper = map(encoderPos*2, 0, 168, 0, 700); break;
    case 1: if(clicked) encoderPos = sur; cper = map(encoderPos*2, 0, 168, 0, 500); break;
    case 2: if(clicked) encoderPos = bas; cper = map(encoderPos*2, 0, 168, 0, 100); break;
    case 3: if(clicked) encoderPos = tre; cper = map(encoderPos*2, 0, 168, 0, 100); break;
    case 4: return;
  }

  if (encoderPos > 84) encoderPos = 84;
  if (encoderPos <  0) encoderPos = 0;
  
  snprintf(buf, 6, "%d", cper);

  lcd.setCharMinWd(5);
  lcd.setDigitMinWd(5);

  lcd.printStr(ALIGN_CENTER, 5, buf);
  lcd.fillWin(0, 2, encoderPos, 1, 0xfe);
  if (encoderPos < 84) lcd.fillWin(encoderPos, 2, 84 - encoderPos, 1, 0);
}

void handleMenu()
{
  lcd.setFont(Small5x7PL);
  lcd.setCharMinWd(5);
  lcd.setDigitMinWd(5);

  switch(menuMode)
  {
    case -1:  // draw menu, highlighted current line //      
              for (int i = 0; i < numScrLines; i++)
              {
                lcd.setInvert(i == encoderPos ? 1 : 0);
                formatMenu(menuTxt[i + menuStart], buf, 13);
                lcd.printStr(ALIGN_LEFT, i, buf);
              }
              if (readButton())
              {
                setMenu(encoderPos); // setMenu will compensate for menuStart
              }
              break;

    case 0:   lcd.setFont(c64enh); lcd.printStr(ALIGN_CENTER, 4, "       "); setLevels();  break; //volcurpos = encoderPos;
    case 1:   lcd.setFont(c64enh); lcd.printStr(ALIGN_CENTER, 4, "       "); setLevels();  break; //surcurpos = encoderPos;
    case 2:   lcd.setFont(c64enh); lcd.printStr(ALIGN_CENTER, 4, "       "); setLevels();  break; //bascurpos = encoderPos;
    case 3:   lcd.setFont(c64enh); lcd.printStr(ALIGN_CENTER, 4, "       "); setLevels();  break; //trecurpos = encoderPos;
    case 4:   lcd.setFont(c64enh); lcd.printStr(ALIGN_CENTER, 4, "       "); intracks = 1; break; 
    case 5:   setBacklight(); break;
    case 6:   setContrast(); break;
    case 7:   reboot(); break;
    default:  menuMode = -1; lcd.clrScr(); intracks = 0;
  }

  if(menuMode != -1)
  {
    endMenu();
  }
}

void loop()
{
  rotating = true;  // reset the debouncer
  handleMenu();
}

void doEncoderA() // Interrupt on A changing state
{
  if ( rotating ) delay (2);
  if ( digitalRead(encoderPinA) != A_set ) // debounce once more
  {
    A_set = !A_set;
    // adjust counter + if A leads B
    if ( A_set && !B_set ) rotationPos = 1;
    rotating = false;
  }
}

void doEncoderB()  // Interrupt on B changing state, same as A above
{
  if ( rotating ) delay (2);
  if ( digitalRead(encoderPinB) != B_set )
  {
    B_set = !B_set;
    //  adjust counter - 1 if B leads A
    if ( B_set && !A_set ) rotationPos = -1;
    rotating = false;
  }
}

/*  for reference: numMenus = 8, encoderPos = 0, menuStart = 0, numScrLines = 6, menuMode = -1 */
 
int readButton()
{
  static int lastState = HIGH;

  int val = 0, state = digitalRead(encoderButton);

  if(state == LOW && lastState == HIGH) val = 1;

  if(lastState == LOW && clicked == false)
  {
    clicked = true; rotationPos = 0;
  }

  if(lastState == HIGH && clicked) { clicked = false; delay(1); }

  lastState = state;

  if(rotationPos == -1) // left rotation
  {
    if(menuMode == -1)
    {
      if(encoderPos > 0)
      {
        encoderPos--;
      }
      else if(encoderPos == 0 && menuStart > 0)
      {
        menuStart--;
      }
    }
    else
    {
      encoderPos--;
    }
    process_encoder(0);
    debugReport();
    rotationPos = 0;
  }
  if(rotationPos == 1) // right rotation
  {
    if(menuMode == -1)
    {
      if(encoderPos < 5) 
      {
        encoderPos++;
      }
      else if(encoderPos == 5 && menuStart < 3 && encoderPos + menuStart < numMenus -1)
      { 
        menuStart++;
      }
    }
    else
    {
      encoderPos++;    
    }
    process_encoder(1);
    debugReport();    
    rotationPos = 0;
  }
  return val;
}

void debugReport(void)
{
  if(rotationPos == 1)
  {
    Serial.print("Right: ");
  }
  else if(rotationPos == -1)
  {
    Serial.print("Left:  ");
  }

    Serial.print("encoderPos [");
    Serial.print(encoderPos);
    Serial.print("] menuMode [");
    Serial.print(menuMode);
    Serial.print("] menuStart [");
    Serial.print(menuStart);

    switch(menuMode)
    {
      case 0:
          Serial.print("] vol = [");
          Serial.print(vol);
      break;
      case 1:
          Serial.print("] sur = [");
          Serial.print(sur);
      break;
      case 2:
          Serial.print("] bas = [");
          Serial.print(bas);
      break;
      case 3:
          Serial.print("] tre = [");
          Serial.print(tre);
      break;
    }
    Serial.print("] M[");
    Serial.print(numMenus);
    Serial.print("] R [ ");
    Serial.print(rotationPos);
    Serial.print("] Src[");    
    Serial.print(numScrLines - 1);
    Serial.print("] mS[");    
    Serial.print(menuStart);
    Serial.println("]");    
}

void process_encoder(int k)
{
  // currently only the first 5 menu items generate key codes, the rest are to manage the LCD //
  
  switch(menuMode)
  {
    case 0: if(k == 1) BootKeyboard.press('.'); else BootKeyboard.press(','); break; // volume
    case 1: if(k == 1) BootKeyboard.press(0x2827); else BootKeyboard.press(0x273b); break; // surround
    case 2: if(k == 1) BootKeyboard.press(0x2822); else BootKeyboard.press(0x273a); break; // bass
    case 3: if(k == 1) BootKeyboard.press(0x1b7d); else BootKeyboard.press(0x1a7b); break; //treble
    case 4: if(k == 1) BootKeyboard.press(KEY_DOWN_ARROW); else BootKeyboard.press(KEY_UP_ARROW); break;
    default: break;
  }
  BootKeyboard.releaseAll();
}

void tracks(void)
{
  if(intracks == 1 && readButton() != 0)
  {
    BootKeyboard.press(KEY_RETURN); BootKeyboard.releaseAll();
    menuMode = 4; lcd.clrScr(); intracks = 0;
    intracks = 0;
    setMenu(4);
  }
  else
  {
    setMenu(4);
    lcd.setFont(c64enh); lcd.printStr(ALIGN_CENTER, 4, "       ");
    lcd.printStr(ALIGN_CENTER, 4, "Press to Select");
    intracks = 0;
  }
}
