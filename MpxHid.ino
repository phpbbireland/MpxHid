/*
    Original Source...

    (c)2018 Pawel A. Hernik
    YouTube video: https://youtu.be/GHULqZpVpz4
    
    Changes and additions by Michaelo
*/

#include <avr/sleep.h>
#include <avr/wdt.h>
#include <EEPROM.h>
#include "HID-Project.h"

#include "N5110_SPI.h"
#include "c64enh_font.h"
#include "term9x14_font.h"
#include "small4x7_font.h"
#include "small5x7_font.h"
#include "small5x7bold_font.h"

#define N5110_RST       9
#define N5110_CS        10
#define N5110_DC        8
#define N5110_BACKLIGHT 6
#define PIXEL_OFF 0
#define PIXEL_ON  1
#define PIXEL_XOR 2

// Display 48*M_Max_Cols pixels //

#define M_Max_Cols 84
#define M_Max_Rows 6
#define Default_Menu -1
#define Rotate_Left -1
#define Rotate_Right 1

N5110_SPI lcd(N5110_RST, N5110_CS, N5110_DC); // RST,CS,DC

enum PinAssignments
{
  encoderPinA   = 2,   // right
  encoderPinB   = 3,   // left
  encoderButton = 4    // button
};

volatile int encoderPos = 0;
volatile int rotationPos = 0;       // can be 0, -1, 1
//volatile int Selecting_Track = 0;

static unsigned int vol =  9;
static unsigned int sur = 99;
static unsigned int bas = 50;
static unsigned int tre = 50;

static boolean rotating = false;    // debounce management

boolean A_set = false;
boolean B_set = false;
boolean clicked = false;
boolean Selecting_Track = false;

byte scr[M_Max_Cols * M_Max_Rows]; // frame buffer
byte scrWd = M_Max_Cols;
byte scrHt = M_Max_Rows;


char buf[25],buf2[15];
char *Menu_Names[] = {"Volume", "Surround", "Bass", "Treble", "Tracks", "Backlight", "Contrast", "Reboot"};

float temp,mint=1000,maxt=-1000;
float minh=1000,maxh=-1000;

int m_Current_Line = 0;   // current line
int m_Top_Line = 0;       // global storage for top menu item to draw
int m_Items_Per_Page = 6; // menus per page to draw (depends on font size)
int m_Total_Items = 8;    // total menus items (depends on font size)
int m_Current_Mode = -1;  // global storage current menu mode

int oldPos = 0;


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

void readEncoderInt()
{
  (digitalRead(encoderPinA) == digitalRead(encoderPinB)) ? encoderPos++ : encoderPos--;
}

void clrBuf()
{
  for (int i = 0; i < scrWd * scrHt; i++) scr[i] = 0;
}

