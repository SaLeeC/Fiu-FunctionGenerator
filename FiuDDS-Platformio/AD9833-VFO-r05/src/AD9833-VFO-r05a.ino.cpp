# 1 "C:\\Users\\ProBook\\AppData\\Local\\Temp\\tmprlt8p7r7"
#include <Arduino.h>
# 1 "I:/Dropbox/Arduino/Generatori Segnale/AD9833/Fiu-FunctionGenerator/FiuDDS-Platformio/AD9833-VFO-r05/src/AD9833-VFO-r05a.ino"
#include<Arduino.h>
#include<Wire.h>
# 12 "I:/Dropbox/Arduino/Generatori Segnale/AD9833/Fiu-FunctionGenerator/FiuDDS-Platformio/AD9833-VFO-r05/src/AD9833-VFO-r05a.ino"
#define DebugLevel 1




#define Modello "Fiu"
#define Modello1 "VFO"
#define Modello2 "FG DDS"
#define Hardware 2
#define Software 5

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <MD_AD9833.h>
#include <SimpleRotary.h>



#include <DigitLed72xx.h>






#define AD9833DATA 11
#define AD9833CLK 13
#define AD9833FSYNC 12



MD_AD9833 AD(AD9833DATA, AD9833CLK, AD9833FSYNC);
MD_AD9833::channel_t chan;

const int wSine = 0b0000000000000000;
const int wTriangle = 0b0000000000000010;
const int wSquare = 0b0000000000101000;





#define FreqL 0
#define FreqH 1
#define FreqNow 2

float Frequency[3][2] = {1,1,1,
                        1,1,1};




#define Generator0 0
#define Generator1 1
uint8_t CurrentGenerator=Generator0;

#define WaveTypeNumber 3


#define Sine 0
#define Triangle 1
#define Square 2
int8_t FrequencyWaveCurrentType[2] = {Sine,Sine};


uint8_t FrequencyWaveType[WaveTypeNumber] = {0b0000000000000000,
                                              0b0000000000000010,
                                              0b0000000000101000};




uint32_t FrequencyLimit[WaveTypeNumber][2] = { 1, 5000000,
                                                1, 5000000,
                                                1, 12500000};
# 101 "I:/Dropbox/Arduino/Generatori Segnale/AD9833/Fiu-FunctionGenerator/FiuDDS-Platformio/AD9833-VFO-r05/src/AD9833-VFO-r05a.ino"
#define GlobalModeNumber 5
#define GlobalModeFixFrequency 0
#define GlobalModeSweepLtH 1
#define GlobalModeSweepHtL 2
#define GlobalModeSweepLtHtL 3
#define GlobalModeModulation 4
int8_t GlobalMode = GlobalModeFixFrequency;
String GlobalModeLabel[GlobalModeNumber]={"Fixed Frequency",
                                          "Sweep L > H",
                                          "Sweep H > L",
                                          "Sweep L > H > L",
                                          "Modulation"};

uint16_t SweepTime=1;
int8_t SweepTimeStep=0;


float TZero;
float TXs;

float SweepTStep;
float FSweepStep;
float SweepCurrentFrequency;





#define DisplayNumber 2
#define Digit4Display 8
#define FirstDigitDisplay1 8


#define FDisplayNumDigit 8
#define MAX7219CS 2
#define MAX7219CS3 3




DigitLed72xx FrequencyDisplay = DigitLed72xx(MAX7219CS, DisplayNumber);


const uint8_t FHello[16] PROGMEM = {B00000000,
                                    B01011110,
                                    B01000111,
                                    B00000000,
                                    B01011011,
                                    B00111101,
                                    B00111101,
                                    B00000000,
                                    B00000000,
                                    B00000000,
                                    B00011100,
                                    B00000100,
                                    B01000111,
                                    B00000000,
                                    B00000000,
                                    B00000000};
# 171 "I:/Dropbox/Arduino/Generatori Segnale/AD9833/Fiu-FunctionGenerator/FiuDDS-Platformio/AD9833-VFO-r05/src/AD9833-VFO-r05a.ino"
#define TFT_CS 4
#define TFT_DC 5
#define TFT_RST 6

