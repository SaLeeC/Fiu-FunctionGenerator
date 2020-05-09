/*
 * FIU
 * VFO a larga banda
 * Generatore di Funzioni in tecnologia DDS
 * Campo di frequenza 1Hz-12MHz
 * 
 */
//Debug Level 0 nessun messaggio di debug
//Debug Level 1 Solo messaggi relativi alla frequenza
#define DebugLevel 0

//#=================================================================================
// Identificativo
//#=================================================================================
#define Modello  "Fiu"
#define Modello1 "VFO"
#define Modello2 "FG DDS"
#define Hardware 1
#define Software 4

#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
#include <SPI.h>             // Arduino SPI library
#include <MD_AD9833.h>
#include <SimpleRotary.h>
#include <math.h>
#include "DigitLedDisplay.h"
//#include <EEPROM.h>

//#=================================================================================
// AD9833 IC area
// DDS Generator
//#=================================================================================
#define AD9833DATA  11  ///< SPI Data pin number
#define AD9833CLK   13  ///< SPI Clock pin number
#define AD9833FSYNC 12  ///< SPI Load pin number (FSYNC in AD9833 usage)

//MD_AD9833  AD(FSYNC); // Hardware SPI
MD_AD9833 AD(AD9833DATA, AD9833CLK, AD9833FSYNC); // Arbitrary SPI pins
MD_AD9833::channel_t chan;

const int wSine     = 0b0000000000000000;
const int wTriangle = 0b0000000000000010;
const int wSquare   = 0b0000000000101000;

//Frequenza corrente per i due generatori
uint32_t Frequency[2];
//Generatore corrente
uint8_t  RefGen=0;

#define WaveTypeNumber 3

//Tipo corrente della forma d'onda per ogni generatore
int8_t  FrequencyWaveCurrentType[2] = {0,0};

//Feasi per la selezione della forma d'onda nel generatore
uint8_t  FrequencyWaveType[WaveTypeNumber] = {0b0000000000000000,
                                              0b0000000000000010,
                                              0b0000000000101000};
//Limiti operativi per ogni forma d'onda. Il generatore funziona sino a 12,5 MHz ma,
//oltre certe frequenze la sinusoide e la trinagolare degenerano degradando la qualità,
//al fine di produrre toni di qualità accettabile ho scelto di limitarlo in frequenza
//in maniera differenziata per forma d'onda
uint32_t FrequencyLimit[WaveTypeNumber][2] = {  1,  5000000,
                                                1,  5000000,
                                                1, 12500000};

//Modalità di funzionamento del Generatore
// 0 -> Genera una frequenza fissa sulla porta 1
     // Controllo della forma d'onda
     // Controllo della frequenza
     // Attivo solo il display 1
// 1 -> Genera uno sweep fra la frequenza 1 (corrente) e la frequenza 2 (impostabile
     // e sempre maggiore di F1. Uscita sulla porta 1
     // Controllo della forma d'onda
     // Controllo della frequenza (2)
     // Attivo display 1 e 2

#define GlobalModeNumber 2
int8_t GlobalMode = 0;
String GlobalModeLabel[GlobalModeNumber]={"Fixed Frequency",
                                          "Sweep"};

//#=================================================================================
// MAX7219 IC area
//7 Segmenti per 8 digit - Display Frequenza
//#=================================================================================
#define FDisplayNumDigit 8
#define MAX7219CS    2
#define MAX7219CLK  13
#define MAX7219Data 11

DigitLedDisplay FrequencyDisplay = DigitLedDisplay(MAX7219Data, MAX7219CS, MAX7219CLK);

//Frase di apertura "ddS FG"
const uint8_t FHello[8] PROGMEM =  {B00000000,
                                    B01011110,
                                    B01000111,
                                    B00000000,
                                    B01011011,
                                    B00111101,
                                    B00111101,
                                    B00000000}; 

