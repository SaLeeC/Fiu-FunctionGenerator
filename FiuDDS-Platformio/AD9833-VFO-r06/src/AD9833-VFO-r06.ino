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
#define Software 6

#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
#include <SPI.h>             // Arduino SPI library
#include <MD_AD9833.h>
#include <SimpleRotary.h>
//#include <PString.h>

//IMPORTANT
//Use the last version of this libraries
#include <DigitLed72xx.h>
//#include <EEPROM.h>

//#=================================================================================
// General Controll area
// Lo stato del generatore e delle funzioni di controllo
//Sono gestite tramite una parola al fine di rendere più semplice il controllo
//degli stati e l'attivazione dei moduli di programma 
//#=================================================================================
// FiuMode
// bit 7 
//            0 Fixed Frequency
//            1 Sweep
// bit 6-5
//            0 L to H
//            1 H to L
//            2 L to H to L
//            3 Center +- Delta
// bit 4
// bit 3
// bit 2
//           0 Encoder Sweep Time Normal
//           1 Encoder Sweep Time Push
// bit 1
//           0 Encoder Aux Normal
//           1 Encoder Aux Push
// bit 0 
//           0 Encoder Frequency Normal
//           1 Encoder Frequency Push
//

#define FixedFrequency 0
#define Swepp 128
#define SweepCenter 96
#define SweepLtoHtoL 64
#define SweepHtoL 32
#define SweepLtoH 0
#define EncoderAuxNormal 0
#define EncoderAuxPush 2
#define EncoderFrequencyNormal 0
#define EncoderFrequencyPush 1
uint8_t FiuMode = FixedFrequency +
                  SweepLtoH +
                  EncoderAuxNormal +
                  EncoderFrequencyNormal;

//#=================================================================================
// AD9833 IC area
// DDS Generator
//#=================================================================================
#define AD9833DATA  11  ///< SPI Data pin number
#define AD9833CLK   13  ///< SPI Clock pin number
#define AD9833FSYNC 12  ///< SPI Load pin number (FSYNC in AD9833 usage)

//Generatore 0 Bassa Frequenza (1Hz to 12.5MHz)
MD_AD9833  AD(AD9833FSYNC); // Hardware SPI
//MD_AD9833 AD(AD9833DATA, AD9833CLK, AD9833FSYNC); // Arbitrary SPI pins
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
#define GlobalModeSweep 1
#define GlobalModeSweepLtH 2
#define GlobalModeSweepHtL 3
#define GlobalModeSweepLtHtL 4
#define GlobalModeModulation 5
int8_t GlobalMode = GlobalModeFixFrequency;
String GlobalModeLabel[GlobalModeNumber]={"Fixed Freq",
                                          "Sweep",
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

// Initialize Adafruit ST7735 TFT library
//Usa la SPI hardware. In questa configurazione hardware crea problemi la convivenza dei
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);


//Usa la SPI software. In questa configurazione si risolvono molti dei problemi causati da conflitti sul canale SPI
//#define TFT_MOSI 11  // Data out
//#define TFT_SCLK 13  // Clock out
// For ST7735-based displays, we will use this call
//Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

//#=================================================================================
// TFT Controll area
// Lo stato del TFT è gestito tramite una parola al fine di rendere più semplice 
//il controllo degli stati e l'attivazione dei moduli di programma 
//#=================================================================================
// TftStatus
// bit 7 
//            0 Fixed Frequency Display Stay
//            1 Fixed Frequency Display Request
// bit 6
//            0 Sweep Frequency Display Stay
//            1 Sweep Frequency Display Request
// bit 5
// bit 4
// bit 3     0 PiP Display Stay
//           1 PiP Display Refresh
// bit 2
// bit 1-0
//           0 PiP Step OFF
//           1 PiP Main Frequency Step ON
//           2 PiP Aux Frequency Step ON
//           3 PiP Sweep Time Step ON
//

#define PipMainFreqOff 0
#define PipMainFreqON 1
#define PipAuxFreqON 2
#define PipRequestRefresh 4
#define FixedFreqRequest 128
#define SweepRequest 64
uint8_t TftStatus = FixedFreqRequest;
                    ;

