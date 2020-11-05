#include <SimpleRotary.h>

//#=================================================================================
// Encoder Rotary area
//Settaggio Frequenza e Step di avanzamento frequenza
//#=================================================================================
#define FERotaryA 2
#define FERotaryB 3
#define FERotaryButton 4
SimpleRotary FrequencyRotary(FERotaryA,FERotaryB,FERotaryButton);
#define PushOff 0
#define PushOn 1

float RotaryTurnAction(float Value, float Delta, float LLimit, float HLimit);

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


void RotaruSetup()
{
  //Setta le opzioni per la gestione degli encoder
  pinMode(FERotaryA, INPUT_PULLUP);
  pinMode(FERotaryB, INPUT_PULLUP);
  FrequencyRotary.setDebounceDelay(5);
  FrequencyRotary.setErrorDelay(100);
}
uint8_t RotaryState = 0;

//uint8_t FrequencyRotaryState;
uint8_t FrequencyStepEsponent[3] = {2,2,2};//potenza del 10 che si somma o sottrae alla frequenza
float FrequencyStepValue[3] = {100,100,100};//Valore effettivo dello step da attuare espresso in Herz
char *FrequencyStepLabel[] = {"F L Step",
                              "F H Step",
                              "Sweep T"};

uint8_t ModeRotaryState;

#define NumOfMode 2
char *ModeLabel[] = {"Forma d'Onda", "Sweep"};


//#=================================================================================
//#RotaryTurn()
//#Legge gli encoder e carica su RotaryState lo stato relativo alla
//#rotazione degli assi
//#=================================================================================
void RotaryTurn()
{
  //Legge l'encoder principale (Frequenza Base)
  RotaryState = FrequencyRotary.rotate();
}


void RotaryManagement()
{
  //Aggiorna lo stato degli encoder
  RotaryTurn();
  //Se Uno degli encoder è stato ruotato
  if(RotaryState)
  //Gestisce solo il primo encoder che risulta mosso nell'ordine
  //Frequency
  //Aux
  //Sweep Time
  //La gestione è condizionata dalla modalità di funzionamento corrente
  {
    //Controlla se è stato girato l'encoder della frequenza L
    if(RotaryState & B00000011)
    {
      //Se è stato girato elimina le informazioni provenienti dagli altri encoder 
      RotaryState = RotaryState & B00000011;
      //E attua la modifica richiesta con la rotazione
      FreqTX = RotaryTurnAction(FreqTX,
                                FrequencyStepValue[0],
                                FrequencyLimit[0][0],
                                FrequencyLimit[0][1]);
      // //Aggiorna la visualizzazione sul display 7 segmenti
      // FrequencyDisplay.printDigit(Frequency[FreqL], 0);
      // //Aggiorna il limite inferiore della Frequency H ponendolo al valore attuale della Frequency L
      // FrequencyLimit[FrequencyWaveCurrentType][LimitL][FreqH] = Frequency[FreqL];
      // //Aggiorna la presentazione dei limiti sul TFT
      // TftFrequencyLimit();
    }
  }  
  RotaryState = 0;  
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