char SweepTimeBuff[9];



Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
# 192 "I:/Dropbox/Arduino/Generatori Segnale/AD9833/Fiu-FunctionGenerator/FiuDDS-Platformio/AD9833-VFO-r05/src/AD9833-VFO-r05a.ino"
#define FERotaryA 7
#define FERotaryB 8
#define FERotaryButton 14
SimpleRotary FrequencyRotary(FERotaryA,FERotaryB,FERotaryButton);

uint8_t FrequencyRotaryState;
uint8_t FrequencyRotaryStep[2] = {0, 0};
float FrequencyRotaryStepHerz[2];





#define MERotaryA 9
#define MERotaryB 10
#define MERotaryButton 15
SimpleRotary ModeRotary(MERotaryA,MERotaryB,MERotaryButton);

uint8_t ModeRotaryState;

#define NumOfMode 2
char *ModeLabel[] = {"Forma d'Onda", "Sweep"};





#define PushGlobalMode 16
#define Push1 17
#define Push2 18
#define AntiBounceDelay 200




#define SerialSpeed 115200
void setup();
void loop();
void CheckControllPanel();
void CheckRotaryFrequencyStep();
byte CheckRotaryFrequencyStepRotate(byte FStep);
void CheckRotaryFrequency();
void CheckRotaryModeStep();
void CheckRotaryMode();
void DisplayFrequency(byte CurrentDisplay);
void AD9833reset();
void AD9833WriteRegister(int dat);
void AD9833FreqSet(long frequency, int wave, uint8_t NumGen);
void SevenSegHello();
void tftHello();
void DrawModeGraph(uint8_t Mode);
void DrawSine(uint8_t Leng, float High,uint8_t Xx, uint8_t Yy);
void DrawTriangle(uint8_t Leng, uint8_t High,uint8_t Xx, uint8_t Yy);
void DrawSquare(uint8_t Leng, uint8_t High,uint8_t Xx, uint8_t Yy);
void PrintFrequencyLimit(uint8_t NumBlocco);
void DrawArrowFDisplay(uint8_t NumDisplay, uint8_t ModeDisplay);
void GlobalModeDisplay();
void DrawSweepTime(uint8_t Xx,uint8_t Yy, uint16_t SweepTimeX);
void TftGraphInit();
void SetupIO();
#line 233 "I:/Dropbox/Arduino/Generatori Segnale/AD9833/Fiu-FunctionGenerator/FiuDDS-Platformio/AD9833-VFO-r05/src/AD9833-VFO-r05a.ino"
void setup()
{

  if(DebugLevel==1)
  {
    Serial.begin(SerialSpeed);
  }


  SetupIO();


  CurrentGenerator = Generator0;
  FrequencyWaveCurrentType [CurrentGenerator] = Sine;
  Frequency[FreqL][CurrentGenerator] = FrequencyLimit[FrequencyWaveCurrentType[CurrentGenerator]][CurrentGenerator];


  SevenSegHello();


  tftHello();

  delay(3000);


  AD.begin();
  AD9833reset();
  GlobalModeDisplay();
}






void loop()
{
  CheckControllPanel();




  if (GlobalMode==GlobalModeSweepLtH)
  {

    if (millis()-TZero>SweepTime)
    {

      SweepCurrentFrequency=Frequency[0][0];

      TZero=millis();

      AD9833FreqSet(SweepCurrentFrequency, FrequencyWaveType[FrequencyWaveCurrentType[0]],0);

      TXs=TZero;
    }
    else
    {


      if (millis()-TXs>= SweepTStep)
      {
        SweepCurrentFrequency+=FSweepStep;

        AD9833FreqSet(SweepCurrentFrequency, FrequencyWaveType[FrequencyWaveCurrentType[0]],0);
        TXs=millis();
      }
    }
  }
}