//#=================================================================================
// Encoder Rotary area
//Settaggio Frequenza e Step di avanzamento frequenza
//#=================================================================================
#define FERotaryA 7
#define FERotaryB 8
#define FERotaryButton 14
SimpleRotary FrequencyRotary(FERotaryA,FERotaryB,FERotaryButton);
#define PushOff 0
#define PushOn 1

//#=================================================================================
// Encoder Rotary controll area
// Lo stato degli encoder (rotazione CCW - 0 - Rotazione CW) è gestita tramite una
// parola al fine di rendere più semplice il controllo degli stati e l'attivazione
// dei moduli di programma 
//#=================================================================================
// RotaryState
// bit 7 
//            0 Fixed Frequency
//            1 Sweep
// bit 6
// bit 5-4
//           0 Encoder Sweep Time No Turn
//           1 Encoder Sweep Time CW
//           2 Encoder Sweep Time CCW
// bit 3-2
//           0 Encoder Aux No Turn
//           1 Encoder Aux CW
//           2 Encoder Aux CCW
// bit 1-0
//           0 Encoder Frequency No Turn
//           1 Encoder Frequency CW
//           2 Encoder Frequency CCW
//

uint8_t RotaryState = 0;

//uint8_t FrequencyRotaryState;
uint8_t FrequencyStepEsponent[3] = {0,0,0};//potenza del 10 che si somma o sottrae alla frequenza
float FrequencyStepValue[3] = {1,1,1};//Valore effettivo dello step da attuare espresso in Herz
char *FrequencyStepLabel[] = {"F L Step",
                              "F H Step",
                              "Sweep T"};
//#=================================================================================
// Encoder Rotary area
//Settaggio forma d'onda e Modalità di generazione (singola frequenza, sweep, ...)
//#=================================================================================
#define MERotaryA  9
#define MERotaryB 19
#define MERotaryButton 15
SimpleRotary AuxRotary(MERotaryA,MERotaryB,MERotaryButton);

uint8_t ModeRotaryState;

#define NumOfMode 2
char *ModeLabel[] = {"Forma d'Onda", "Sweep"};

//#=================================================================================
// Encoder Rotary area
//Settaggio Tempo di Sweep
//#=================================================================================
#define SweepTimeRotaryA  20
#define SweepTimeRotaryB 21
#define SweepTimeRotaryButton 3
SimpleRotary SweepTimeRotary(SweepTimeRotaryA,SweepTimeRotaryB,SweepTimeRotaryButton);

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


uint32_t Counter = 0;

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

  //Inizializza il display TfT con la grafica per modalià Fixed Frequency
  TftGraphInit();
}



//#=================================================================================
// Loop
//#=================================================================================
void loop()
{
  //Controlla se è stato premuto il bottone per modificare la modalità di
  //funzionamento corrente e nel caso, aggiorna la presentazione grafica del TFT
  CheckControllPanel();

  //Controlla se gli encoder sono premuti
  //RotaryPush();

  //controlla se è premuto l'encoder principale (frequenza)
  //Il controllo è fuori dalla scelta della modalità perchè è comune
  //ad entrambe le modalità
/*   if (bitRead(FiuMode,0))
  {
    PoPManagment();
  }
 */  //Se non è attivo
/*   else
  {
    //Se la La PoP risulta ancora attiva la disattiva
    if ((TftStatus & B00000011) == 1)
    {
      //Segna come spenta la PoP
      TftStatus = TftStatus & B11111100;
      //Richiede l'aggiornamento della finestra di base per la MOdalità
      //corrente
      //Se è in modalità Sweep
      if (FiuMode & B10000000)
      {
        TftStatus = bitSet(TftStatus, 6);
      }
      //altrimenti è in modalità Fixed Frequency
      else
      {
        TftStatus = bitSet(TftStatus, 7);
      }
  
      //Ricrea la finestra di base
      TftGraphInit();
  
    }
  }//Se la modalità di funzionamento è Fixed Frequency
 */  //non servono altri controlli diversamente se la 
  //modalità di funzionamento impostata
  //è SWEEP
/*   if (FiuMode & B10000000)
  {
    //Aggiorna lo stato dei Push degli encoder
    //RotaryPush();
    //Se l'encoder ausiliario o quello dei tempi di sweep sono attivi
    //li gestisce 
    if (FiuMode & B00000110)
    {
 */
      PoPManagment();
//    }
/*   }
  //Se è falso è in modalità Fixed Frequency
  else
  {
  }
 */  




}