void drawPixel(int16_t x, int16_t y, uint16_t c)
{
  if ((x < 0) || (x >= scrWd) || (y < 0) || (y >= scrHt * 8)) return;

  switch (c)
  {
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

void setup()
{
  BootKeyboard.begin();
  Serial.begin(112500);

  lcd.init();
  lcd.clrScr();

  encoderPos=0;

  for(int i=7; i<12; i++) pinMode(i, OUTPUT);

  initEncoder();

  m_Total_Items = sizeof(Menu_Names) / sizeof(char *);
  m_Current_Line = 0;
  encoderPos = 0;

  lcd.setContrast(0x24);

  pinMode(N5110_BACKLIGHT, OUTPUT);
  analogWrite(N5110_BACKLIGHT,100);

  debugReport();
}

void setBacklight()
{
  if (encoderPos > M_Max_Cols) encoderPos = M_Max_Cols;

  snprintf(buf, 6, " %d ", encoderPos * 255 / M_Max_Cols);

  lcd.setFont(Small5x7PL);
  lcd.setCharMinWd(5);
  lcd.setDigitMinWd(5);
  lcd.printStr(ALIGN_CENTER, 1, buf);
  lcd.printStr(ALIGN_LEFT, 1, "000");
  lcd.printStr(ALIGN_RIGHT, 1, "255");
  lcd.fillWin(0, 2, encoderPos, 1, 0xfc);

  if (encoderPos < M_Max_Cols) lcd.fillWin(encoderPos, 2, M_Max_Cols - encoderPos, 1, 0);

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

  if (encoderPos < M_Max_Cols) lcd.fillWin(encoderPos, 2, M_Max_Cols - encoderPos, 1, 0);

  lcd.setContrast(encoderPos * 2);
}

void setMenu(int m)
{
  m_Current_Mode = m;
  lcd.clrScr();
  oldPos = encoderPos;
  encoderPos = 0;
}

void endMenu()
{
  if (readButton() > 0)
  {
    switch(m_Current_Mode) // update vars with current encoder value
    {
      case 0: vol = encoderPos; break;
      case 1: sur = encoderPos; break;
      case 2: bas = encoderPos; break;
      case 3: tre = encoderPos; break;
    }

    m_Current_Mode = -1;
    lcd.clrScr();
    encoderPos = oldPos;
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
  int n = m_Items_Per_Page * m_Current_Line / m_Total_Items;
  lcd.fillWin(83,0,1,6,0);
  lcd.fillWin(82,0,1,6,0xff);
  lcd.fillWin(81,0,1,6,0);
  lcd.fillWin(81,n,1,1,B01111100);
  lcd.fillWin(83,n,1,1,B01111100);
}

void setLevels(void)
{
  int cper = 0;

  switch(m_Current_Mode)
  {
    case 0: if(clicked) encoderPos = vol; cper = map(encoderPos*2, 0, 168, 0, 700); break;
    case 1: if(clicked) encoderPos = sur; cper = map(encoderPos*2, 0, 168, 0, 500); break;
    case 2: if(clicked) encoderPos = bas; cper = map(encoderPos*2, 0, 168, 0, 100); break;
    case 3: if(clicked) encoderPos = tre; cper = map(encoderPos*2, 0, 168, 0, 100); break;
    case 4: return;
    case 5: break;
    case 6: break;
    case 7: break;
  }

  if (encoderPos > M_Max_Cols) encoderPos = M_Max_Cols;
  if (encoderPos <  0) encoderPos = 0;

  snprintf(buf, 6, "%d", cper);

  lcd.setCharMinWd(5);
  lcd.setDigitMinWd(5);

  lcd.printStr(ALIGN_CENTER, 5, buf);
  lcd.fillWin(0, 2, encoderPos, 1, 0xfe);
  if (encoderPos < M_Max_Cols) lcd.fillWin(encoderPos, 2, M_Max_Cols - encoderPos, 1, 0);
}

void Process_Tracks(void)
{
  if(readButton() != 0)
  {
    BootKeyboard.press(KEY_RETURN);
    BootKeyboard.releaseAll();
    Selecting_Track = false;
  }
  else
  {
    Selecting_Track = true;
    lcd.setCharMinWd(5);
    lcd.setDigitMinWd(5);
    lcd.printStr(ALIGN_CENTER, 5, "TRACKS");
  }
}

void handleMenu()
{
  lcd.setFont(Small5x7PL);
  lcd.setCharMinWd(5);
  lcd.setDigitMinWd(5);

  if (encoderPos < 0) encoderPos = 0;

  switch(m_Current_Mode)
  {
    case -1:  // default, no mode selected //

              for (int i = 0; i < m_Items_Per_Page; i++)
              {
                if (i + m_Top_Line < m_Total_Items)
                {
                  lcd.setInvert(i + m_Top_Line == m_Current_Line ? 1 : 0);
                  formatMenu(Menu_Names[i + m_Top_Line], buf, 13);
                  lcd.printStr(ALIGN_LEFT, i, buf);
                }
              }

              drawMenuSlider();

              if (readButton())
              {
                setMenu(m_Current_Line);
              }
              break;

    case 0:   lcd.setFont(c64enh); lcd.printStr(ALIGN_CENTER, 4, "       "); setLevels(); break; //volcurpos = encoderPos;
    case 1:   lcd.setFont(c64enh); lcd.printStr(ALIGN_CENTER, 4, "       "); setLevels(); break; //surcurpos = encoderPos;
    case 2:   lcd.setFont(c64enh); lcd.printStr(ALIGN_CENTER, 4, "       "); setLevels(); break; //bascurpos = encoderPos;
    case 3:   lcd.setFont(c64enh); lcd.printStr(ALIGN_CENTER, 4, "       "); setLevels(); break; //trecurpos = encoderPos;
    case 4:   Process_Tracks(); break;
    case 5:   setBacklight(); break;
    case 6:   setContrast(); break;
    case 7:   break;
    case 8:   break;
  }
  if(m_Current_Mode != -1)
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
  if ( rotating ) delay (1);

  if ( digitalRead(encoderPinA) != A_set ) // debounce once more
  {
    A_set = !A_set;

    if ( A_set && !B_set ) rotationPos = 1; // adjust counter + if A leads B

    rotating = false;
  }
}

void doEncoderB()  // Interrupt on B changing state, same as A above
{
  if ( rotating ) delay (1);

  if ( digitalRead(encoderPinB) != B_set )
  {
    B_set = !B_set;

    if ( B_set && !A_set ) rotationPos = -1; //  adjust counter - 1 if B leads A

    rotating = false;
  }
}

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

  if(rotationPos == Rotate_Left)
  {
    encoderPos--;

    if(m_Current_Mode  == Default_Menu)
    {
      if(encoderPos == -1)
      {
        if(m_Top_Line > 0)
        {
          m_Top_Line--; m_Current_Line--; //encoderPos--;
        }else encoderPos = m_Current_Line = m_Items_Per_Page - 1;
      }
      else m_Current_Line = encoderPos;
    }
    process_encoder(0);
    debugReport();
    rotationPos = 0;
  }

  if(rotationPos == Rotate_Right)
  {
    encoderPos++;

    if(m_Current_Mode  == Default_Menu)
    {
      if(encoderPos >= m_Items_Per_Page)
      {
        if(m_Top_Line < 2)
        {
          m_Top_Line++; m_Current_Line++; //encoderPos++;
        }else encoderPos = m_Current_Line = 0;
      }
      else m_Current_Line = encoderPos;
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
    Serial.print("] m_Current_Mode [");
    Serial.print(m_Current_Mode);
    Serial.print("] m_Current_Line [");
    Serial.print(m_Current_Line);
    Serial.print("] m_Top_Line [");
    Serial.print(m_Top_Line);

    switch(m_Current_Mode)
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

    Serial.println("] ");
}

void process_encoder(int k)
{
  switch(m_Current_Mode)
  {
    case 0: if(k == 1) BootKeyboard.press('.'); else BootKeyboard.press(','); break; // volume
    case 1: if(k == 1) BootKeyboard.press(0x2827); else BootKeyboard.press(0x273b); break; // surround
    case 2: if(k == 1) BootKeyboard.press(0x2822); else BootKeyboard.press(0x273a); break; // bass
    case 3: if(k == 1) BootKeyboard.press(0x1b7d); else BootKeyboard.press(0x1a7b); break; //treble
    case 4: if(k == 1) BootKeyboard.press(KEY_DOWN_ARROW); else BootKeyboard.press(KEY_UP_ARROW); break;
  }
  BootKeyboard.releaseAll();
}