//#=================================================================================
// MAX7219 IC area
//7 Segmenti per 8 digit - Display Frequenza 2
//#=================================================================================
#define MAX7219CS2 3

DigitLedDisplay Frequency2Display = DigitLedDisplay(MAX7219Data, MAX7219CS2, MAX7219CLK);

//Frase di apertura "Fiu"
const uint8_t F2Hello[8] PROGMEM = {B00000000,
                                    B00000000,
                                    B00011100,
                                    B00000100,
                                    B01000111,
                                    B00000000,
                                    B00000000,
                                    B00000000}; 


//#=================================================================================
// ST7735 - 1,8" OLED area
//Display per forma d'onda, livello di uscita e altre funzioni
//#=================================================================================
// ST7735 TFT module connections
#define TFT_CS      4  // define chip select pin
#define TFT_DC      5  // define data/command pin
#define TFT_RST     6  // define reset pin, or set to -1 and connect to Arduino RESET pin

// Initialize Adafruit ST7735 TFT library
//Usa la SPI nativa. In questa configurazione hardware crea problemi la convivenza dei
//diversi driver per cui ho adottato la SPI software, più lenta ma compatibile
//Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

#define TFT_MOSI 11  // Data out
#define TFT_SCLK 13  // Clock out

// For ST7735-based displays, we will use this call
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

//#=================================================================================
// Encoder Rotary area
//Settaggio Frequenza e Step di avanzamento frequenza
//#=================================================================================
#define FERotaryA 7
#define FERotaryB 8
#define FERotaryButton 14
SimpleRotary FrequencyRotary(FERotaryA,FERotaryB,FERotaryButton);

uint8_t FrequencyRotariState;
uint8_t FrequencyRotaryStep = 0;//potenza del 10 che si somma o sottrae alla frequenza

//#=================================================================================
// Encoder Rotary area
//Settaggio forma d'onda e Modalità di generazione (singola frequenza, sweep, ...)
//#=================================================================================
#define MERotaryA  9
#define MERotaryB 10
#define MERotaryButton 15
SimpleRotary ModeRotary(MERotaryA,MERotaryB,MERotaryButton);

uint8_t ModeRotariState;

#define NumOfMode 2
char *ModeLabel[] = {"Forma d'Onda", "Sweep"};

//#=================================================================================
// Serial
//#=================================================================================
#define SerialSpeed 57600
 
// Character constants for commands
const char CMD_HELP = '?';
const char BLANK = ' ';
const char PACKET_START = ':';
const char PACKET_END = ';';
const char CMD_FREQ = 'F';
const char CMD_PHASE = 'P';
const char CMD_OUTPUT = 'O';
const char OPT_FREQ = 'F';
const char OPT_PHASE = 'P';
const char OPT_SIGNAL = 'S';
const char OPT_1 = '1';
const char OPT_2 = '2';
const char OPT_MODULATE = 'M';
const uint8_t PACKET_SIZE = 20;

//#=================================================================================
// Setup
//#=================================================================================
void setup()
{
  Serial.begin(SerialSpeed);

  pinMode(AD9833DATA, OUTPUT);
  pinMode(AD9833CLK, OUTPUT);
  pinMode(AD9833FSYNC, OUTPUT);

  pinMode(FERotaryButton, INPUT_PULLUP);
  pinMode(MERotaryButton, INPUT_PULLUP);

  tft.initR(INITR_BLACKTAB);      // Init ST7735S chip, black tab

  /* Set the brightness min:1, max:15 */
  FrequencyDisplay.setDigitLimit(FDisplayNumDigit);
  FrequencyDisplay.setBright(10);
  FrequencyDisplay.clear();

  /* Set the brightness min:1, max:15 */
  Frequency2Display.setDigitLimit(FDisplayNumDigit);
  Frequency2Display.setBright(10);
  Frequency2Display.clear();

  SevenSegHello();
  // Use this initializer if using a 1.8" TFT screen:
  tftHello();
  
  delay(3000);

  Frequency[RefGen] = FrequencyLimit[FrequencyWaveCurrentType[RefGen]][0];
  
  AD.begin();
  AD9833reset();
  GlobalModeDisplay();  
}