//#=================================================================================
// CheckControllPanel
// Su pressione del primo bottone a DX sul pannello principale provvede a passare
// alla modalità operativa successiva
//#=================================================================================
void CheckControllPanel()
{
  //Il cambio di modalità di funzionamento è possibile SOLO se non c'è nessun push
  //attivo
  if ((FiuMode & B00000111) != 0)
  {
    //cancella la dicitura F1 -> Mode
  }
  else
  {
    //Controlla lo stato del bottone di cambio GlobalMode e se non è premuto passa avanti
    if(digitalRead(PushGlobalMode)==LOW)
    {
      //Antiounce
      delay(AntiBounceDelay);
      if(digitalRead(PushGlobalMode)==LOW)
      {
        //Inverte la modalità di funzionamento (fixed / Sweep)
        bitWrite(FiuMode,7,(!bitRead(FiuMode,7)));
        //Prepara l'aggiornamento del TfT
        if (FiuMode & B10000000)//128 in FiuMode = Sweep
        {
          TftStatus = SweepRequest;
        }
        else
        {
          TftStatus = FixedFreqRequest;
        }
        //Aggiorna il display TFT
        TftGraphInit();
        //Aspetta il rilascio del pulsante
        while(digitalRead(PushGlobalMode)==LOW)
        {
        }
      }
    }
  }
}




