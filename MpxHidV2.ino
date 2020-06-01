/*
    Using Rotary Encoder for HID related application such as Macro Keyboard etc...
    Serial may not be immediately available while testing HID related code as is the case with
    the Pro Micro. Each upload can result in delays before you can activate the console...

    As a lot of applications employ some type of display, why not use that display while building
    the pproject to act as the debug console...

    This code employs the Nokia 5110 in this capacity as I often use it in the same applications...
    Michael O'Toole (c) 2020

    The following code was inspired by Pawel A. Hernik & Simon Merrett.

    Supports: Rotation Left/Right, Button Press, Button Release and Button Long Press...
*/

#include "HID-Project.h"
#include "N5110_SPI.h"
#include "small5x7_font.h"

#define N5110_RST        9
#define N5110_CS        10
#define N5110_DC         8
#define N5110_BACKLIGHT  6

#define NONE  0
#define LEFT  1
#define RIGHT 2
#define ENTER 3
#define UP    4
#define LONG  5 


static byte pinA = 2;
static byte pinB = 3;
static byte pinS = 4;
static byte oldAction = 0;
static byte newAction = 0;
static bool newEvent = false;
static bool etoggle = false;
static bool mtoggle = false;
static bool process = false;
volatile byte aMask  = 0;
volatile byte bMask  = 0;
volatile byte intracks = 0;
volatile byte reading   = 0;

volatile byte action = 0;        // Rotary Encoder Action: 0 = none, 1 = left rotation, 2 = right rotation, 3 = button press/down, 4 = button release/up, 5 = long press.
volatile byte actionchange = 0;

volatile int encoderxPos = 0;
volatile int encoderyPos = 0;
volatile byte oldEncyPos = 0;

unsigned long currentTime;
unsigned long sPushTime;
unsigned long releaseTime;
unsigned long downTime = 0;
unsigned long longPressTime = 1000;

byte oldButtonState = HIGH;

bool longPress = false;
bool inSide = false;
const unsigned long debounceTime = 10;

byte buttonState;

/* menu vars */
byte numMenus = 7;
byte menuStart = 0;
byte numScrLines = 6;
int oldPos = 0;
int menuMode = 0;

volatile byte currentPage = 0;

byte scr[84 * 6]; // buffer
byte scrWd = 84;  // width
byte scrHt = 6;   // height

static unsigned int vol = 10;
static unsigned int sur = 99;
static unsigned int bas = 50;
static unsigned int tre = 50;

char *menuTxt[] = {"Volume", "Surround", "Bass", "Treble", "Tracks", "Backlight", "Contrast"};
char buf[50];

/*
const byte UpChar[] PROGMEM = {
  B00100,
  B01110,
  B11111,
  B00100,
  B00100,
  B00100,
  B00100,
  B00100
};
const byte DnChar[] PROGMEM = {
  B00100,
  B00100,
  B00100,
  B00100,
  B00100,
  B11111,
  B01110,
  B00100
};
*/

/* menu vars */

static byte lastTrack = 0;

N5110_SPI lcd(N5110_RST, N5110_CS, N5110_DC); // RST,CS,DC

char lineBuffer[50];

void initEncoder()
{
    pinMode(pinA, INPUT_PULLUP);
    pinMode(pinB, INPUT_PULLUP);
    pinMode(pinS, INPUT_PULLUP);

    attachInterrupt(0, PinA, RISING);
    attachInterrupt(1, PinB, RISING);
}

void initDisplay(void)
{
    lcd.init();
    lcd.clrScr();
    lcd.setFont(Small5x7PL);
    pinMode(N5110_BACKLIGHT, OUTPUT);
    analogWrite(N5110_BACKLIGHT,100);
}

void setup()
{
    BootKeyboard.begin();
    Serial.begin(115200); // uncomment if required ;)
    initEncoder();
    initDisplay();
    lcd.setFont(Small5x7PL);
    lcd.setCharMinWd(5);
    lcd.setDigitMinWd(5);
    lcd.setContrast(32);
    lcd.printStr(ALIGN_CENTER, 0, "12345678901234567890");
}

void loop()
{
    lcd.setContrast(32);
    processLoop();
}

