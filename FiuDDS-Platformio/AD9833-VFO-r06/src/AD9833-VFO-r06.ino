#include<Arduino.h>
#include<Wire.h>
/*
 * FIU
 * VFO a larga banda
 * Generatore di Funzioni in tecnologia DDS
 * Campo di frequenza 1Hz-12MHz
 * 
 */
//Debug Level 0 nessun messaggio di debug
//Debug Level 1 Solo messaggi relativi alla frequenza
#define DebugLevel 1

//#=================================================================================
// Identificativo
//#=================================================================================
#define Modello  "Fiu"
#define Modello1 "VFO"
#define Modello2 "FG DDS"
#define Hardware 2
#define Software 5

#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
#include <SPI.h>             // Arduino SPI library
#include <MD_AD9833.h>
#include <SimpleRotary.h>
#include <PString.h>

//IMPORTANT
//Use the last version of this libraries
#include <DigitLed72xx.h>
//#include <EEPROM.h>

//#=================================================================================
// AD9833 IC area
// DDS Generator
//#=================================================================================
#define AD9833DATA  11  ///< SPI Data pin number
#define AD9833CLK   13  ///< SPI Clock pin number
#define AD9833FSYNC 12  ///< SPI Load pin number (FSYNC in AD9833 usage)

//Generatore 0 Bassa Frequenza (1Hz to 12.5MHz)
//MD_AD9833  AD(FSYNC); // Hardware SPI
MD_AD9833 AD(AD9833DATA, AD9833CLK, AD9833FSYNC); // Arbitrary SPI pins
MD_AD9833::channel_t chan;

const int wSine     = 0b0000000000000000;
const int wTriangle = 0b0000000000000010;
const int wSquare   = 0b0000000000101000;

//Frequenza per i due generatori
//La 0 è la frequenza di generazione in Fixed Mode e la L in Sweep
//La 1 è la frequenza H in Sweep Mode
//La 2 è la frequenza corrente in Sweep Mode
#define FreqL 0
#define FreqH 1
#define FreqNow 2

float Frequency[3][2] = {1,1,1,
                        1,1,1};

//Generatore corrente
//Il generatore utilizzato è sempre lo 0
//Questo parametro verrà cambiato all'introduzione del secondo chip di sintesi (generatore)
#define Generator0 0
#define Generator1 1
uint8_t  CurrentGenerator=Generator0;

#define WaveTypeNumber 3

//Tipo corrente della forma d'onda per ogni generatore
#define Sine 0
#define Triangle 1
#define Square 2
int8_t  FrequencyWaveCurrentType[2] = {Sine,Sine};

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
// 2 -> Usa entrambe i generatori e li mixa

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

//Ultimo marcher temporale fissato (in microsecondi)
float TZero;
float TXs;
//Differenziale del tempo
float SweepTStep;
float FSweepStep;
float SweepCurrentFrequency;
//#=================================================================================
// MAX7219 IC area
//7 Segmenti per 8 digit - Display Frequenza
//I display collegati e gestiti sono 2 da 8 digit l'uno
//#=================================================================================
#define DisplayNumber 2
#define Digit4Display 8
#define FirstDigitDisplay1 8


#define FDisplayNumDigit 8
#define MAX7219CS    2
#define MAX7219CS3    3

//#define MAX7219CLK  13
//#define MAX7219Data 11

DigitLed72xx FrequencyDisplay = DigitLed72xx(MAX7219CS, DisplayNumber);

//Frase di apertura "ddS FG" - "Fiu"
const uint8_t FHello[16] PROGMEM = {B00000000,
                                    B01011110,
                                    B01000111,
                                    B00000000,
                                    B01011011,
                                    B00111101,
                                    B00111101,
                                    B00000000,//
                                    B00000000,
                                    B00000000,
                                    B00011100,
                                    B00000100,
                                    B01000111,
                                    B00000000,
                                    B00000000,
                                    B00000000}; 