//void usage(void)
//{
//  Serial.print(F("\n\n[MD_AD9833_Tester]"));
//  Serial.print(F("\n?\thelp - this message"));
//  Serial.print(F("\n\n:<cmd><opt> <param>;"));
//  Serial.print(F("\n:f1n;\tset frequency 1 to n Hz"));
//  Serial.print(F("\n:f2n;\tset frequency 2 to n Hz"));
//  Serial.print(F("\n:fmn;\tset frequency modulation to n Hz"));
//  Serial.print(F("\n:p1n;\tset phase 1 to n in tenths of a degree (1201 is 120.1 deg)"));
//  Serial.print(F("\n:p2n;\tset phase 2 to n in tenths of a degree (1201 is 120.1 deg)"));
//  Serial.print(F("\n:ofn;\toutput frequency n or modulation [n=1/2/m]"));
//  Serial.print(F("\n:opn;\toutput phase n or modulation [n=1/2/m]"));
//  Serial.print(F("\n:osn;\toutput signal type n [n=(o)ff/(s)ine/(t)riangle/s(q)uare]"));
//}


uint8_t htoi(char c)
{
  c = toupper(c);
  
  if (c >= '0' && c <= '9')
      return(c - '0');
  else if (c >= 'A' && c <= 'F')
      return(c - 'A' + 10);
  else
      return(0);
}

char nextChar(void)
// Read the next character from the serial input stream
// Blocking wait
{
  //Controlla il pannello comandi
  CheckRotary();
  while (!Serial.available())
  //Controlla la seriale in attesa di comandi
  {
    ; /* do nothing */
    return(toupper(Serial.read()));
  }
}

char *readPacket(void)
// read a packet and return the 
{
  static enum { S_IDLE, S_READ_CMD, S_READ_MOD, S_READ_PKT } state = S_IDLE;
  static char cBuf[PACKET_SIZE + 1];
  static char *cp;
  char c;

  switch (state)
  {
  case S_IDLE:   // waiting for packet start
    c = nextChar();
    if (c == CMD_HELP)
    {
//      usage();
      break;
    }
    if (c == PACKET_START)
    {
      cp = cBuf;
      state = S_READ_CMD;
    }
    break;

  case S_READ_CMD:   // waiting for command char
    c = nextChar();
    if (c == CMD_FREQ || c == CMD_PHASE || c == CMD_OUTPUT)
    {
      *cp++ = c;
      state = S_READ_MOD;
    }
    else
      state = S_IDLE;
    break;

  case S_READ_MOD: // Waiting for command modifier
    c = nextChar();
    if (c == OPT_FREQ || c == OPT_PHASE || c == OPT_SIGNAL ||
      c == OPT_1 || c == OPT_2 || c == OPT_MODULATE)
    {
      *cp++ = c;
      state = S_READ_PKT;
    }
    else
      state = S_IDLE;
    break;

  case S_READ_PKT: // Reading parameter until packet end
    c = nextChar();
    if (c == PACKET_END)
    {
      *cp = '\0';
      state = S_IDLE;
      return(cBuf);
    }
    *cp++ = c;
    break;

  default:
    state = S_IDLE;
    break;
  }
  return(NULL);
}