void CheckControllPanel()
{

  if(digitalRead(PushGlobalMode)==LOW)
  {

    delay(AntiBounceDelay);
    if(digitalRead(PushGlobalMode)==LOW)
    {
      GlobalMode++;

      if (GlobalMode>=GlobalModeNumber)
      {
        GlobalMode = GlobalModeFixFrequency;
      }

      GlobalModeDisplay();

      while(digitalRead(PushGlobalMode)==LOW)
      {

      }
    }
  }
  else
  {

    FrequencyRotaryState= FrequencyRotary.rotate();
    if (FrequencyRotaryState != 0)

    {
      if (HIGH == digitalRead(FERotaryButton))

      {
        CheckRotaryFrequency();
      }
      else

      {
        CheckRotaryFrequencyStep();
      }
    }

    ModeRotaryState = ModeRotary.rotate();
    if (ModeRotaryState != 0)
    {
      if (HIGH == digitalRead(MERotaryButton))
      {
        Serial.println("Mode");
        CheckRotaryMode();
      }
      else
      {
        CheckRotaryModeStep();
      }
    }
  }
}
# 376 "I:/Dropbox/Arduino/Generatori Segnale/AD9833/Fiu-FunctionGenerator/FiuDDS-Platformio/AD9833-VFO-r05/src/AD9833-VFO-r05a.ino"
void CheckRotaryFrequencyStep()
{



  FrequencyDisplay.clear(CurrentGenerator);

  FrequencyDisplay.write(FrequencyRotaryStep[0]+1,B01100011);
  delay(10);

  while (digitalRead(FERotaryButton) == LOW)

 {
    FrequencyRotaryStep[CurrentGenerator]=CheckRotaryFrequencyStepRotate(FrequencyRotaryStep[CurrentGenerator]);
    FrequencyDisplay.clear(CurrentGenerator);
    FrequencyDisplay.write(FrequencyRotaryStep[CurrentGenerator]+1,B01100011);
  }
# 410 "I:/Dropbox/Arduino/Generatori Segnale/AD9833/Fiu-FunctionGenerator/FiuDDS-Platformio/AD9833-VFO-r05/src/AD9833-VFO-r05a.ino"
  PrintFrequencyLimit(0);
}




byte CheckRotaryFrequencyStepRotate(byte FStep)

{
  FrequencyRotaryState= FrequencyRotary.rotate();

  if ( FrequencyRotaryState== 1 )
  {
    FStep++;
  }
  if ( FrequencyRotaryState== 2 )
  {
    FStep--;
  }
  delay(20);
  return(FStep);
}