//#=================================================================================
// ST7735 - 1,8" OLED area
//Display da 1.8" 128X160
//per la rappresentazione di forma d'onda, livello di uscita e altre funzioni
//#=================================================================================
// ST7735 TFT module connections
#define TFT_CS      4  // define chip select pin
#define TFT_DC      5  // define data/command pin
#define TFT_RST     6  // define reset pin, or set to -1 and connect to Arduino RESET pin

char SweepTimeBuff[9];

// Initialize Adafruit ST7735 TFT library
//Usa la SPI hardware. In questa configurazione hardware crea problemi la convivenza dei
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);


//Usa la SPI software. In questa configurazione si risolvono molti dei problemi causati da conflitti sul canale SPI
//#define TFT_MOSI 11  // Data out
//#define TFT_SCLK 13  // Clock out
// For ST7735-based displays, we will use this call
//Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

//#=================================================================================
// Encoder Rotary area
//Settaggio Frequenza e Step di avanzamento frequenza
//#=================================================================================
#define FERotaryA 7
#define FERotaryB 8
#define FERotaryButton 14
SimpleRotary FrequencyRotary(FERotaryA,FERotaryB,FERotaryButton);

uint8_t FrequencyRotaryState;
uint8_t FrequencyRotaryStep[2] = {0, 0};//potenza del 10 che si somma o sottrae alla frequenza
float FrequencyRotaryStepHerz[2];//Valore effettivo dello step da attuare espresso in Herz

//#=================================================================================
// Encoder Rotary area
//Settaggio forma d'onda e Modalità di generazione (singola frequenza, sweep, ...)
//#=================================================================================
#define MERotaryA  9
#define MERotaryB 10
#define MERotaryButton 15
SimpleRotary ModeRotary(MERotaryA,MERotaryB,MERotaryButton);

uint8_t ModeRotaryState;

#define NumOfMode 2
char *ModeLabel[] = {"Forma d'Onda", "Sweep"};

//#=================================================================================
// Pulsantiera area
//Settaggio varie funzioni
//#=================================================================================
#define PushGlobalMode 16
#define Push1 17
#define Push2 18
#define AntiBounceDelay 200

//#=================================================================================
// Serial
//#=================================================================================
#define SerialSpeed 115200


//#=================================================================================
// Setup
//#=================================================================================
void setup()
{

  //Inizializza gli I/O digitali
  SetupIO();

  //Inizializza le variabili di stato
  CurrentGenerator = Generator0;
  FrequencyWaveCurrentType [CurrentGenerator] = Sine;
  Frequency[FreqL][CurrentGenerator] = FrequencyLimit[FrequencyWaveCurrentType[CurrentGenerator]][CurrentGenerator];

  //Manda il messaggio di benvenuto sui display sette segmenti
  SevenSegHello();

  // Use this initializer if using a 1.8" TFT screen:
  tftHello();
  //lascia il tempo per leggere i messaggi di apertura
  delay(3000);
  
  //Inizializza il genertore 0
  AD.begin();
  AD9833reset();
  GlobalModeDisplay();  
}



//#=================================================================================
// Loop
//#=================================================================================
void loop()
{
  CheckControllPanel();
//  char  *cp;
//  
//  if ((cp = readPacket()) != NULL)
//    processPacket(cp);
  if (GlobalMode==GlobalModeSweepLtH)
  {
    //Se è finito il tempo di sweep, azzera i parametri e inizia una nuova rampa
    if (millis()-TZero>SweepTime)
    {
      //Frequenza Iniziale
      SweepCurrentFrequency=Frequency[0][0];
      //Azzera il contatore del tempo di Sweep
      TZero=millis();
      //Attualizza la frequenza
      AD9833FreqSet(SweepCurrentFrequency, FrequencyWaveType[FrequencyWaveCurrentType[0]],0);
      //Mette il tempo per lo step corrente uguale al tempo di inizio
      TXs=TZero;
    }
    else
    {
      //Se è trascorso il tempo di passo aggiorna la frequenza al prossimo passo
      //e il tempo relativo per il prossimo passo
      if (millis()-TXs>= SweepTStep)
      {
        SweepCurrentFrequency+=FSweepStep;
        //Attualizza la frequenza
        AD9833FreqSet(SweepCurrentFrequency, FrequencyWaveType[FrequencyWaveCurrentType[0]],0);
        TXs=millis();
      }
    }
  }
}