//#=================================================================================
//#PoPManagment()
//#Gestisce la creazione e l'aggiornamento delle PoP
//#=================================================================================
void PoPManagment()
{
  RotaryPush();
  uint8_t Indice = 9;//9 è il valore di indice non settato
  //Converte i tre bit di stato nell'indice per gli array associati agli encoder  
  switch (FiuMode & B0000111)
  {
  case 1:
    Indice = 0;//Punta a Frequenza L
    break;
  case 2:
    Indice = 1;//Punta a Frequenza H
    break;
  case 4:
    Indice = 2;//Punta a Tempo di Sweep
    break;
  default:
    Indice = 9;//Indice sconosciuto
    break;
  }
  //Se nessuna PoP è attiva ma è richiesta l'attivazione 
  //CREA
  //la PoP e gli assegna il titolo, il primo valore
  //e l'unità di misura
  if (((TftStatus & B0000011) == 0) & (Indice != 9))
  {
    //Crea la PoP
    TftPopCreate(Indice);
    //Forza lo stato della PoP in Da Aggiornare
    bitSet(TftStatus,3);
    //e lo aggiorna
    //L'aggiornamento è differenziato asseconda se si interviene su frequenze e sul
    //tempo di sweep
    if (Indice<2)
    {
      //Aggiorna la PoP per gli step delle frequenze (L e H)
      TftPopPrint("Hz",FrequencyStepValue[Indice]);
      //Aggiorna il display a sette segmenti relativo alla frequenza corrente
      FrequencyDisplay.clear(Indice);
      FrequencyDisplay.write(FrequencyStepEsponent[Indice]+1, B01100011, Indice);
    }
    if (Indice == 2)
    {
      //Aggiorna la PoP per lo step dello Sweep
      TftPopPrint("uS",FrequencyStepValue[Indice]);
      //Pulisce i display a sette segmenti 
      FrequencyDisplay.clear(0);
      FrequencyDisplay.clear(1);
    }
  }
  //Se una PoP è
  //ATTIVA
  // la gestisce
  if ((TftStatus & B0000011) != 0)
  {
    //Legge la rotazione degli encoder
    RotaryTurn();
    //Se è stato ruotato un encoder
    if (RotaryState != 0)
    {
    switch (TftStatus & B00000011)
      {
        case 1:
          //Pulisce i dati provenienti dagli encoder Aux e Sweep Time
          RotaryState = RotaryState & B00000011;
          //Attua l'azione collegata alla rotazione della manopola
          FrequencyStepEsponent[Indice] = RotaryTurnAction(FrequencyStepEsponent[Indice], 1.0, 0, 6);
          FrequencyStepValue[Indice] = 1;
          for (uint8_t ii = FrequencyStepEsponent[Indice]; ii != 0; ii--)
          {
            FrequencyStepValue[Indice] = FrequencyStepValue[Indice] * 10.0;
          } 
          //Aggiorna la visualizzazione dei limiti
          TftFrequencyLimit(); 
          //Forza lo stato della PoP in "Da Aggiornare"
          bitSet(TftStatus,3);
          //e lo aggiorna
          TftPopPrint("Hz",FrequencyStepValue[Indice]);
  
          FrequencyDisplay.clear(Indice);
          FrequencyDisplay.write(FrequencyStepEsponent[Indice] + 1, B01100011, Indice);
  
          break;
        case 2:
          //Pulisce i dati provenienti dagli encoder Frequency e Sweep Time e allinea a Dx
          RotaryState = RotaryState >> 2;
          RotaryState = RotaryState & B00000011;
  
           //Attua la azione collegata alla rotazione della manopola
          FrequencyStepEsponent[Indice] = RotaryTurnAction(FrequencyStepEsponent[Indice], 1.0, 0, 6);
          FrequencyStepValue[Indice] = 1;
          for (uint8_t ii = FrequencyStepEsponent[Indice]; ii != 0; ii--)
          {
            FrequencyStepValue[Indice] = FrequencyStepValue[Indice] * 10.0;
          } 
          //Aggiorna la visualizzazione dei limiti
          TftFrequencyLimit(); 
          //Forza lo stato della PoP in "Da Aggiornare"
          bitSet(TftStatus,3);
          //e lo aggiorna
          TftPopPrint("Hz",FrequencyStepValue[Indice]);
  
          FrequencyDisplay.clear(Indice);
          FrequencyDisplay.write(FrequencyStepEsponent[Indice] + 1, B01100011, Indice);
  
          break;
        case 3:
          FrequencyDisplay.clear(0);
          FrequencyDisplay.clear(1);
          //Pulisce i dati provenienti dagli encoder Frequency e Sweep Time e allinea a Dx
          RotaryState = RotaryState >> 4;
          RotaryState = RotaryState & B00000011;
          //Attua la azione collegata alla rotazione della manopola
           //Attua la azione collegata alla rotazione della manopola
          FrequencyStepEsponent[Indice] = RotaryTurnAction(FrequencyStepEsponent[Indice], 1.0, 0, 6);
          FrequencyStepValue[Indice] = 1;
          for (uint8_t ii = FrequencyStepEsponent[Indice]; ii != 0; ii--)
          {
            FrequencyStepValue[Indice] = FrequencyStepValue[Indice] * 10.0;
          } 
          //Aggiorna la visualizzazione dei limiti
          TftFrequencyLimit(); 
          //Forza lo stato della PoP in "Da Aggiornare"
          bitSet(TftStatus,3);
          //e lo aggiorna
          TftPopPrint("uS",FrequencyStepValue[Indice]);
  
          break;
        default:
          break;
      }
    }
  }
  //Se tutte le richieste di PoP sono
  //NON ATTIVE
  //e lo stato del TFT riporta ancora finestre attive
  //Riporta il sistema alla condizione originale
  //in funzione della Modalità impostata
  if (((FiuMode & B00000111) == 0) & (TftStatus & B00000011) != 0)
  {
    //Segna come spente tutte le PoP
    TftStatus = TftStatus & B11111100;
    //Richiede l'aggiornamento della finestra di base per la MOdalità
    //corrente
    //Se è in modalità Sweep
    if (FiuMode & B10000000)
    {
      TftStatus = bitSet(TftStatus, 6);
    }
    //altrimenti è in modalità Fixed Frequency
    else
    {
      TftStatus = bitSet(TftStatus, 7);
    }

    //Ricrea la finestra di base
    TftGraphInit();
  }
}