void CheckRotaryFrequency()
{



  for (byte i=0;i<2;i++)
  {
    FrequencyRotaryStepHerz[i]=1;
    for(byte ii=0; ii>FrequencyRotaryStep[i]; ii++)
    {
      FrequencyRotaryStepHerz[i]*=10;
    }

  }



  if(GlobalMode == GlobalModeModulation)
  {
    if ( FrequencyRotaryState== 1 )
    {
      Frequency[0][0] += FrequencyRotaryStepHerz[0];
    }

    if ( FrequencyRotaryState== 2 )
    {
      Frequency[0][0] -= FrequencyRotaryStepHerz[0];
    }
  }
  else
  {
    if ( FrequencyRotaryState== 1 )
    {
      Frequency[CurrentGenerator][0] += FrequencyRotaryStepHerz[CurrentGenerator];
    }

    if ( FrequencyRotaryState== 2 )
    {
      Frequency[CurrentGenerator][0] -= FrequencyRotaryStepHerz[CurrentGenerator];
    }
  }

  switch (GlobalMode)
  {
    case GlobalModeFixFrequency:


      if (Frequency[0][0]<FrequencyLimit[FrequencyWaveCurrentType[0]][0])
      {
        Frequency[0][0] = FrequencyLimit[FrequencyWaveCurrentType[0]][1];
      }

      if (Frequency[0][0]>FrequencyLimit[FrequencyWaveCurrentType[0]][1])
      {
        Frequency[0][0] = FrequencyLimit[FrequencyWaveCurrentType[0]][0];
      }

      AD9833FreqSet(Frequency[CurrentGenerator][0],FrequencyWaveType[FrequencyWaveCurrentType[CurrentGenerator]],CurrentGenerator);
      break;
    case GlobalModeSweepLtH:


      if (Frequency[1][0]<Frequency[0][0]+1)
      {
        Frequency[1][0] = FrequencyLimit[FrequencyWaveCurrentType[0]][1];
      }

      if (Frequency[1][0]>FrequencyLimit[FrequencyWaveCurrentType[0]][1])
      {
        Frequency[1][0] = Frequency[0][0]+1;
      }

      SweepTStep = 1;
      FSweepStep=(Frequency[1][0]-Frequency[0][0])/(SweepTime/SweepTStep);
      if (FSweepStep<1.0)
      {
        SweepTStep= 1/FSweepStep;
        FSweepStep=(Frequency[1][0]-Frequency[0][0])/(SweepTime/SweepTStep);
      }
      break;
    case GlobalModeModulation:



      if (Frequency[0][0]<FrequencyLimit[FrequencyWaveCurrentType[0]][0])
      {
        Frequency[0][0] = FrequencyLimit[FrequencyWaveCurrentType[0]][1];
      }

      if (Frequency[0][0]>FrequencyLimit[FrequencyWaveCurrentType[0]][1])
      {
        Frequency[0][0] = FrequencyLimit[FrequencyWaveCurrentType[0]][0];
      }



      if (Frequency[0][0]<FrequencyLimit[FrequencyWaveCurrentType[0]][0])
      {
        Frequency[0][0] = FrequencyLimit[FrequencyWaveCurrentType[0]][1];
      }

      if (Frequency[0][0]>FrequencyLimit[FrequencyWaveCurrentType[0]][1])
      {
        Frequency[0][0] = FrequencyLimit[FrequencyWaveCurrentType[0]][0];
      }

      AD9833FreqSet(Frequency[0][0],FrequencyWaveType[FrequencyWaveCurrentType[0]],0);

      AD9833FreqSet(Frequency[1][0],FrequencyWaveType[FrequencyWaveCurrentType[1]],1);
      break;
    default:
      break;
  }

  DisplayFrequency(CurrentGenerator);
}

void CheckRotaryModeStep()
{
  DrawSweepTime(10,60,8*(pow(10,SweepTimeStep)));
  delay(10);
  while (digitalRead(MERotaryButton) == LOW)
  {
    ModeRotaryState= ModeRotary.rotate();

    if ( ModeRotaryState== 1 )
    {
       SweepTimeStep++;
    }
    if ( ModeRotaryState== 2 )
    {
      SweepTimeStep--;
    }

    if(SweepTimeStep<0)
    {
      SweepTimeStep=4;
    }
    if(SweepTimeStep>4)
    {
      SweepTimeStep=0;
    }
    if(ModeRotaryState!=0)
    {
      DrawSweepTime(10,60,8*(pow(10,SweepTimeStep)));
    }
  }
}