void processPacket(char *cp)
// Assume we have a correctly formed packet from the pasing in readPacket()
{
  uint32_t  ul;
//  MD_AD9833::channel_t chan;
  MD_AD9833::mode_t mode;

  switch (*cp++)
  {
  case CMD_FREQ:
    switch (*cp++)
    {
    case OPT_1:
      chan = MD_AD9833::CHAN_0;
      RefGen = 0;
      break;
    case OPT_2:
      chan = MD_AD9833::CHAN_1;
      RefGen = 1;
      break;
    case OPT_MODULATE:
      /* do something in future */
      break;
    }

    ul = strtoul(cp, NULL, 10);
    Frequency[RefGen] = ul;//Salva il valore della frequenza nel buffer di servizio
    Serial.print("canale ");
    Serial.print(RefGen);
    Serial.print(" - Frequenza ");
    Serial.println(Frequency[RefGen]);
//    DisplayFrenquency();
    AD.setFrequency(chan, ul);
    break;

  case CMD_PHASE:
    switch (*cp++)
    {
    case OPT_1: chan = MD_AD9833::CHAN_0; break;
    case OPT_2: chan = MD_AD9833::CHAN_1; break;
    }

    ul = strtoul(cp, NULL, 10);
    AD.setPhase(chan, (uint16_t)ul);
    break;

  case CMD_OUTPUT:
    switch (*cp++)
    {
    case OPT_FREQ:
      switch (*cp)
      {
      case OPT_1:
        chan = MD_AD9833::CHAN_0;
        RefGen = 0;
        break;
      case OPT_2:
        chan = MD_AD9833::CHAN_1;
        RefGen = 1;
        break;
      case OPT_MODULATE:
        /* do something in future */
        break;
      }
      AD.setActiveFrequency(chan);
      break;

    case OPT_PHASE:
      switch (*cp)
      {
      case OPT_1: chan = MD_AD9833::CHAN_0; break;
      case OPT_2: chan = MD_AD9833::CHAN_1; break;
      case OPT_MODULATE: /* do something in future */ break;
      }
      AD.setActivePhase(chan);
      break;

    case OPT_SIGNAL:
      switch (*cp)
      {
      case 'O': mode = MD_AD9833::MODE_OFF;    break;
      case 'S': mode = MD_AD9833::MODE_SINE;   break;
      case 'T': mode = MD_AD9833::MODE_TRIANGLE;  break;
      case 'Q': mode = MD_AD9833::MODE_SQUARE1;  break;
      }
      AD.setMode(mode);
      break;
    }
    break;
  }

  return;
}


void loop()
{
  char  *cp;
  
  if ((cp = readPacket()) != NULL)
    processPacket(cp);
}

void CheckRotary()
{
//  Serial.println("CheckRotary");
  if (HIGH == digitalRead(FERotaryButton))
  //Se il rotore dell'encoder della frquenza NON è premuto gestisce la frequenza
  {
    CheckRotaryFrequency();
  }
  else
  {
    CheckRotaryFrequencyStep();
  }
//  Serial.println("CheckRotary");
  if (HIGH == digitalRead(MERotaryButton))
  {
    CheckRotaryMode();
  }
  else
  {
    CheckRotaryGlobalMode();
  }
}

void CheckRotaryFrequencyStep()
{
  Serial.println("Rotary Step");
  FrequencyDisplay.clear();
  FrequencyDisplay.write(FrequencyRotaryStep+1,B01100011);
  delay(1000);
  while (digitalRead(FERotaryButton) == LOW)
  {
    FrequencyRotariState = FrequencyRotary.rotate();
// 0 = not turning, 1 = CW, 2 = CCW
    if ( FrequencyRotariState == 1 ) 
    {
      FrequencyRotaryStep++;
      FrequencyDisplay.clear();
      FrequencyDisplay.write(FrequencyRotaryStep+1,B01100011);
      delay(200);
    }
    if ( FrequencyRotariState == 2 ) 
    {
      FrequencyRotaryStep--;
      FrequencyDisplay.clear();
      FrequencyDisplay.write(FrequencyRotaryStep+1,B11100011);
      delay(200);
    }  
  }
}