//#=================================================================================
//#DisplayFrequency(uint8_t CurrentDisplay)
//#
//#Presenta la frequenza sul display CurrentDisplay
//#Il display è composto da 2 (o più) unità da 8 cifre collegate in serie (8+8)
//#il valore posto in CurrentDisplay permette di sapere da quale digit si deve
//#iniziare a scrivere
//#=================================================================================
void DisplayFrequency(uint8_t CurrentDisplay)
{
  //Stampa la frequenza corrente allineata a destra
    FrequencyDisplay.clear(CurrentDisplay);
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
// GlobalModeDisplay
//    Preset the tft and the seven segment display for the current operating mode
//    Set the CurrentGenerator
//-----------------------------------------------------------------------------

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
//  sprintf(SweepTimeBuff, "%05u mS", SweepTimeX);
  //Presenta su tft il tempo di sweep in formato fisso
//  tft.print(SweepTimeBuff);
//  tft.print("mS");  
//  tft.setTextSize(1);
//  tft.setCursor(Xx,Yy+15);
//  tft.print(SweepTStep,3);
//  tft.print(" Delta T min");
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
// TftFrequencyLimit
//    Draw on tft the Frequency limit for the wave sprite
//-----------------------------------------------------------------------------
void TftFrequencyLimit()
{
  //Cancella il simbolo precedente
  tft.fillRect(76,112,51, 27, ST77XX_BLACK);
  //Cancella i limiti precedenti
  tft.fillRect(1,112, 73, 24, ST77XX_BLACK);
  //Scrive i limiti di frequenza impostati
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(2,128);
  tft.print("min");
  //Stampa il valore allineato a Dx tramite la routines specializzata
  TftPrintIntDxGiustify(59, 128, 1, FrequencyLimit[FrequencyWaveCurrentType[CurrentGenerator]][0]);
  tft.setCursor(59,128);
  tft.print("Hz");
  tft.setCursor(2,119);
  tft.print("Step");
  //Stampa il valore allineato a Dx tramite la routines specializzata
  TftPrintIntDxGiustify(59, 119, 1, FrequencyStepValue[0]);
  tft.setCursor(59,119);
  tft.print("Hz");
  tft.setCursor(2,110);
  tft.print("MAX");
  //Stampa il valore allineato a Dx tramite la routines specializzata
  TftPrintIntDxGiustify(59, 110, 1, FrequencyLimit[FrequencyWaveCurrentType[CurrentGenerator]][1]/1000000);
  tft.setCursor(59,110);
  tft.print("MHz");
  //Se è in modalità Sweep aggiorna anche i parametri della seconda frequenza
  if (bitRead(FiuMode,7) & ((TftStatus & B00000011) == 0)) 
  {
    //Cancella il simbolo precedente
    tft.fillRect(1,20,51, 31, ST77XX_BLACK);
    //Cancella i limiti precedenti
//    tft.fillRect(1,20, 73, 31, ST77XX_BLACK);
    //Scrive i limiti di frequenza impostati
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(2,43);
    tft.print("min");
    //Stampa il valore allineato a Dx tramite la routines specializzata
    TftPrintIntDxGiustify(59, 43, 1, FrequencyLimit[FrequencyWaveCurrentType[CurrentGenerator]][0]);
    tft.setCursor(59,43);
    tft.print("Hz");
    tft.setCursor(2,34);
    tft.print("Step");
    //Stampa il valore allineato a Dx tramite la routines specializzata
    TftPrintIntDxGiustify(59, 34, 1, FrequencyStepValue[1]);
    tft.setCursor(59,34);
    tft.print("Hz");
    tft.setCursor(2,25);
    tft.print("MAX");
    //Stampa il valore allineato a Dx tramite la routines specializzata
    TftPrintIntDxGiustify(59, 25, 1, FrequencyLimit[FrequencyWaveCurrentType[CurrentGenerator]][1]/1000000);
    tft.setCursor(59,25);
    tft.print("MHz");
  }
}

//-----------------------------------------------------------------------------
// TftCurrentWave
//    Draw on tft the wave form and frequency limit
//-----------------------------------------------------------------------------
void TftCurrentWave(uint8_t Mode)
{
  switch (Mode)
  {
    case 0:
      TftDrawSine(45, 25,77, 110);
      break;
    case 1:
      TftDrawTriangle(45, 25,77, 110);
      break;
    case 2:
      TftDrawSquare(45, 25,77, 110);
      break;
    default:
      break;
  }
}

//-----------------------------------------------------------------------------
// TftDrawSine
//    Draw on tft the Sine sprite
//-----------------------------------------------------------------------------
void TftDrawSine(uint8_t Leng, float High,uint8_t Xx, uint8_t Yy)
{
  //Cancella il simbolo precedente e presenta i limiti in frequenza pert la forma d'onda corrente
  TftFrequencyLimit();

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
// TftDrawTriangle
//    Draw on tft the Triangle sprite
//-----------------------------------------------------------------------------
void TftDrawTriangle(uint8_t Leng, uint8_t High,uint8_t Xx, uint8_t Yy)
{
  //Cancella il simbolo precedente e presenta i limiti in frequenza pert la forma d'onda corrente
  TftFrequencyLimit();

  for(uint8_t offset=0;offset<6;offset++)
  {
    tft.drawLine(           Xx+offset, Yy+High/2,     Xx+Leng/6+offset, Yy+High-1, ST77XX_GREEN);
    tft.drawLine(  Xx+(Leng/6)+offset, Yy+High-1, Xx+(3*Leng/6)+offset,        Yy, ST77XX_GREEN);
    tft.drawLine(Xx+(3*Leng/6)+offset,        Yy, Xx+(5*Leng/6)+offset, Yy+High-1, ST77XX_GREEN);
    tft.drawLine(Xx+(5*Leng/6)+offset, Yy+High-1,     Xx+Leng-5+offset, Yy+High/2, ST77XX_GREEN);
  }
}

//-----------------------------------------------------------------------------
// TftDrawSquare
//    Draw on tft the Square sprite
//-----------------------------------------------------------------------------
void TftDrawSquare(uint8_t Leng, uint8_t High,uint8_t Xx, uint8_t Yy)
{
  //Cancella il simbolo precedente e presenta i limiti in frequenza pert la forma d'onda corrente
  TftFrequencyLimit();

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


//#=================================================================================
//#TftGraphInit()
//#Compone la videata di base per le modalità Fixed Frequency e Sweep
//#NON presenta nessun valore ma solo la fincatura, le label e il titolo
//#=================================================================================
void TftGraphInit()
{
  //Controlla se è stato richiesto un aggiornamento del Display
  if ((TftStatus & B11000000) != 0)
  {
    //Inizializza il display tft con gli elementi comuni
    tft.fillScreen(ST77XX_BLACK);
    tft.drawLine(0,19, 159,19,ST77XX_WHITE);//Delimitatore inferiore campo MODO
    tft.drawRect(0,139,42,20,ST77XX_WHITE);//Funzione F1
    tft.drawRect(42,139,43,20,ST77XX_WHITE);//Funzione F2
    tft.drawRect(85,139,43,20,ST77XX_WHITE);//Funzione F3
    tft.drawRect(0,106,75,33,ST77XX_WHITE);//Limiti di frequenza
    tft.drawRect(75,106,53,33,ST77XX_WHITE);//Forma d'onda (Grafica)
    //Scrive la modalità sul TFT
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(18,141);
    tft.print("F1");
    tft.setCursor(12,151);
    tft.print("Mode");
    tft.setCursor(57,141);
    tft.print("F2");    
    tft.setCursor(100,141);
    tft.print("F3");  
    //Stampa i limiti di frequenza (servono in entrambe le modalità)
    TftFrequencyLimit(); 
    //Stampa la forma d'onda generata (serve in entrambe le modalità)
    TftCurrentWave(0);
    tft.setCursor(0,0);
    tft.setTextSize(2);
    //Modalità Frequenza Fissa
    if(TftStatus & B10000000)
    {
      //Aggiunge gli elementi specifici per la Fixed Frequency
      tft.print(GlobalModeLabel[GlobalModeFixFrequency]);

    } 
    else
    //Aggiunge gli elementi specifici per la Sweep
    {
      tft.print(GlobalModeLabel[GlobalModeSweep]);
      tft.drawRect(0,19,75,33,ST77XX_WHITE);//Limiti di frequenza
      tft.drawRect(75,19,53,33,ST77XX_WHITE);//Tempo di Sweep
      //Aggiunge la descrizione al pulsante Funzione 2
      tft.setCursor(43,151);
      tft.setTextSize(1);
      tft.print("Sw.Mode");

    }
  //Segnala che l'aggiornamento del TfT è stato effettuato
  TftStatus = TftStatus & B00111111;
  }
}

//#=================================================================================
//#TftPopCreate(uint8_t Titolo)
//#Crea la PoP e gli mette il titolo
//#La PoP creata è vuota nella parte relativa al valore
//#=================================================================================
void TftPopCreate(uint8_t Titolo)
{
#define PipRadius 5
#define XPip 0
#define YPipTitle 30
#define YPipValue 65
#define PipLarg 123
#define PipHight 30
  tft.drawRoundRect((XPip + 5),YPipTitle,PipLarg,PipHight,PipRadius,ST77XX_WHITE);
  tft.fillRoundRect((XPip + 6),(YPipTitle+1),(PipLarg-2),(PipHight-2),PipRadius, ST77XX_BLUE);
  tft.drawRoundRect(XPip,(YPipTitle - 5),PipLarg,PipHight,PipRadius, ST77XX_BLACK);
  tft.fillRoundRect((XPip + 1),26,121,28,PipRadius, ST77XX_YELLOW);
  tft.setTextSize(2);
  tft.setTextColor(ST7735_BLACK);
  tft.setCursor(10,35);
  tft.print(FrequencyStepLabel[Titolo]);
  tft.drawRoundRect((XPip + 5),YPipValue,PipLarg,PipHight,PipRadius,ST77XX_WHITE);
  tft.fillRoundRect((XPip + 6),66,(PipLarg-2),(PipHight-2),PipRadius,ST77XX_BLUE);
  tft.drawRoundRect(XPip,60,PipLarg,PipHight,PipRadius,ST77XX_BLACK);
  tft.fillRoundRect((XPip + 1),61,(PipLarg-2),(PipHight-2),PipRadius,ST77XX_YELLOW);
  //Azzera lo stato precedente delle PiP (misura cautelativa)
  TftStatus = TftStatus & B11111100;
  //Setta lo stato delle PiP alla condizione attuale
  TftStatus = TftStatus | (Titolo+1);
}

//#=================================================================================
//#TftPopPrint(char *UnitaMisura, uint32_t Misura)
//#Se è richiesto il refresh del vaolore presentato nella PoP
//#Pulisce l'area dal precedente valore e chiama la funzione generica che
//#scrive il valore e l'unità di misura allineati a Dx
//#e alla fine resetta il flag del refresch
//#=================================================================================
void TftPopPrint(char *UnitaMisura, uint32_t Misura)
{
#define FontLarg 12
#define PipEndSpace 2
#define PipFontY 65
#define PipEndArea 122
  //Aggiorna il contenuto del PoP SOLO se richiesto
  if(bitRead(TftStatus,3))
  {
    tft.setTextSize(2);//Moltiplica la dimensione del font di default (5*8)
    tft.setTextColor(ST77XX_BLACK);
    //pulisce il valore precedente
    tft.fillRect(1,61,121,28,ST77XX_YELLOW);
    //Calcola la lunghezza dell'Unità di Misura (in pixel)
    uint8_t XUnitMisLeng = ((strlen(UnitaMisura) * FontLarg) + PipEndSpace);
    //Calcola la posizione dell'unità di misura
    uint8_t Xx = PipEndArea - XUnitMisLeng;
    //Posiziona il cursore
    tft.setCursor(Xx,PipFontY);
    //Stampa
    tft.print(UnitaMisura);
    //Calcola il numero massimo di cifre presentabili
    uint8_t MaxLength = ((121-XUnitMisLeng)/FontLarg);
    //Stampa il valore allineato a Dx tramite la routines specializzata
    TftPrintIntDxGiustify(Xx, PipFontY, 2, Misura);
    //Indica che il PiP è attivo ed è stato attualizzato il valore presentato
    bitClear(TftStatus,3);
  }
}
  
//#=================================================================================
//#TftPrintIntDxGiustify(uint8_t Xxx, uint8_t Yyy, uint8_t FontMultiplyer, uint32_t Value)
//#Funzione generica che
//#Scrive il valore e l'unità di misura allineati a destra
//#=================================================================================
void TftPrintIntDxGiustify(uint8_t Xxx, uint8_t Yyy, uint8_t FontMultiplyer, uint32_t Value)
{
//Font Large considera la larghezza del font base e dello spazio fra un carattere e l'altro
#define FontLargBase 6
  //Presenta la Misura allineandola a destra rispetto all'Unità di Misura
  //Converte il fontMultiplyer in FontSize
  FontMultiplyer = FontLargBase * FontMultiplyer;
  if (Value == 0)
  //Gestisce il caso in cui si vuole presentare il valor 0
  {
    Xxx-= FontMultiplyer;
    tft.setCursor(Xxx,Yyy);
    tft.print(Value % 10);
  }
  while (Value != 0)
  {
    Xxx-= FontMultiplyer;
    tft.setCursor(Xxx,Yyy);
    tft.print(Value % 10);
    Value /= 10;
  }
}


//#=================================================================================
//#
//#=================================================================================
void SetupIO()
{
  pinMode(AD9833DATA, OUTPUT);
  pinMode(AD9833CLK, OUTPUT);
  pinMode(AD9833FSYNC, OUTPUT);


  pinMode(PushGlobalMode, INPUT_PULLUP);
  pinMode(Push1, INPUT_PULLUP);
  pinMode(Push2, INPUT_PULLUP);

  pinMode(MAX7219CS, OUTPUT);

  //Setta le opzioni per la gestione degli encoder
  FrequencyRotary.setDebounceDelay(5);
  AuxRotary.setDebounceDelay(5);
  FrequencyRotary.setErrorDelay(100);
  AuxRotary.setErrorDelay(100);
}

//#=================================================================================
//#
//#=================================================================================
void RotaryPush()
{
  //Se c'è un push attivo deve essere gestito solo quello e se ci sono più push
  //attivi vengono resettati
  switch (FiuMode & B00000111)
  {
    case 1: //Push della frequenza principale attivo
      //La frequenza principale può andare in modalità Push in tutti le modalità 
      if (FrequencyRotary.push() == 1)
      {
        //Disattiva lo stato del bit 0
        bitClear(FiuMode,0);
        //Fa un ritardo arbitrario per evitare eventuali spurie
        delay(50);
      }
      break;
    case 2:
      //La frequenza Aux può andare in modalità Push solo in modalità Sweep 
      if (AuxRotary.push() == 1)
      {
        //Disattiva lo stato del bit 1
        bitClear(FiuMode,1);
        //Fa un ritardo arbitrario per evitare eventuali spurie
        delay(50);
      }
      break;
    case 4:
      //Il tempo di Sweep può andare in modalità Push solo in modalità Sweep
      if (SweepTimeRotary.push() == 1)
      {
        //Disattiva lo stato del bit 2
        bitClear(FiuMode,2);
        //Fa un ritardo arbitrario per evitare eventuali spurie
        delay(50);
      }
      break;
    default: //Resetta tutti i push
      FiuMode = FiuMode & B11111000;
      break;
  }
  //Se non c'è nessun Push attivo verifica se deve attivarne uno
  //La sequanza di push è Frequency; Aux; Sweep Time ed è condizionata 
  //dalla modalità di funzionamento corrente
  if (FrequencyRotary.push() == 1)
  {
    bitSet(FiuMode,0);
    delay(50);
  }
  else
  {
    if (FiuMode & B10000000)
    {
      if (AuxRotary.push() == 1)
      {
        //Disattiva lo stato del bit 1
        bitSet(FiuMode,1);
        //Fa un ritardo arbitrario per evitare eventuali spurie
        delay(50);
      }
      else
      {
        if (SweepTimeRotary.push() == 1)
        {
          //Disattiva lo stato del bit 1
          bitSet(FiuMode,2);
          //Fa un ritardo arbitrario per evitare eventuali spurie
          delay(50);
        }
      }
 
    }
  }
}


//#=================================================================================
//#RotaryTurn()
//#Legge gli encoder e carica su RotaryState lo stato relativo alla
//#rotazione degli assi
//#=================================================================================
void RotaryTurn()
{
  //Legge l'encoder principale (Frequenza Base)
  RotaryState = FrequencyRotary.rotate();
  //Legge l'encoder ausiliario (Seconda Frequenza per lo Sweep)
  RotaryState = RotaryState + (4*AuxRotary.rotate());
  //Legge l'encoder Sweep Time (Tempo di Sweep)
  RotaryState = RotaryState + (16*SweepTimeRotary.rotate());
}

//#=================================================================================
//#RotaryTurnAction(float Value, float Delta, float LLimit, float HLimit)
//#In base a contenuto della RotaryState modifica le variabili di riferimento
//#per farlo usa il passo impostato come parametro
//#gestisce il superamento dei limiti inferiore ed superiore 
//#=================================================================================
float RotaryTurnAction(float Value, float Delta, float LLimit, float HLimit)
{
  if (RotaryState == 1)
  {
    Value += Delta ;
  }
  else
  {
    Value -= Delta ;
  }
  if (Value < LLimit)
  {
    Value = HLimit;
  } 
  if (Value > HLimit)
  {
    Value = LLimit;
  }
  return (Value);
}