void CheckRotaryMode()
{
    switch (GlobalMode)
    {
      case GlobalModeFixFrequency:


        if ( ModeRotaryState == 1 )
        {
          FrequencyWaveCurrentType[0]++;
        }


        if ( ModeRotaryState == 2 )
        {
          FrequencyWaveCurrentType[0]--;
        }



        if (FrequencyWaveCurrentType[0]>=WaveTypeNumber)
        {
          FrequencyWaveCurrentType[0] = 0;
        }


        if (FrequencyWaveCurrentType[0]<0)
        {
          FrequencyWaveCurrentType[0] = WaveTypeNumber-1;
        }


        DrawModeGraph(FrequencyWaveCurrentType[0]);

        AD9833FreqSet(Frequency[0][0],FrequencyWaveType[FrequencyWaveCurrentType[0]],0);

        DisplayFrequency(CurrentGenerator);
        break;
      case GlobalModeSweepLtH:


        if ( ModeRotaryState == 1 )
        {
          SweepTime+=pow(10,SweepTimeStep);
        }


        if ( ModeRotaryState == 2 )
        {
          SweepTime-=pow(10,SweepTimeStep);
        }


        if (SweepTime>=65500)
        {
          SweepTime = 1;
        }


        if (SweepTime<1)
        {
          SweepTime = 65500;
        }
        DrawSweepTime(10,60, SweepTime);

        break;
      case GlobalModeModulation:

        if ( ModeRotaryState == 1 )
        {
          Frequency[1][0]++;
        }


        if ( ModeRotaryState == 2 )
        {
          Frequency[1][0]--;
        }



        if (Frequency[1][0]>=FrequencyLimit[FrequencyWaveCurrentType[1]][1])
        {
          Frequency[1][0] = FrequencyLimit[FrequencyWaveCurrentType[1]][0];
        }


        if (Frequency[1][0]<FrequencyLimit[FrequencyWaveCurrentType[1]][0])
        {
          Frequency[1][0] = FrequencyLimit[FrequencyWaveCurrentType[1]][1];
        }




        AD9833FreqSet(Frequency[1][0],FrequencyWaveType[FrequencyWaveCurrentType[1]],1);

        DisplayFrequency(CurrentGenerator);
        break;
      default:
        break;
    }
}
# 697 "I:/Dropbox/Arduino/Generatori Segnale/AD9833/Fiu-FunctionGenerator/FiuDDS-Platformio/AD9833-VFO-r05/src/AD9833-VFO-r05a.ino"
void DisplayFrequency(byte CurrentDisplay)
{

    FrequencyDisplay.clear(CurrentDisplay);

    FrequencyDisplay.printDigit(Frequency[0][0],CurrentDisplay);
}







void AD9833reset()
{
  AD9833WriteRegister(0x100);
  delay(10);
}

void AD9833WriteRegister(int dat)
{
  digitalWrite(AD9833CLK, LOW);
  digitalWrite(AD9833CLK, HIGH);

  digitalWrite(AD9833FSYNC, LOW);
  for (byte i = 0; i < 16; i++)
  {
    if (dat & 0x8000)
      digitalWrite(AD9833DATA, HIGH);
    else
      digitalWrite(AD9833DATA, LOW);
    dat = dat << 1;
    digitalWrite(AD9833CLK, HIGH);
    digitalWrite(AD9833CLK, LOW);
  }
  digitalWrite(AD9833CLK, HIGH);
  digitalWrite(AD9833FSYNC, HIGH);
}





void AD9833FreqSet(long frequency, int wave, uint8_t NumGen)
{
  if(NumGen==0)
  {
    long fl = frequency * (0x10000000 / 25000000.0);
    AD9833WriteRegister(0x2000 | wave);
    AD9833WriteRegister((int)(fl & 0x3FFF) | 0x4000);
    AD9833WriteRegister((int)((fl & 0xFFFC000) >> 14) | 0x4000);
  }
  else
  {
    long fl = frequency * (0x10000000 / 25000000.0);
    AD9833WriteRegister(0x2000 | wave);
    AD9833WriteRegister((int)(fl & 0x3FFF) | 0x8000);
    AD9833WriteRegister((int)((fl & 0xFFFC000) >> 14) | 0x8000);
  }
}






void SevenSegHello()
{



  FrequencyDisplay.on(DisplayNumber);
  FrequencyDisplay.setBright(10,0);
  FrequencyDisplay.setBright(10,1);

  FrequencyDisplay.setDigitLimit(Digit4Display,0);
  FrequencyDisplay.setDigitLimit(Digit4Display,1);
  FrequencyDisplay.clear(0);
  FrequencyDisplay.printDigit(12245678,0);
  FrequencyDisplay.clear(1);
  FrequencyDisplay.printDigit(87754321,1);
  FrequencyDisplay.clear(0);
  FrequencyDisplay.clear(1);

  for(uint8_t ii=0;ii<Digit4Display; ii++)
  {
    FrequencyDisplay.write(ii+1,pgm_read_word_near(FHello+ii),0);
    FrequencyDisplay.write(ii+1,pgm_read_word_near(FHello+(Digit4Display+ii)),1);
  }
}