//#=================================================================================
// CheckControllPanel
// Su pressione del primo bottone a DX sul pannello principale provvede a passare
// alla modalità operativa successiva
//#=================================================================================
void CheckControllPanel()
{
  //Controlla lo stato del bottone di cambio GlobalMode e se non è premuto passa avanti
  if(digitalRead(PushGlobalMode)==LOW)
  {
    //Antiounce
    delay(AntiBounceDelay);
    if(digitalRead(PushGlobalMode)==LOW)
    {
      GlobalMode++;
      //Controlla il fuori scala superiore
      if (GlobalMode>=GlobalModeNumber)
      {
        GlobalMode = GlobalModeFixFrequency;
      }
      //Aggiorna il display
      GlobalModeDisplay();
      //Aspetta il rilascio del pulsante
      while(digitalRead(PushGlobalMode)==LOW)
      {
        //Serial.print("Aspetto che rilasci");
      }
    }
  }
  else
  {
    //Controlla se l'encoder principale è stato ruotato
    FrequencyRotaryState= FrequencyRotary.rotate();
    if (FrequencyRotaryState != 0)
    //Controlla se l'encoder principale è premuto
    {
      if (HIGH == digitalRead(FERotaryButton))
      //Se il rotore dell'encoder della frquenza NON è premuto ed è stato ruotato gestisce la frequenza
      {
        CheckRotaryFrequency();
      }
      else
      //Se è stato premuto gestisce l'aggiornamento del passo di incremento/decremento della frequenza
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

//#=================================================================================
// CheckRotaryFrequencyStep
// Gestisce il settaggio del passo per l'aggiornamento della frequenza.
// In questa versione, la frequenza è aggiornabile tramite manopola (encoder)e
// ad ogni passo della manopola corrisponde un icremento/decremento del modulo della
// frequenza.
// Il passo è definito per ogni generatore/display della frequenza
//#=================================================================================
void CheckRotaryFrequencyStep()
{
//  if (CurrentGenerator == 0)
//  //controlla se il generatore corrente è il primo
//  {
  FrequencyDisplay.clear(CurrentGenerator);//Pulisce il primo display a 7 segmenti
  //Presenta il carattere che indica la cifra di incremento del valore
  FrequencyDisplay.write(FrequencyRotaryStep[0]+1,B01100011);
  delay(10);//Antibounce

  while (digitalRead(FERotaryButton) == LOW)
  //Fin quando la manopola rimane premuta
 {
    FrequencyRotaryStep[CurrentGenerator]=CheckRotaryFrequencyStepRotate(FrequencyRotaryStep[CurrentGenerator]);
    FrequencyDisplay.clear(CurrentGenerator);
    FrequencyDisplay.write(FrequencyRotaryStep[CurrentGenerator]+1,B01100011);
  }
//  }
//  else
//  //Se il generatore non è il primo
//  {
//    //Frequency2Display.clear();//Pulisce il secondo display a 7 segmenti
//    //Presenta il carattere che indica la cifra di incremento del valore
//    //Frequency2Display.write(FrequencyRotaryStep[1]+1,B01100011);
//    delay(10);//Antibounce
//  
//    while (digitalRead(FERotaryButton) == LOW)
//    //Fin quando la manopola rimane premuta
//    {
//      FrequencyRotaryStep[1]=CheckRotaryFrequencyStepRotate(FrequencyRotaryStep[1]);
//      //Frequency2Display.clear();
//      //Frequency2Display.write(FrequencyRotaryStep[1]+1,B01100011);
//    }
//  }
  PrintFrequencyLimit(0);
}

//#=================================================================================
//#
//#=================================================================================
byte CheckRotaryFrequencyStepRotate(byte FStep)
//Gestisce la rotazione della manopola
{
  FrequencyRotaryState= FrequencyRotary.rotate();//Verifica se la manopola è stata ruotata
  // 0 = not turning, 1 = CW, 2 = CCW
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

//#=================================================================================
//#
//#=================================================================================
void CheckRotaryFrequency()
{
//Calcola lo step attuale effettivo per i generatori 0 e 1
//  FrequencyRotaryStepHerz[0]=pow(10,FrequencyRotaryStep[0]);
//  FrequencyRotaryStepHerz[1]=pow(10,FrequencyRotaryStep[1]);
  for (byte i=0;i<2;i++)
  {
    FrequencyRotaryStepHerz[i]=1;
    for(byte ii=0; ii>FrequencyRotaryStep[i]; ii++)
    {
      FrequencyRotaryStepHerz[i]*=10;
    }
    
  }

  //Modifica il valore della frequenza
  //Ruota in senso orario -> Aumenta la frequenza
  if(GlobalMode == GlobalModeModulation)
  {
    if ( FrequencyRotaryState== 1 ) 
    {
      Frequency[0][0] += FrequencyRotaryStepHerz[0];
    }
    //Ruota in senso antiorario -> Diminuisce la frequenza
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
    //Ruota in senso antiorario -> Diminuisce la frequenza
    if ( FrequencyRotaryState== 2 ) 
    {
      Frequency[CurrentGenerator][0] -= FrequencyRotaryStepHerz[CurrentGenerator];
    }
  }
  //Fa il controllo del fuori scala (differenziato in funzione della modalità di funzionamento)
  switch (GlobalMode)
  {
    case GlobalModeFixFrequency:
      //Controlla il fuori scala rispetto ai limiti assoluti per forma d'onda
      //Controlla il fuori scala inferiore
      if (Frequency[0][0]<FrequencyLimit[FrequencyWaveCurrentType[0]][0])
      {
        Frequency[0][0] = FrequencyLimit[FrequencyWaveCurrentType[0]][1];
      }
      //Controlla il fuori scala superiore
      if (Frequency[0][0]>FrequencyLimit[FrequencyWaveCurrentType[0]][1])
      {
        Frequency[0][0] = FrequencyLimit[FrequencyWaveCurrentType[0]][0];
      }
      //Setta frequenza e forma d'onda
      AD9833FreqSet(Frequency[CurrentGenerator][0],FrequencyWaveType[FrequencyWaveCurrentType[CurrentGenerator]],CurrentGenerator);
      break;
    case GlobalModeSweepLtH:
      //Controlla il fuori scala rispetto al limite superiore per forma d'onda e inferiore per F0+1
      //Controlla il fuori scala inferiore
      if (Frequency[1][0]<Frequency[0][0]+1)
      {
        Frequency[1][0] = FrequencyLimit[FrequencyWaveCurrentType[0]][1];
      }
      //Controlla il fuori scala superiore
      if (Frequency[1][0]>FrequencyLimit[FrequencyWaveCurrentType[0]][1])
      {
        Frequency[1][0] = Frequency[0][0]+1;
      }
      //Ricalcola i parametri di sweep
      SweepTStep = 1;
      FSweepStep=(Frequency[1][0]-Frequency[0][0])/(SweepTime/SweepTStep);
      if (FSweepStep<1.0)
      {
        SweepTStep= 1/FSweepStep;
        FSweepStep=(Frequency[1][0]-Frequency[0][0])/(SweepTime/SweepTStep);
      }
      break;
    case GlobalModeModulation:
      //F1
      //Controlla il fuori scala rispetto ai limiti assoluti per forma d'onda
      //Controlla il fuori scala inferiore
      if (Frequency[0][0]<FrequencyLimit[FrequencyWaveCurrentType[0]][0])
      {
        Frequency[0][0] = FrequencyLimit[FrequencyWaveCurrentType[0]][1];
      }
      //Controlla il fuori scala superiore
      if (Frequency[0][0]>FrequencyLimit[FrequencyWaveCurrentType[0]][1])
      {
        Frequency[0][0] = FrequencyLimit[FrequencyWaveCurrentType[0]][0];
      }
      //F2
      //Controlla il fuori scala rispetto ai limiti assoluti per forma d'onda
      //Controlla il fuori scala inferiore
      if (Frequency[0][0]<FrequencyLimit[FrequencyWaveCurrentType[0]][0])
      {
        Frequency[0][0] = FrequencyLimit[FrequencyWaveCurrentType[0]][1];
      }
      //Controlla il fuori scala superiore
      if (Frequency[0][0]>FrequencyLimit[FrequencyWaveCurrentType[0]][1])
      {
        Frequency[0][0] = FrequencyLimit[FrequencyWaveCurrentType[0]][0];
      }
      //Setta frequenza e forma d'onda F1
      AD9833FreqSet(Frequency[0][0],FrequencyWaveType[FrequencyWaveCurrentType[0]],0);
      //Setta frequenza e forma d'onda
      AD9833FreqSet(Frequency[1][0],FrequencyWaveType[FrequencyWaveCurrentType[1]],1);
      break;
    default:
      break;
  }
  //Aggiorna il display
  DisplayFrequency(CurrentGenerator);
}

void CheckRotaryModeStep()
{
  DrawSweepTime(10,60,8*(pow(10,SweepTimeStep)));
  delay(10);
  while (digitalRead(MERotaryButton) == LOW)
  {
    ModeRotaryState= ModeRotary.rotate();
// 0 = not turning, 1 = CW, 2 = CCW
    if ( ModeRotaryState== 1 ) 
    {
       SweepTimeStep++;
    }
    if ( ModeRotaryState== 2 ) 
    {
      SweepTimeStep--;
    }  
    //Controlla i fuori scala
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
//        Serial.println("Global 0 Wave");
        //Ruota in senso orario -> Aumenta il parametro del modo corrente
        if ( ModeRotaryState == 1 ) 
        {
          FrequencyWaveCurrentType[0]++;
        }

        //Ruota in senso antiorario -> Diminuisce la frequenza
        if ( ModeRotaryState == 2 ) 
        {
          FrequencyWaveCurrentType[0]--;
        }
  

        //Controlla il fuori scala superiore
        if (FrequencyWaveCurrentType[0]>=WaveTypeNumber)
        {
          FrequencyWaveCurrentType[0] = 0;
        }

        //Controlla il fuori scala inferiore
        if (FrequencyWaveCurrentType[0]<0)
        {
          FrequencyWaveCurrentType[0] = WaveTypeNumber-1;
        }

        //Setta frequenza e forma d'onda
        DrawModeGraph(FrequencyWaveCurrentType[0]);
    
        AD9833FreqSet(Frequency[0][0],FrequencyWaveType[FrequencyWaveCurrentType[0]],0);
        //Aggiorna il display
        DisplayFrequency(CurrentGenerator);
        break;
      case GlobalModeSweepLtH:
//        Serial.println("Global 1 Sweep time");
        //Ruota in senso orario -> Aumenta il parametro del modo corrente
        if ( ModeRotaryState == 1 ) 
        {
          SweepTime+=pow(10,SweepTimeStep);
        }

        //Ruota in senso antiorario -> Diminuisce la frequenza
        if ( ModeRotaryState == 2 ) 
        {
          SweepTime-=pow(10,SweepTimeStep);
        }
  
        //Controlla il fuori scala superiore
        if (SweepTime>=65500)
        {
          SweepTime = 1;
        }

        //Controlla il fuori scala inferiore
        if (SweepTime<1)
        {
          SweepTime = 65500;
        }
        DrawSweepTime(10,60, SweepTime);
        
        break;
      case GlobalModeModulation:
        //Ruota in senso orario -> Aumenta il parametro del modo corrente
        if ( ModeRotaryState == 1 ) 
        {
          Frequency[1][0]++;
        }

        //Ruota in senso antiorario -> Diminuisce la frequenza
        if ( ModeRotaryState == 2 ) 
        {
          Frequency[1][0]--;
        }
  

        //Controlla il fuori scala superiore
        if (Frequency[1][0]>=FrequencyLimit[FrequencyWaveCurrentType[1]][1])
        {
          Frequency[1][0] = FrequencyLimit[FrequencyWaveCurrentType[1]][0];
        }

        //Controlla il fuori scala inferiore
        if (Frequency[1][0]<FrequencyLimit[FrequencyWaveCurrentType[1]][0])
        {
          Frequency[1][0] = FrequencyLimit[FrequencyWaveCurrentType[1]][1];
        }

        //Setta frequenza e forma d'onda
//        DrawModeGraph(FrequencyWaveCurrentType[1]);
    
        AD9833FreqSet(Frequency[1][0],FrequencyWaveType[FrequencyWaveCurrentType[1]],1);
        //Aggiorna il display
        DisplayFrequency(CurrentGenerator);
        break;
      default:
        break;
    }
}

//#=================================================================================
//#DisplayFrequency(byte CurrentDisplay)
//#
//#Presenta la frequenza sul display CurrentDisplay
//#Il display è composto da 2 (o più) unità da 8 cifre collegate in serie (8+8)
//#il valore posto in CurrentDisplay permette di sapere da quale digit si deve
//#iniziare a scrivere
//#=================================================================================
void DisplayFrequency(byte CurrentDisplay)
{
  //Stampa la frequenza corrente allineata a destra
    FrequencyDisplay.clear(CurrentDisplay);
//    FrequencyDisplay.printDigit(Frequency[0][0],FirstDigit[CurrentDisplay],CurrentDisplay);
    FrequencyDisplay.printDigit(Frequency[0][0],CurrentDisplay);
}


//-----------------------------------------------------------------------------
// AD9833reset
// AD9833 documentation advises a 'Reset' on first applying power.
//-----------------------------------------------------------------------------
//
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


//-----------------------------------------------------------------------------
// SevenSegHello
//    set Seven segment Hello Message 
//-----------------------------------------------------------------------------
void SevenSegHello()
{
  /* Set the number of digit; brightness min:1, max:15 
     and clear display
  */
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


//-----------------------------------------------------------------------------
// tftHello
//    set TFT Hello Message 
//-----------------------------------------------------------------------------
void tftHello() 
{
  tft.initR(INITR_BLACKTAB);      // Init ST7735S chip, black tab
  
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
////  tft.setCursor(15, 105);
  tft.setCursor(30, 105);
  tft.setTextSize(2);
  tft.print("SaLe");
  tft.setCursor(5, 120);
  tft.println("DarioMAS");
  tft.setTextSize(1);
//  tft.println();
  tft.print("Hardware Ver.");
  tft.println(Hardware);
  tft.print("Software Ver.");
  tft.println(Software);
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

//-----------------------------------------------------------------------------
// DrawSine
//    Draw on tft the Sine sprite
//-----------------------------------------------------------------------------
void DrawSine(uint8_t Leng, float High,uint8_t Xx, uint8_t Yy)
{
  //Cancella il simbolo precedente e presenta i limiti in frequenza pert la forma d'onda corrente
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


//-----------------------------------------------------------------------------
// DrawTriangle
//    Draw on tft the Triangle sprite
//-----------------------------------------------------------------------------
void DrawTriangle(uint8_t Leng, uint8_t High,uint8_t Xx, uint8_t Yy)
{
  //Cancella il simbolo precedente e presenta i limiti in frequenza pert la forma d'onda corrente
  PrintFrequencyLimit(0);

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
  PrintFrequencyLimit(0);

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
void PrintFrequencyLimit(uint8_t NumBlocco)
{
  //Cancella il simbolo precedente
  tft.fillRect(84,112,43, 27, ST77XX_BLACK);
  //Scrive i limiti di frequenza impostati
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
      tft.fillTriangle(0, 120, 10, 110, 10, 130, ST77XX_RED);
      break;
    case 2:
    //Display 2 freccia ROSSA
      tft.fillTriangle(0, 25, 10, 15, 10, 35, ST77XX_RED);
      break;
    case 3:
    //Display 1 freccia VERDE
      tft.fillTriangle(0, 120, 10, 110, 10, 130, ST77XX_GREEN);
      break;
    case 4:
    //Display 2 freccia VERDE
      tft.fillTriangle(0, 25, 10, 15, 10, 35, ST77XX_GREEN);
      break;
    case 9:
    //Display 1 freccia NERA
      tft.fillTriangle(0, 120, 10, 110, 10, 130, ST77XX_BLACK);
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
      tft.setCursor(4,117);
      tft.print("1");
      break;
    case 2:
    //Scrive 2 
      tft.setCursor(4,23);
      tft.print("2");
      break;
    case 3:
    //Scrive L 
      tft.setCursor(4,117);
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


//-----------------------------------------------------------------------------
// GlobalModeDisplay
//    Preset the tft and the seven segment display for the current operating mode
//    Set the CurrentGenerator
//-----------------------------------------------------------------------------
void GlobalModeDisplay()
{
  TftGraphInit();

  switch (GlobalMode)
  {
    case GlobalModeFixFrequency:
    //Generazione a frequenza fissa
    //Pulisce i display 7 segmenti 0 e 1
      FrequencyDisplay.clear(0);
      FrequencyDisplay.clear(1);
      //Fissa il generatore corrente a 0 (Generatore di bassa frequenza)
      CurrentGenerator = 0;
      FrequencyDisplay.printDigit(Frequency[GlobalModeFixFrequency][CurrentGenerator], 0);
      //Freccia verde e numero 1 sul display 1
      DrawArrowFDisplay(3,1);
      //Forma d'onda per il display 1
      DrawModeGraph(FrequencyWaveType[FrequencyWaveCurrentType[CurrentGenerator]]);
      //Display frequenza 2 spento
      //Frequency2Display.clear();
      //Setta la frequenza minima per la forma d'onda corrente
      AD9833FreqSet(Frequency[CurrentGenerator][0],FrequencyWaveType[FrequencyWaveCurrentType[CurrentGenerator]],CurrentGenerator);
      DisplayFrequency(CurrentGenerator);
      break;
    case GlobalModeSweepLtH:
    //Generazione sweep fra frequenza 1 e frequenza 2
      CurrentGenerator = 1;
      //Calcola il tempo necessario ad impostare la frequenza
      SweepTStep=micros();
      //Simula il calcolo della prossima frequenza
      Frequency[CurrentGenerator][0]=Frequency[CurrentGenerator][0]+0;
      //Simula l'aggiornamento del generatore
      AD9833FreqSet(Frequency[CurrentGenerator][0],FrequencyWaveType[FrequencyWaveCurrentType[CurrentGenerator]],0);
      SweepTStep=(micros()-SweepTStep)/1000.0;
      DisplayFrequency(CurrentGenerator);
      delay(10);
      //Freccia rossa e lettera L sul display 1
      DrawArrowFDisplay(1,3);
      //Freccia verde e lettera H sul display 2
      DrawArrowFDisplay(4,4);
      //Forma d'onda per il display 1
      DrawModeGraph(FrequencyWaveType[FrequencyWaveCurrentType[CurrentGenerator]]);
      DrawSweepTime(10,60, SweepTime);
      CurrentGenerator = 1;
      //Inizializza la seconda frequenza (quella di arrivo)
      Frequency[1][0] = Frequency[0][0]+1;
      //Display frequenza 2
      DisplayFrequency(1);
      //Inizializza TZero per la misurazione del tempo di sweep
      TZero=millis();
      break;
    case GlobalModeSweepHtL:
    //Generazione sweep fra frequenza 1 e frequenza 2
      //Calcola il tempo necessario ad impostare la frequenza
      SweepTStep=micros();
      //Simula il calcolo della prossima frequenza
      Frequency[CurrentGenerator][0]=Frequency[CurrentGenerator][0]+0;
      //Simula l'aggiornamento del generatore
      AD9833FreqSet(Frequency[CurrentGenerator][0],FrequencyWaveType[FrequencyWaveCurrentType[CurrentGenerator]],0);
      SweepTStep=(micros()-SweepTStep)/1000.0;
      DisplayFrequency(CurrentGenerator);
      delay(10);
      //Freccia rossa e lettera L sul display 1
      DrawArrowFDisplay(1,3);
      //Freccia verde e lettera H sul display 2
      DrawArrowFDisplay(4,4);
      //Forma d'onda per il display 1
      DrawModeGraph(FrequencyWaveType[FrequencyWaveCurrentType[CurrentGenerator]]);
      DrawSweepTime(10,60, SweepTime);
      CurrentGenerator = 1;
      //Inizializza la seconda frequenza (quella di arrivo)
      Frequency[1][0] = Frequency[0][0]+1;
      //Display frequenza 2
      DisplayFrequency(1);
      //Inizializza TZero per la misurazione del tempo di sweep
      TZero=millis();
      break;
    case GlobalModeSweepLtHtL:
    //Generazione sweep fra frequenza 1 e frequenza 2
      //Calcola il tempo necessario ad impostare la frequenza
      SweepTStep=micros();
      //Simula il calcolo della prossima frequenza
      Frequency[CurrentGenerator][0]=Frequency[CurrentGenerator][0]+0;
      //Simula l'aggiornamento del generatore
      AD9833FreqSet(Frequency[CurrentGenerator][0],FrequencyWaveType[FrequencyWaveCurrentType[CurrentGenerator]],0);
      SweepTStep=(micros()-SweepTStep)/1000.0;
      DisplayFrequency(CurrentGenerator);
      delay(10);
      //Freccia rossa e lettera L sul display 1
      DrawArrowFDisplay(1,3);
      //Freccia verde e lettera H sul display 2
      DrawArrowFDisplay(4,4);
      //Forma d'onda per il display 1
      DrawModeGraph(FrequencyWaveType[FrequencyWaveCurrentType[CurrentGenerator]]);
      DrawSweepTime(10,60, SweepTime);
      CurrentGenerator = 1;
      //Inizializza la seconda frequenza (quella di arrivo)
      Frequency[1][0] = Frequency[0][0]+1;
      //Display frequenza 2
      DisplayFrequency(1);
      //Inizializza TZero per la misurazione del tempo di sweep
      TZero=millis();
      break;
    case GlobalModeModulation:
    //Generazione con modulazione fra F1 e F2
      //Freccia verde e lettera 1 sul display 1
      DrawArrowFDisplay(3,1);
      //Freccia verde e lettera 2 sul display 2
      DrawArrowFDisplay(4,2);
      //Forma d'onda per il display 1
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
  //Cancella il simbolo precedente
  tft.fillRect(Xx,Yy-1,80, 15, ST77XX_BLACK);
  delay(10);
  tft.fillRect(Xx,Yy-1,80, 15, ST77XX_BLACK);
  //Scrive il periodo sdi sweep corrente
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(Xx,Yy);
  //Converte in formato fisso SweepTime
  sprintf(SweepTimeBuff, "%05u mS", SweepTimeX);
  //Presenta su tft il tempo di sweep in formato fisso
  tft.print(SweepTimeBuff);
//  tft.print("mS");  
//  tft.setTextSize(1);
//  tft.setCursor(Xx,Yy+15);
//  tft.print(SweepTStep,3);
//  tft.print(" Delta T min");
}

void TftGraphInit()
{
  //Inizializza il display tft
  tft.setTextSize(1);
//  tft.setTextColor(ST77XX_WHITE);
  tft.fillScreen(ST77XX_BLACK);
  tft.drawLine(0,12, 159,12,ST77XX_WHITE);//Delimitatore inferiore campo MODO
  tft.drawRect(0,139,42,20,ST77XX_WHITE);//Funzione F1
  tft.drawRect(42,139,43,20,ST77XX_WHITE);//Funzione F2
  tft.drawRect(85,139,43,20,ST77XX_WHITE);//Funzione F3
  tft.drawRect(15,106,68,33,ST77XX_WHITE);//Limiti di frequenza
  tft.drawRect(83,106,45,33,ST77XX_WHITE);//Forma d'onda (Grafica)
  //Scrive la modalità sul TFT
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