void processLoop(void)
{
    getButtonAction();

    if(oldAction != action) { oldAction = action; newAction = true; } else { newAction = false; }
    if(newAction && action == ENTER) { etoggle = !etoggle;}
   
    if(!etoggle && !mtoggle)
    {
      paintMenu();
    }

    else if(etoggle)
    {
      mtoggle = !mtoggle;
      lcd.setContrast(35);
      oldEncyPos = encoderxPos;
      lcd.setCharMinWd(5);
      lcd.setDigitMinWd(5);
       
       switch(encoderxPos+menuStart)
       {
        case 0:
        case 1:
        case 2:
        case 3: setLevels(encoderxPos + menuStart + 1); break;
        case 4: playTrack(lastTrack); break;
        case 5: setBacklight(); break;
        case 6: setContrast(); break;
       }
       
       encoderxPos = oldEncyPos;
    }

    if(newAction  || (!newAction && action == 3) || (!newAction && action == 5))
    {
      debugReport();
    }    

    switch(action)
    {
      case 0: break;
      
      case 1: if(!etoggle)
              {
                if(encoderxPos == 0 && menuStart > 0) { menuStart--; lcd.clrScr(); paintMenu(); }
                if(encoderxPos < 0) { encoderxPos = 0; }
              }
              break;
              
      case 2: if(!etoggle)
              {
                if(encoderxPos == 6 && menuStart < 1) { menuStart++; lcd.clrScr(); paintMenu(); }
                if(encoderxPos > 5) { encoderxPos = 5; }
              }
              break;
              
      case 3: break;
      case 4: break;
      case 5: break; encoderxPos = 0; menuStart = 0; lcd.setContrast(35); break;
    }
}