void tftHello()
{
  tft.initR(INITR_BLACKTAB);

  tft.setTextWrap(false);
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(30, 5);
  tft.setTextColor(ST77XX_GREEN);
  tft.setTextSize(4);
  tft.println(Modello);
  tft.setCursor(40, 45);
  tft.setTextColor(ST77XX_YELLOW);
  tft.setTextSize(3);
  tft.println(Modello1);
  tft.setCursor(15, 75);
  tft.println(Modello2);

  tft.setCursor(30, 105);
  tft.setTextSize(2);
  tft.print("SaLe");
  tft.setCursor(5, 120);
  tft.println("DarioMAS");
  tft.setTextSize(1);

  tft.print("Hardware Ver.");
  tft.println(Hardware);
  tft.print("Software Ver.");
  tft.println(Software);
}





void DrawModeGraph(uint8_t Mode)
{
  switch (Mode)
  {
    case 0:
      DrawSine(39, 25,85, 110);
      break;
    case 1:
      DrawTriangle(39, 25,85, 110);
      break;
    case 2:
      DrawSquare(39, 25,85, 110);
      break;
    default:
      break;
  }
}





void DrawSine(uint8_t Leng, float High,uint8_t Xx, uint8_t Yy)
{

  PrintFrequencyLimit(0);

  float AlfHigh = High/2;
  for (uint8_t xx=0;xx<Leng;xx++)
  {
    for (uint8_t offset=0; offset<6; offset++)
    {
      tft.drawPixel(Xx+xx+offset, Yy+AlfHigh+(AlfHigh*sin((3.14/High)*float(xx))), ST77XX_GREEN);
    }
  }
}






void DrawTriangle(uint8_t Leng, uint8_t High,uint8_t Xx, uint8_t Yy)
{

  PrintFrequencyLimit(0);

  for(uint8_t offset=0;offset<6;offset++)
  {
    tft.drawLine( Xx+offset, Yy+High/2, Xx+Leng/6+offset, Yy+High-1, ST77XX_GREEN);
    tft.drawLine( Xx+(Leng/6)+offset, Yy+High-1, Xx+(3*Leng/6)+offset, Yy, ST77XX_GREEN);
    tft.drawLine(Xx+(3*Leng/6)+offset, Yy, Xx+(5*Leng/6)+offset, Yy+High-1, ST77XX_GREEN);
    tft.drawLine(Xx+(5*Leng/6)+offset, Yy+High-1, Xx+Leng-5+offset, Yy+High/2, ST77XX_GREEN);
  }
}





void DrawSquare(uint8_t Leng, uint8_t High,uint8_t Xx, uint8_t Yy)
{

  PrintFrequencyLimit(0);

  for(uint8_t offset=0;offset<6;offset++)
  {
    tft.drawLine( Xx+offset, Yy, Xx+offset, Yy+High-1, ST77XX_GREEN);
    tft.drawLine( Xx+offset, Yy+High-1, Xx+Leng/3+offset, Yy+High-1, ST77XX_GREEN);
    tft.drawLine( Xx+Leng/3+offset, Yy+High-1, Xx+Leng/3+offset, Yy, ST77XX_GREEN);
    tft.drawLine( Xx+Leng/3+offset, Yy, Xx+2*Leng/3+offset, Yy, ST77XX_GREEN);
    tft.drawLine(Xx+2*Leng/3+offset, Yy, Xx+2*Leng/3+offset, Yy+High-1, ST77XX_GREEN);
    tft.drawLine(Xx+2*Leng/3+offset, Yy+High-1, Xx+Leng+offset, Yy+High-1, ST77XX_GREEN);
    tft.drawLine( Xx+Leng-offset, Yy, Xx+Leng-offset, Yy+High-1, ST77XX_GREEN);
  }
}