void CheckRotaryFrequency()
{
  if (DebugLevel & 0B00000001)Serial.println("Rotary Frequency");
  // 0 = not turning, 1 = CW, 2 = CCW
  FrequencyRotariState = FrequencyRotary.rotate();
  if (DebugLevel & 0B00000001)Serial.print("Rotary state ");
  if (DebugLevel & 0B00000001)Serial.println(FrequencyRotariState);
  if ( FrequencyRotariState != 0 )
  {
    //Ruota in senso orario -> Aumenta la frequenza
    if ( FrequencyRotariState == 1 ) 
    {
      if (DebugLevel & 0B00000001)Serial.println("F Aumenta");
      Frequency[RefGen] += pow(10,FrequencyRotaryStep);
    }

    //Ruota in senso antiorario -> Diminuisce la frequenza
    if ( FrequencyRotariState == 2 ) 
    {
      if (DebugLevel & 0B00000001)Serial.println("F Diminuisce");
      Frequency[RefGen] -= pow(10,FrequencyRotaryStep);
    }

    //Controlla il fuori scala inferiore
    if (Frequency[RefGen]<FrequencyLimit[FrequencyWaveCurrentType[RefGen]][0])
    {
      Frequency[RefGen] = FrequencyLimit[FrequencyWaveCurrentType[RefGen]][1];
    }

    //Controlla il fuori scala superiore
    if (Frequency[RefGen]>FrequencyLimit[FrequencyWaveCurrentType[RefGen]][1])
    {
      Frequency[RefGen] = FrequencyLimit[FrequencyWaveCurrentType[RefGen]][0];
    }

    //Setta frequenza e forma d'onda
    AD9833FreqSet(Frequency[RefGen],FrequencyWaveType[FrequencyWaveCurrentType[RefGen]]);
    if (DebugLevel & 0B00000001)Serial.println("Ha settato F e forma d'onda");
    //Aggiorna il display
    DisplayFrenquency();

  }
    
}

void CheckRotaryGlobalMode()
{
  Serial.println("Rotary Global Mode");
  while (digitalRead(MERotaryButton) == LOW)
  {
    // 0 = not turning, 1 = CW, 2 = CCW
    ModeRotariState = ModeRotary.rotate();
  
    //Se non è stato girato salta tutto
    if ( ModeRotariState != 0 )
    {
      //Ruota in senso orario -> Aumenta il parametro del modo corrente
      if ( ModeRotariState == 1 ) 
      {
        GlobalMode++;
      }

      //Ruota in senso antiorario -> Diminuisce la frequenza
      if ( ModeRotariState == 2 ) 
      {
        GlobalMode--;
      }
  

      //Controlla il fuori scala superiore
      if (GlobalMode>=GlobalModeNumber)
      {
        GlobalMode = 0;
      }

      //Controlla il fuori scala inferiore
      if (GlobalMode<0)
      {
        GlobalMode = GlobalModeNumber-1;
      }
      GlobalModeDisplay();
    }
  }
}

void CheckRotaryMode()
{

  // 0 = not turning, 1 = CW, 2 = CCW
  ModeRotariState = ModeRotary.rotate();
  
  //Se non è stato girato salta tutto
  if ( ModeRotariState != 0 )
  {
    //Ruota in senso orario -> Aumenta il parametro del modo corrente
    if ( ModeRotariState == 1 ) 
    {
//      Serial.println("F Aumenta");
      FrequencyWaveCurrentType[RefGen] ++;
    }

    //Ruota in senso antiorario -> Diminuisce la frequenza
    if ( ModeRotariState == 2 ) 
    {
//      Serial.println("F Diminuisce");
      FrequencyWaveCurrentType[RefGen]--;
    }
  

    //Controlla il fuori scala superiore
    if (FrequencyWaveCurrentType[RefGen]>=WaveTypeNumber)
    {
      FrequencyWaveCurrentType[RefGen] = 0;
    }

    //Controlla il fuori scala inferiore
    if (FrequencyWaveCurrentType[RefGen]<0)
    {
      FrequencyWaveCurrentType[RefGen] = WaveTypeNumber-1;
    }

    //Setta frequenza e forma d'onda
    DrawModeGraph(FrequencyWaveCurrentType[RefGen]);
    
    AD9833FreqSet(Frequency[RefGen],FrequencyWaveType[FrequencyWaveCurrentType[RefGen]]);
    Serial.println("Ha settato F e forma d'onda");
    //Aggiorna il display
    DisplayFrenquency();

  }
}


