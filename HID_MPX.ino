/*
    Using Rotary Encoder for HID (keyboard) related application such as Macro Keyboard etc...
    As serial may not be immediately available while testing HID related code as is the case with
    the Pro Micro. Each upload can result in delays before you can activate the console...

    As a lot of applications employ some type of display, why not use that display while building
    the project to act as the debug console. This code employs the Nokia 5110 in this capacity as I often use it in
    the same applications...
   
    Code inspired by Pawel A. Hernik (Menu) & Simon Merrett (Int). Fairly simple so few comments. Use it as the basis
    of any HID related ProMicro build. It supports: Rotation Left/Right, Button Press, Button Release and Button Long Press.
    
    Michael O'Toole (c) 2020
*/

#include "HID-Project.h"
#include "N5110_SPI.h"
#include "small5x7_font.h"

#define N5110_RST        9
#define N5110_CS        10
#define N5110_DC         8
#define N5110_BACKLIGHT  6

static byte pinA = 2;
static byte pinB = 3;
static byte pinS = 4;
static byte oldAction;
static byte newAction;

volatile byte aMask  = 0;       // 
volatile byte bMask  = 0;       //
volatile byte action = 0;       // Rotary Encoder Action: 0 = none, 1 = left rotation, 2 = right rotation, 3 = button press/down, 4 = button release/up, 5 = long press.

volatile int encoderPos  = 0;
volatile byte oldEncPos  = 0;
volatile byte reading    = 0;

unsigned long currentTime;
unsigned long sPushTime;
unsigned long releaseTime;
unsigned long downTime = 0;
unsigned long longPressTime = 1000;

byte oldButtonState = HIGH;

bool longPress = false;

const unsigned long debounceTime = 10;

byte numMenus = 8;
byte menuStart = 0;
byte numScrLines = 6;

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
    //Serial.begin(115200); // uncomment if required ;)
    initEncoder();
    initDisplay();
    encoderPos = 0;
    lcd.setFont(Small5x7PL);
    lcd.setCharMinWd(5);
    lcd.setDigitMinWd(5);
    lcd.setContrast(47);
    lcd.printStr(ALIGN_CENTER, 0, "12345678901234567890"); // check width of display //
}

int timer = 0;

void loop()
{
    lcd.setContrast(32);
    processLoop();
}

void processLoop(void)
{
    getButtonAction();
    
    if(encoderPos > numMenus) encoderPos = 0;
    if(encoderPos < 0) encoderPos = numMenus;
    
    if(oldAction != action)
    {
        oldAction = action;
        newAction = true;
    }
    else
    {
      newAction = false;
    }

    if(oldEncPos != encoderPos)
    {
        oldEncPos = encoderPos;
        lcdClearLine(0);
        lcd.printStr(ALIGN_CENTER, 0, itoa(encoderPos, lineBuffer, 10));
    }

    lcdClearLine(5);
    switch(action)
    {
      case 0:
              lcd.printStr(ALIGN_CENTER, 5, "None");
      break;
      case 1:
              lcd.printStr(ALIGN_CENTER, 5, "Left Rotate");
      break;
      case 2:
              lcd.printStr(ALIGN_CENTER, 5, "Right Rotate");
      break;
      case 3:
              lcd.printStr(ALIGN_CENTER, 5, "Button Down");
      break;
      case 4:
              lcd.printStr(ALIGN_CENTER, 5, "Button Up");
      break;
      case 5:
              lcd.printStr(ALIGN_CENTER, 5, "Long Press");
      break;
    }
}


// Rotary encoder interrupt service routines (Pro Micro) //

/*  Standard interrupt routine, disable interrupts, read port masked for the bits we are interested in,
    compare result, take appropriate action and re-enable interrupts again
*/

void PinA()
{
    cli();
    //printBinary(PIND); // uncomment to print and ensure you have the right port as it differs with processor //
    reading = PIND & 0x3;
    //printBinary(reading); // ditto
    if(reading == B00000011 & aMask)
    {
        encoderPos ++;
        bMask = aMask = 0;
        action = 2;        
    }
    else if (reading == B00000001) bMask = 1;
    sei();
}

void PinB()
{
    cli();
    //printBinary(PIND); // uncomment to print and ensure you have the right port as it differs with processor //
    reading = PIND & 0x3;
    //printBinary(reading); // ditto
    if (reading == B00000011 && bMask)
    {
        encoderPos --;
        bMask = aMask = 0;
        action = 1;        
    }
    else if (reading == B00000010) aMask = 1;
    sei();
}

// to print binary use //
void printBinary(byte inByte)
{
    Serial.print("[");
    for (int b = 7; b >= 0; b--)
    {
        Serial.print(bitRead(inByte, b));
    }
    Serial.println("]");
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