void PrintFrequencyLimit(uint8_t NumBlocco)
{

  tft.fillRect(84,112,43, 27, ST77XX_BLACK);

  tft.fillRect(20,112, 58, 24, ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(19,128);
  tft.print("min ");
  tft.print(FrequencyLimit[FrequencyWaveCurrentType[CurrentGenerator]][0]);
  tft.setCursor(65,128);
  tft.print("Hz");
  tft.setCursor(19,119);
  tft.print("Step ");
  tft.print(int(FrequencyRotaryStepHerz[0]));
  tft.setCursor(65,119);
  tft.print("Hz");
  tft.setCursor(19,110);
  tft.print("MAX ");
  tft.print(FrequencyLimit[FrequencyWaveCurrentType[CurrentGenerator]][1]/1000000);
  tft.setCursor(65,110);
  tft.print("MHz");
}






void DrawArrowFDisplay(uint8_t NumDisplay, uint8_t ModeDisplay)
{
  switch (NumDisplay)
  {
    case 1:

      tft.fillTriangle(0, 120, 10, 110, 10, 130, ST77XX_RED);
      break;
    case 2:

      tft.fillTriangle(0, 25, 10, 15, 10, 35, ST77XX_RED);
      break;
    case 3:

      tft.fillTriangle(0, 120, 10, 110, 10, 130, ST77XX_GREEN);
      break;
    case 4:

      tft.fillTriangle(0, 25, 10, 15, 10, 35, ST77XX_GREEN);
      break;
    case 9:

      tft.fillTriangle(0, 120, 10, 110, 10, 130, ST77XX_BLACK);
      break;
    case 10:

      tft.fillTriangle(0, 25, 10, 15, 10, 35, ST77XX_BLACK);
      break;
    default:
      break;
  }
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_BLACK);
  switch (ModeDisplay)
  {
    case 1:

      tft.setCursor(4,117);
      tft.print("1");
      break;
    case 2:

      tft.setCursor(4,23);
      tft.print("2");
      break;
    case 3:

      tft.setCursor(4,117);
      tft.print("L");
      break;
    case 4:

      tft.setCursor(4,23);
      tft.print("H");
      break;
    case 9:
      break;
    case 10:
      break;
    default:
      break;
  }
}