void DisplayFrenquency()
{
  //Stampa la frequenza corrente allineata a destra
  FrequencyDisplay.clear();
  if(RefGen==0)
  {
    FrequencyDisplay.printDigit(Frequency[RefGen],0);
  }
  else
  {
    Frequency2Display.printDigit(Frequency[RefGen],0);
  }
}



// AD9833 documentation advises a 'Reset' on first applying power.
void AD9833reset() 
{
  AD9833WriteRegister(0x100);   // Write '1' to AD9833 Control register bit D8.
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

//-----------------------------------------------------------------------------
// AD9833freqSet
//    set the AD9833 frequency regs 
//-----------------------------------------------------------------------------
void AD9833FreqSet(long frequency, int wave) 
{
  long fl = frequency * (0x10000000 / 25000000.0);
  AD9833WriteRegister(0x2000 | wave);
  AD9833WriteRegister((int)(fl & 0x3FFF) | 0x4000);
  AD9833WriteRegister((int)((fl & 0xFFFC000) >> 14) | 0x4000);
}


//-----------------------------------------------------------------------------
// SevenSegHello
//    set Seven segment Hello Message 
//-----------------------------------------------------------------------------
void SevenSegHello()
{
  FrequencyDisplay.clear();
  Frequency2Display.clear();
  for(uint8_t ii=0;ii<FDisplayNumDigit; ii++)
  {
    FrequencyDisplay.write(ii+1,pgm_read_word_near(FHello+ ii));    
    Frequency2Display.write(ii+1,pgm_read_word_near(F2Hello+ ii));        
  }

}


//-----------------------------------------------------------------------------
// tftHello
//    set TFT Hello Message 
//-----------------------------------------------------------------------------
void tftHello() 
{
  tft.setTextWrap(false);
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(30, 10);
  tft.setTextColor(ST77XX_GREEN);
  tft.setTextSize(4);
  tft.println(Modello);
  tft.setCursor(40, 50);
  tft.setTextColor(ST77XX_YELLOW);
  tft.setTextSize(3);
  tft.println(Modello1);
  tft.setCursor(15, 80);
  tft.println(Modello2);
  tft.setCursor(15, 110);
  tft.setTextSize(2);
  tft.println("SaLe 2020");
  tft.setTextSize(1);
  tft.println();
  tft.print("Hardware Ver.");
  tft.println(Hardware);
  tft.print("Software Ver.");
  tft.println(Software);
  tft.setTextColor(ST77XX_BLUE);
}

//-----------------------------------------------------------------------------
// DrawModeGraph
//    Draw on tft the wave form and frequency limit
//-----------------------------------------------------------------------------
void DrawModeGraph(uint8_t Mode)
{
  switch (Mode)
  {
    case 0:
      DrawSine(64, 32,64, 120);
      break;
    case 1:
      DrawTriangle(64, 32,64, 120);
      break;
    case 2:
      DrawSquare(64, 32,64, 120);
      break;
    default:
      break;
  }
}

//-----------------------------------------------------------------------------
// DrawSine
//    Draw on tft the Sine sprite
//-----------------------------------------------------------------------------
void DrawSine(uint8_t Leng, float High,uint8_t Xx, uint8_t Yy)
{
  //Cancella il simbolo precedente e presenta i limiti in frequenza pert la forma d'onda corrente
  PrintFrequencyLimit(Leng, High,Xx, Yy);

  //Toglie lo spazio in basso per inserire i limiti di frequenza
  High-=10;
  float AlfHigh = High/2;
  for (uint8_t xx=0;xx<Leng;xx++)
  {
    for (uint8_t offset=0; offset<6; offset++)
    {
      tft.drawPixel(Xx+xx+offset, Yy+AlfHigh+(AlfHigh*sin((3.14/High)*float(xx))), ST77XX_GREEN);
    }
  }
}


//-----------------------------------------------------------------------------
// DrawTriangle
//    Draw on tft the Triangle sprite
//-----------------------------------------------------------------------------
void DrawTriangle(uint8_t Leng, uint8_t High,uint8_t Xx, uint8_t Yy)
{
  //Cancella il simbolo precedente e presenta i limiti in frequenza pert la forma d'onda corrente
  PrintFrequencyLimit(Leng, High,Xx, Yy);

  //Toglie lo spazio in basso per inserire i limiti di frequenza
  High-=10;
  for(uint8_t offset=0;offset<6;offset++)
  {
    tft.drawLine(           Xx+offset, Yy+High/2,     Xx+Leng/6+offset, Yy+High-1, ST77XX_GREEN);
    tft.drawLine(  Xx+(Leng/6)+offset, Yy+High-1, Xx+(3*Leng/6)+offset,        Yy, ST77XX_GREEN);
    tft.drawLine(Xx+(3*Leng/6)+offset,        Yy, Xx+(5*Leng/6)+offset, Yy+High-1, ST77XX_GREEN);
    tft.drawLine(Xx+(5*Leng/6)+offset, Yy+High-1,     Xx+Leng-5+offset, Yy+High/2, ST77XX_GREEN);
  }
}

//-----------------------------------------------------------------------------
// DrawSquare
//    Draw on tft the Square sprite
//-----------------------------------------------------------------------------
void DrawSquare(uint8_t Leng, uint8_t High,uint8_t Xx, uint8_t Yy)
{
  //Cancella il simbolo precedente e presenta i limiti in frequenza pert la forma d'onda corrente
  PrintFrequencyLimit(Leng, High,Xx, Yy);

  //Toglie lo spazio in basso per inserire i limiti di frequenza
  High-=10;
  for(uint8_t offset=0;offset<6;offset++)
  {
    tft.drawLine(         Xx+offset,        Yy,          Xx+offset,  Yy+High-1, ST77XX_GREEN);
    tft.drawLine(         Xx+offset, Yy+High-1,   Xx+Leng/3+offset,  Yy+High-1, ST77XX_GREEN);
    tft.drawLine(  Xx+Leng/3+offset, Yy+High-1,   Xx+Leng/3+offset,         Yy, ST77XX_GREEN);
    tft.drawLine(  Xx+Leng/3+offset,        Yy, Xx+2*Leng/3+offset,         Yy, ST77XX_GREEN);
    tft.drawLine(Xx+2*Leng/3+offset,        Yy, Xx+2*Leng/3+offset,  Yy+High-1, ST77XX_GREEN);
    tft.drawLine(Xx+2*Leng/3+offset, Yy+High-1,     Xx+Leng+offset,  Yy+High-1, ST77XX_GREEN);
    tft.drawLine(    Xx+Leng-offset,        Yy,     Xx+Leng-offset,  Yy+High-1, ST77XX_GREEN);
  }
}

//-----------------------------------------------------------------------------
// PrintFrequencyLimit
//    Draw on tft the Frequency limit for the wave sprite
//-----------------------------------------------------------------------------
void PrintFrequencyLimit(uint8_t Leng1, uint8_t High1,uint8_t Xx1, uint8_t Yy1)
{
  //Cancella il simbolo precedente
  tft.fillRect(Xx1,Yy1,Leng1, High1, ST77XX_BLACK);
  //Scrive i limiti di frequenza impostati
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(Xx1,Yy1+High1-8);
  tft.print(FrequencyLimit[FrequencyWaveCurrentType[RefGen]][0]);
  tft.print("Hz - ");
  tft.print(FrequencyLimit[FrequencyWaveCurrentType[RefGen]][1]/1000000);
  tft.print("MHz");
}


//-----------------------------------------------------------------------------
// DrawArrowFDisplay
//    Draw on tft the Arrow to indicate the frequency display status
//-----------------------------------------------------------------------------
void DrawArrowFDisplay(uint8_t NumDisplay, uint8_t ModeDisplay)
{
  switch (NumDisplay)
  {
    case 1:
    //Display 1 freccia ROSSA
      tft.fillTriangle(0, 135, 10, 125, 10, 145, ST77XX_RED);
      break;
    case 2:
    //Display 2 freccia ROSSA
      tft.fillTriangle(0, 25, 10, 15, 10, 35, ST77XX_RED);
      break;
    case 3:
    //Display 1 freccia VERDE
      tft.fillTriangle(0, 135, 10, 125, 10, 145, ST77XX_GREEN);
      break;
    case 4:
    //Display 2 freccia VERDE
      tft.fillTriangle(0, 25, 10, 15, 10, 35, ST77XX_GREEN);
      break;
    case 9:
    //Display 1 freccia NERA
      tft.fillTriangle(0, 135, 10, 125, 10, 145, ST77XX_BLACK);
      break;
    case 10:
    //Display 2 freccia NERA
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
    //Scrive 1 
      tft.setCursor(4,132);
      tft.print("1");
      break;
    case 2:
    //Scrive 2 
      tft.setCursor(4,23);
      tft.print("2");
      break;
    case 3:
    //Scrive L 
      tft.setCursor(4,132);
      tft.print("L");
      break;
    case 4:
    //Scrive H 
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



void DrawSweepDisplay(uint8_t Color)
{
  switch (Color)
  {
    case 0:
      tft.drawLine(10,135,13,135,ST77XX_BLACK);
      tft.drawLine(13,135,13,25,ST77XX_BLACK);
      tft.drawLine(10,25,13,25,ST77XX_BLACK);
      break;
    case 1:
      tft.drawLine(10,135,13,135,ST77XX_RED);
      tft.drawLine(13,135,13,25,ST77XX_RED);
      tft.drawLine(10,25,13,25,ST77XX_RED);
      break;
    default:
      break;
  }
}


//-----------------------------------------------------------------------------
// GlobalModeDisplay
//    Preset the tft and the seven segment display for the current operating mode
//-----------------------------------------------------------------------------
void GlobalModeDisplay()
{
  //Il comando seguente sembra ignorarlo probabilmente per problemi sul bis SPI
  //Inizializza il display tft
  tft.fillScreen(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  //comando ripetuto
  tft.fillScreen(ST77XX_BLACK);
  //Scrive la modalità sul TFT
  tft.setCursor(0,0);
  tft.print(GlobalModeLabel[GlobalMode]);
  switch (GlobalMode)
  {
    case 0:
    //Generazione a frequenza fissa
      //Freccia verde e numero 1 sul display 1
      DrawArrowFDisplay(3,1);
      //Forma d'onda per il display 1
      DrawModeGraph(FrequencyWaveType[FrequencyWaveCurrentType[RefGen]]);
      //Display frequenza 2 spento
      Frequency2Display.clear();
      //Setta la frequenza minima per la forma d'onda corrente
      AD9833FreqSet(Frequency[RefGen],FrequencyWaveType[FrequencyWaveCurrentType[RefGen]]);
      DisplayFrenquency();

      break;
    case 1:
    //Generazione sweep fra frequenza 1 e frequenza 2
      //Freccia rossa e lettera L sul display 1
      DrawArrowFDisplay(1,3);
      //Freccia verde e lettera H sul display 2
      DrawArrowFDisplay(4,4);
      //Forma d'onda per il display 1
      DrawModeGraph(FrequencyWaveType[FrequencyWaveCurrentType[RefGen]]);
      //Display frequenza 2 spento
      Frequency2Display.clear();
      break;
    default:
      break;
  }
}