void paintMenu()
{
    for (int i = 0; i < numScrLines; i++)
    {
        lcd.setInvert(i == encoderxPos ? 1 : 0);
        formatMenu(menuTxt[i + menuStart], buf, 16);
        lcd.printStr(ALIGN_LEFT, i, buf);
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

void setLevels(int input)
{
  mtoggle = 1;
  int cper = 0;
  if(mtoggle) lcd.clrScr();

  switch(input)
  {
    case 1: encoderxPos = vol; cper = map(encoderxPos*2, 0, 168, 0, 700); lcd.printStr(ALIGN_CENTER, 0, "Volume"); lcd.printStr(ALIGN_RIGHT, 0, itoa(encoderxPos, lineBuffer, 10));break;
    case 2: encoderxPos = sur; cper = map(encoderxPos*2, 0, 168, 0, 500); lcd.printStr(ALIGN_CENTER, 0, "Surround"); lcd.printStr(ALIGN_RIGHT, 0, itoa(encoderxPos, lineBuffer, 10));break;
    case 3: encoderxPos = bas; cper = map(encoderxPos*2, 0, 168, 0, 100); lcd.printStr(ALIGN_CENTER, 0, "Bass"); lcd.printStr(ALIGN_RIGHT, 1, itoa(encoderxPos, lineBuffer, 10));break;
    case 4: encoderxPos = tre; cper = map(encoderxPos*2, 0, 168, 0, 100); lcd.printStr(ALIGN_CENTER, 0, "Treble"); lcd.printStr(ALIGN_RIGHT, 1, itoa(encoderxPos, lineBuffer, 10));break;
  }
  action = 0;
  while(action < 3)
  {
    getButtonAction();
    if(action == 3 || action == 5) break;
    
    if (encoderxPos > 84) encoderxPos = 84; if (encoderxPos <  0) encoderxPos = 0;
    
    snprintf(buf, 6, "%d", cper);
    lcd.fillWin(0, 2, encoderxPos, 1, 0xfe);
    
    if (encoderxPos < 84) lcd.fillWin(encoderxPos, 2, 84 - encoderxPos, 1, 0);
    if(action == 1 || action == 2) { sendKey(input); }
  }
  mtoggle = 0;
}

void playTrack(int input)
{
  mtoggle = 1;
  if(mtoggle) lcd.clrScr();
  
  encoderxPos = lastTrack; 

  if(encoderxPos <  0) encoderxPos = 0;
  if(encoderxPos > 99) encoderxPos = 99;

  lcd.setContrast(52);
  lcd.printStr(ALIGN_CENTER, 0, "Select");
  lcd.printStr(ALIGN_CENTER, 1, "Track");  
  
  action = 0;
  while(action < 3)
  {
    getButtonAction();
    
    if(action == 1)
    {
      lcdClearLine(3); lcd.printStr(ALIGN_CENTER, 3, "UP"); sendKey(5);
    }
    else if (action == 2)
    {
      lcdClearLine(3); lcd.printStr(ALIGN_CENTER, 3, "DOWN"); sendKey(5);
    }
    if(action == 3 || action == 5) break;
  }
  
  lastTrack = encoderxPos ;
  mtoggle = 0;
}

void setBacklight()
{
  mtoggle = 1;
  if(mtoggle) lcd.clrScr();
  lcd.setContrast(32);

  while(action != 3)
  {
    if (encoderxPos > 84) encoderxPos = 84;
    if (encoderxPos <  0) encoderxPos = 0;
    
    int cper = map(encoderxPos, 0, 84, 0, 100);
    
    snprintf(buf, 6, " %d%c ", cper, '%');
    lcd.printStr(ALIGN_CENTER, 4
    +, buf);
    lcd.fillWin(0, 2, encoderxPos, 1, 0xfe);
    
    analogWrite(N5110_BACKLIGHT, 255 - cper);

    getButtonAction();
    if(action == 3 || action == 5) break;
  }
  mtoggle = 0;
}

void setContrast()
{
  static bool start = false;
  mtoggle = 1;
  if(mtoggle) lcd.clrScr();

  while(action != 3)
  {
    getButtonAction();
    if(start == false) // set mid position starting //
    {
      start = true; encoderxPos = 39;
    }
    // usable contrast for this display, depends on speed of loop among other things //
    if (encoderxPos > 51) encoderxPos = 51;
    if (encoderxPos < 27) encoderxPos = 27;

    lcd.printStr(ALIGN_CENTER, 2, "Adjust");
    lcd.printStr(ALIGN_CENTER, 3, "Contrast");
    lcd.setContrast(encoderxPos);
    if(action == 3 || action == 5) break;
  }
  mtoggle = 0;
}


  void debugReport(void)
  {
    if(action == 2)
    {
      Serial.print("Right: ");
    }
    else if(action == 1)
    {
      Serial.print("Left:  ");
    }

      Serial.print("encoderxPos [");
      Serial.print(encoderxPos);
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
      Serial.print("] Menus[");
      Serial.print(numMenus);
      Serial.print("] Action [ ");
      Serial.print(action);
      Serial.print("] NewAction [ ");
      Serial.print(newAction);
      Serial.print("] Src[");
      Serial.print(numScrLines - 1);
      Serial.print("] mStart[");
      Serial.print(menuStart);
      Serial.print("] tog[");
      Serial.print(etoggle);
      Serial.print("] page[");
      Serial.print(currentPage);
      Serial.println("]");
  }



// Rotary encoder interrupt service routines (Pro Micro) //
void PinA()
{
    cli();
    //printBinary(PIND);
    reading = PIND & 0x3;
    //printBinary(reading);
    if(reading == B00000011 & aMask)
    {
        encoderxPos ++;
        bMask = aMask = 0;
        action = 2;
    }
    else if (reading == B00000001) bMask = 1;
    sei();
}

void PinB()
{
    cli();
    //printBinary(PIND);
    reading = PIND & 0x3;
    //printBinary(reading);
    if (reading == B00000011 && bMask)
    {
        encoderxPos --;
        bMask = aMask = 0;
        action = 1;
    }
    else if (reading == B00000010) aMask = 1;
    sei();
}

void printBinary(byte inByte) // to print binary //
{
    Serial.print("[");
    for (int b = 7; b >= 0; b--)
    {
        Serial.print(bitRead(inByte, b));
    }
    Serial.println("]");
}

void setContrast(int con)
{
    lcd.setContrast(con);
    lcd.printStr(ALIGN_CENTER, 2, itoa(con, lineBuffer, 10));
}

void lcdClearLine(int line)
{
    if(newAction) lcd.printStr(ALIGN_LEFT, line,"              ");
}

void getButtonAction()
{
    buttonState = digitalRead (pinS);

    if(oldButtonState != buttonState) // state change //
    {
        oldButtonState = buttonState;
        
        if(buttonState == HIGH)
        {
          releaseTime = millis();
          action = 4; 
        }
        else if(buttonState == LOW)
        {
          sPushTime = millis();
        }
        //downTime = sPushTime = millis();

        if(sPushTime + debounceTime > millis())
        {
           action = 3;
        }
    }
    else if(buttonState == LOW && releaseTime < sPushTime) // no state change + button pressed for long time //
    {
        downTime = sPushTime + (millis() - sPushTime);
        
        if(downTime > sPushTime + longPressTime)
        {
          longPress = true; action = 5;
        }
        else { longPress = false; }
    }
}

void sendKey(int input)
{
  switch(input)
  {
    case 1: if(action == 1) BootKeyboard.press(46); else BootKeyboard.press(44); break; // volume , .
    case 2: if(action == 1) BootKeyboard.press(39); else BootKeyboard.press(59); break; // surround ; '
    case 3: if(action == 1) BootKeyboard.press(34); else BootKeyboard.press(58); break; // bass : @
    case 4: if(action == 1) BootKeyboard.press(125); else BootKeyboard.press(123); break; //treble { }
    case 5: if(action == 1) BootKeyboard.press(KEY_DOWN_ARROW); else BootKeyboard.press(KEY_UP_ARROW); break;
    default: break;
  }
  BootKeyboard.releaseAll();
  action = 0;
}