void GlobalModeDisplay()
{
  TftGraphInit();

  switch (GlobalMode)
  {
    case GlobalModeFixFrequency:


      FrequencyDisplay.clear(0);
      FrequencyDisplay.clear(1);

      CurrentGenerator = 0;
      FrequencyDisplay.printDigit(Frequency[GlobalModeFixFrequency][CurrentGenerator], 0);

      DrawArrowFDisplay(3,1);

      DrawModeGraph(FrequencyWaveType[FrequencyWaveCurrentType[CurrentGenerator]]);



      AD9833FreqSet(Frequency[CurrentGenerator][0],FrequencyWaveType[FrequencyWaveCurrentType[CurrentGenerator]],CurrentGenerator);
      DisplayFrequency(CurrentGenerator);
      break;
    case GlobalModeSweepLtH:

      CurrentGenerator = 1;

      SweepTStep=micros();

      Frequency[CurrentGenerator][0]=Frequency[CurrentGenerator][0]+0;

      AD9833FreqSet(Frequency[CurrentGenerator][0],FrequencyWaveType[FrequencyWaveCurrentType[CurrentGenerator]],0);
      SweepTStep=(micros()-SweepTStep)/1000.0;
      DisplayFrequency(CurrentGenerator);
      delay(10);

      DrawArrowFDisplay(1,3);

      DrawArrowFDisplay(4,4);

      DrawModeGraph(FrequencyWaveType[FrequencyWaveCurrentType[CurrentGenerator]]);
      DrawSweepTime(10,60, SweepTime);
      CurrentGenerator = 1;

      Frequency[1][0] = Frequency[0][0]+1;

      DisplayFrequency(1);

      TZero=millis();
      break;
    case GlobalModeSweepHtL:


      SweepTStep=micros();

      Frequency[CurrentGenerator][0]=Frequency[CurrentGenerator][0]+0;

      AD9833FreqSet(Frequency[CurrentGenerator][0],FrequencyWaveType[FrequencyWaveCurrentType[CurrentGenerator]],0);
      SweepTStep=(micros()-SweepTStep)/1000.0;
      DisplayFrequency(CurrentGenerator);
      delay(10);

      DrawArrowFDisplay(1,3);

      DrawArrowFDisplay(4,4);

      DrawModeGraph(FrequencyWaveType[FrequencyWaveCurrentType[CurrentGenerator]]);
      DrawSweepTime(10,60, SweepTime);
      CurrentGenerator = 1;

      Frequency[1][0] = Frequency[0][0]+1;

      DisplayFrequency(1);

      TZero=millis();
      break;
    case GlobalModeSweepLtHtL:


      SweepTStep=micros();

      Frequency[CurrentGenerator][0]=Frequency[CurrentGenerator][0]+0;

      AD9833FreqSet(Frequency[CurrentGenerator][0],FrequencyWaveType[FrequencyWaveCurrentType[CurrentGenerator]],0);
      SweepTStep=(micros()-SweepTStep)/1000.0;
      DisplayFrequency(CurrentGenerator);
      delay(10);

      DrawArrowFDisplay(1,3);

      DrawArrowFDisplay(4,4);

      DrawModeGraph(FrequencyWaveType[FrequencyWaveCurrentType[CurrentGenerator]]);
      DrawSweepTime(10,60, SweepTime);
      CurrentGenerator = 1;

      Frequency[1][0] = Frequency[0][0]+1;

      DisplayFrequency(1);

      TZero=millis();
      break;
    case GlobalModeModulation:


      DrawArrowFDisplay(3,1);

      DrawArrowFDisplay(4,2);

      DrawModeGraph(FrequencyWaveType[FrequencyWaveCurrentType[CurrentGenerator]]);
      Frequency[1][0]= FrequencyLimit[FrequencyWaveCurrentType[1]][1];
      DisplayFrequency(1);
      break;
    default:
      break;
  }
}

void DrawSweepTime(uint8_t Xx,uint8_t Yy, uint16_t SweepTimeX)
{

  tft.fillRect(Xx,Yy-1,80, 15, ST77XX_BLACK);
  delay(10);
  tft.fillRect(Xx,Yy-1,80, 15, ST77XX_BLACK);

  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(Xx,Yy);

  sprintf(SweepTimeBuff, "%05u mS", SweepTimeX);

  tft.print(SweepTimeBuff);





}

void TftGraphInit()
{

  tft.setTextSize(1);

  tft.fillScreen(ST77XX_BLACK);
  tft.drawLine(0,12, 159,12,ST77XX_WHITE);
  tft.drawRect(0,139,42,20,ST77XX_WHITE);
  tft.drawRect(42,139,43,20,ST77XX_WHITE);
  tft.drawRect(85,139,43,20,ST77XX_WHITE);
  tft.drawRect(15,106,68,33,ST77XX_WHITE);
  tft.drawRect(83,106,45,33,ST77XX_WHITE);

  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(0,0);
  tft.print(GlobalModeLabel[GlobalMode]);
  tft.setCursor(18,141);
  tft.print("F1");
  tft.setCursor(12,151);
  tft.print("Mode");

  tft.setCursor(57,141);
  tft.print("F2");

  tft.setCursor(100,141);
  tft.print("F3");
}


void SetupIO()
{
  pinMode(AD9833DATA, OUTPUT);
  pinMode(AD9833CLK, OUTPUT);
  pinMode(AD9833FSYNC, OUTPUT);

  pinMode(FERotaryButton, INPUT_PULLUP);
  pinMode(MERotaryButton, INPUT_PULLUP);

  pinMode(PushGlobalMode, INPUT_PULLUP);
  pinMode(Push1, INPUT_PULLUP);
  pinMode(Push2, INPUT_PULLUP);

  pinMode(MAX7219CS, OUTPUT);
}