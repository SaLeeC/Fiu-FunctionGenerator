#include <Arduino.h>

#include <ESP32DMASPISlave.h>

#include <SPI.h>
//#include <MD_cmdProcessor.h>
#include <MD_AD9833.h>

//#include <si5351.h>
#include <Adafruit_SI5351.h>
#include "Wire.h"

//Si5351 HSClockGen;
Adafruit_SI5351 HSClockGen = Adafruit_SI5351();
#define PLLB_FREQ    87600000000
uint32_t HSCFrequency;

/*
Il processo di controllo del DDS gira autonomamente nel core 1 mentre tutti 
i rimanenti processi girano nel core 0.
Questo consente di ottenere la massima velocità nella gestione del DDS riducendo 
le interferenze fra i processi di servizio e il processo di controllo del DDS 
*/

TaskHandle_t DDSManagement;

void sweepgenCreate(float TSweep, uint8_t SweepMode);
void DACIncrementStep(uint16_t ii);
void DACDecrementStep(uint16_t ii);
void DDSManagementCode(void * pvParameters);

//Blocco variabili di controllo dei processi
//DDSGenMode
//128 = Frequenza fissa
//  0 = Rampa Ascendente
//  1 = Rampa discendente
//  2 = trinagolare (simmetrica)
//  4 = sinusoidale
// 16 = andamento logaritmico
uint8_t DDSGenMode = 128;
float FL, FH;
//DDSWave
//0 = Sinusoide
//1 = Triangolare
//2 = Quadra
uint8_t DDSWave = 0;

//#define freqSerieMaxLeng 2048
#define freqSerieMaxLeng 2048
//uint32_t freqSerie[freqSerieMaxLeng];//Sequenza di frequenze per lo sweep
float freqSerie[freqSerieMaxLeng];//Sequenza di frequenze per lo sweep
uint16_t freqSerieNumActiveElement = 0;//Numero degli elementi caricati per lo sweep
uint32_t freqSerieTStep = 0;//Tempo in uS fra uno step e il successivo
#define DACResolution 256
float DACSerieStep;
float DACCurrentProg;
//il numero di elementi dellArray del DAC è uguale al numero di elementi dell'Array
//delle frequenze.
//Il numero di celle utilizzate rimarà uguale a quello delle frequenze
uint16_t DACSerie[freqSerieMaxLeng];

// Pins for SPI comm with the AD9833 IC
#define FSYNC 10	///< SPI Load pin number (FSYNC in AD9833 usage)
MD_AD9833	DDS9833_0(FSYNC); // DDS AD9833 via Hardware SPI


ESP32DMASPI::Slave rxSPI;

static const uint32_t BUFFER_SIZE = 70;
uint8_t* spi_slave_tx_buf;
uint8_t* spi_slave_rx_buf;

void set_buffer()
{
    for (uint32_t i = 0; i < BUFFER_SIZE; i++)
    {
        // spi_master_tx_buf[i] = i & 0xFF;
        spi_slave_tx_buf[i] = (0xFF - i) & 0xFF;
    }
    // memset(spi_master_rx_buf, 0, BUFFER_SIZE);
    memset(spi_slave_rx_buf, 0, BUFFER_SIZE);
}


constexpr uint8_t CORE_TASK_SPI_SLAVE {0};
constexpr uint8_t CORE_TASK_PROCESS_BUFFER {0};

static TaskHandle_t task_handle_wait_spi = 0;
static TaskHandle_t task_handle_process_buffer = 0;

void task_wait_spi(void *pvParameters)
{
    while(1)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        rxSPI.wait(spi_slave_rx_buf, spi_slave_tx_buf, BUFFER_SIZE);

        String taskMessage = "SPI wait running on core ";
        taskMessage = taskMessage + xPortGetCoreID();
        Serial.println(taskMessage);

        xTaskNotifyGive(task_handle_process_buffer);
    }
}

void task_process_buffer(void *pvParameters)
{
    while(1)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        String taskMessage = "Buffer print running on core ";
        taskMessage = taskMessage + xPortGetCoreID();
        Serial.println(taskMessage);

        // show received data
        for (size_t i = 0; i < BUFFER_SIZE; ++i)
        {
            printf("%c", spi_slave_rx_buf[i]);
            //spi_slave_tx_buf[i] = ++spi_slave_rx_buf[i];
        }
        printf("\n");

        rxSPI.pop();

        xTaskNotifyGive(task_handle_wait_spi);
    }
}

void sweepgenCreate(float TSweep, uint8_t SweepMode)
/*
----------------------------------------------------------------------------------------
Genera la sequenza di frequenze per far sweepare il DDS
Le sequenze di frequenze limite sono FL e FH le quali sono espresse in Hz.
Il tempo di ciclo è definito da TSweep il quale è espresso in uS.
La sequenza è salvata nell'array freqSerie[] che può avere sino a 2048 elementi
Il numero di elementi utilizzato per descrivere la serie è in freqSerieNumActiveElement
Il tempo fra uno step e il successivo è in freqSerieTStep ed è espresso in uS
----------------------------------------------------------------------------------------
SweepMode definisce la tipologia di distribuzione delle frequenze durante lo sweep
0 = Rampa Ascendente
1 = Rampa discendente
2 = trinagolare (simmetrica)
4 = sinusoidale
16 = andamento logaritmico
----------------------------------------------------------------------------------------
Viene anche generata la sequenza di valori per la generazione di una tensione con la 
stessa forma d-onda dello sweep. Questa tensione pu; essere utilizzata come asse X di un
oscilloscopio per visualizzazione sincronizzata rispetto allo sweep dei segnali.
La tensione viene generata con un DAC da 256 passi. I passi sono normalizzati in maniera
da coprire sempre tutta l-escursione dello sweep.
*/
{
  uint32_t deltaF;
  //Inizializza il numero di elementi utilizzati dell'array delle frequenze pari alla dimensione
  //dellArray.
  //Lo stesso valore è utilizzato per gestire lArray del DAC principale
  freqSerieNumActiveElement = freqSerieMaxLeng; 
  //Calcola il deltaF
  deltaF = FH - FL;
  //Se deltaF espresso in Hz è inferiore a freqSerieMaxLeng i valori della serie
  //dello sweep sono inferiori alla dimensione dell'Array.
  //Fissa il numero di celle dell'Array utilizzate
  if (deltaF < freqSerieMaxLeng)
  {
    freqSerieNumActiveElement = deltaF;
  }
  //calcola il rapporto fra i passi F e i passi V (passi del DAC)
  DACSerieStep = freqSerieNumActiveElement/DACResolution;
  //Inizializza la prima casella della serie
  //e calcola lo step di avanzamento per la frequenza nei casi in
  //cui il processo sia lineare
  deltaF /= freqSerieNumActiveElement;
  //Se la ssequenza e inversa fa partire la serie dal valire massimo
  if (SweepMode & B00000001)
  {
    freqSerie[0] = FH;//Inizia dal valore massimo
    DACSerie[0] = DACResolution;//Inizia dal valore massimo
  }
  //altrimenti parte dal valire minimo
  else
  {
    freqSerie[0] = FL;//Inizia dal valore minimo
    DACSerie[0] = 0;//Inizia dal valore minimo
  }
  //Calcola il passo angolare in frazioni di 2Pigreco per la sinusoide
  float passoAngolare = (2.0 * PI) / freqSerieNumActiveElement;
  //Se è richiesta la triangolare raddoppia il passo
  if((SweepMode & B00000010) != 0)
  {
      deltaF += deltaF;//Raddopia il passo
      DACSerieStep += DACSerieStep;//Raddopia il passo
  }
  //Controlla se è richiesta la sinusoide nel qual caso setta deltaF a delta/2 per
  //snellire il calcolo
  if((SweepMode & B00000100) != 0)
  {
    deltaF = (FH - FL) / 2.0;
    DACSerieStep = freqSerieNumActiveElement / 2;//?????????
  }
  //Inizializza i riferimenti per la curva logaritmica
  float stepLog = (log10(FH) - log10(FL)) / freqSerieNumActiveElement;//calcola il passo per coprire l'intero sweep
  //Inizializza laccumulatore per il controllo del passo del DAC
  DACCurrentProg = DACSerie[0];
  //Apre il loop di popolamento degli Array (frequenza e DAC)
  for(int32_t ii = 1; ii < freqSerieNumActiveElement; ii++)//per tutte le celle dell'Array
  {
    //Sceglie il processo di popolamento dell'Array
    switch (SweepMode)
    {
    case 0://Rampa ascendente-popola per somma con andamento lineare
      freqSerie[ii] = freqSerie[ii - 1] + deltaF;
      DACIncrementStep(ii);
      break;
    case 1://Rampa discendente-popola per differenza
      freqSerie[ii] = freqSerie[ii - 1] - deltaF;
      DACDecrementStep(ii);
      break;
    case 2://Triangolare (simmetrica)-popola con un doppio passo
      //Per la prima metà dei passi va in salita
      if (ii <= (freqSerieNumActiveElement/2))
      {
        freqSerie[ii] = freqSerie[ii-1] + deltaF;
        DACIncrementStep(ii);
      }
      //Per la seconda metà dei passi va in discesa
      else
      {
        freqSerie[ii] = freqSerie[ii-1] - deltaF;
        DACDecrementStep(ii);
      }
      break;
    case 4://Sinusoidale
      //In questa modalità deltaF vale delta/2
      //Lo split di (freqSerieNumActiveElement/4) serve a ritardare di 90° la simulazione
      //della sinusoide così da partire dal valore minimo (FL)
      freqSerie[ii] = (FL + deltaF) + (deltaF * sin((float(ii) - (freqSerieNumActiveElement/4.0)) * passoAngolare));
      DACIncrementStep(ii);
      break;
    case 16://Rampa ascendente-popola per somma con andamento logaritmico
      freqSerie[ii] = pow(10,(log10(freqSerie[ii-1]) + stepLog));
      DACIncrementStep(ii);
      break;
    default:
      break;
    }
  }
}

void DACIncrementStep(uint16_t ii)
{
  DACCurrentProg += DACSerieStep;//incrementa del passo di avanzamento del DAC
  //Se laccumulatore di passi ha superato lunità (il DAC lavora per numeri interi)
  if(int(DACCurrentProg) >= 1)
  {
    //Incrementa il contenuto della cella corrente dellArray del DAC della parte intera
    //dell'accumulatore di passo
    DACSerie[ii] = DACSerie[ii-1] + int(DACCurrentProg);
    //Elimina la parte intera dallaccumulatore di passo lasciando la parte decimale
    DACCurrentProg -= int(DACCurrentProg);
  }
  //Se l'accumulatore di passo non ha ancora superato l'unità
  else
  {
    //Conferma il valore precedente nella cella corrente dell'Array del DAC
    DACSerie[ii] = DACSerie[ii-1];
  } 
}

void DACDecrementStep(uint16_t ii)
{
  DACCurrentProg += DACSerieStep;//incrementa del passo di avanzamento del DAC
  //Se laccumulatore di passi ha superato lunità (il DAC lavora per numeri interi)
  if(int(DACCurrentProg) >= 1)
  {
    //Incrementa il contenuto della cella corrente dellArray del DAC della parte intera
    //dell'accumulatore di passo
    DACSerie[ii] = DACSerie[ii-1] - int(DACCurrentProg);
    //Elimina la parte intera dallaccumulatore di passo lasciando la parte decimale
    DACCurrentProg -= int(DACCurrentProg);
  }
  //Se l'accumulatore di passo non ha ancora superato l'unità
  else
  {
    //Conferma il valore precedente nella cella corrente dell'Array del DAC
    DACSerie[ii] = DACSerie[ii-1];
  } 
}

void DDSManagementCode(void * pvParameters)
{
  uint32_t iii = 0;//Puntatore per lo Sweep
  //Genera uno sweep sino a quando non viene richiesta una frequenza fissa
  while ((DDSGenMode & B01000000) == 0)
  {
    //Imposta la frequenza corrente
    DDS9833_0.setFrequency(MD_AD9833::CHAN_0, freqSerie[iii]);
    //Imposta la tensione corrente per il DAC
    DACSerie[iii];
    //Aspetta il tempo di step
    delayMicroseconds(freqSerieTStep);
    //Aggiorna il puntatore
    iii++;
    //controlla che non abbia superato il limite
    iii %= freqSerieNumActiveElement;
  }
  //Se è stato richiesto un cambio frequenza provvede, diversamente salta
  if((DDSGenMode & B10000000) !=0)
  {
    //Imposta la frequenza fissa
    DDS9833_0.setFrequency(MD_AD9833::CHAN_0, FL);
    //Azzera il flag di "cambia frequenza fissa"
    bitClear(DDSGenMode,7);
  }
}

//Attua la sequenza sweep 
void sweepgen()
{
 
}

void setup()
{
 // Start serial and initialize the Si5351
  Serial.begin(115200);
  Serial.print("Init result ");
  Serial.println("Si5351 Clockgen Test"); Serial.println("");
  
  /* Initialise the sensor */
  if (HSClockGen.begin() != ERROR_NONE)
  {
    /* There was a problem detecting the IC ... check your connections */
    Serial.print("Ooops, no Si5351 detected ... Check your wiring or I2C ADDR!");
    while(1);
  }
  Serial.println("OK!");

  /* INTEGER ONLY MODE --> most accurate output */  
  /* Setup PLLA to integer only mode @ 900MHz (must be 600..900MHz) */
  /* Set Multisynth 0 to 112.5MHz using integer only mode (div by 4/6/8) */
  /* 25MHz * 36 = 900 MHz, then 900 MHz / 8 = 112.5 MHz */
  Serial.println("Set PLLA to 900MHz");
  HSClockGen.setupPLLInt(SI5351_PLL_A, 36);
  Serial.println("Set Output #0 to 112.5MHz");  
  HSClockGen.setupMultisynthInt(0, SI5351_PLL_A, SI5351_MULTISYNTH_DIV_8);

  /* FRACTIONAL MODE --> More flexible but introduce clock jitter */
  /* Setup PLLB to fractional mode @616.66667MHz (XTAL * 24 + 2/3) */
  /* Setup Multisynth 1 to 13.55311MHz (PLLB/45.5) */
  HSClockGen.setupPLL(SI5351_PLL_B, 24, 2, 3);
  Serial.println("Set Output #1 to 13.553115MHz");  
  HSClockGen.setupMultisynth(1, SI5351_PLL_B, 45, 1, 2);

  /* Multisynth 2 is not yet used and won't be enabled, but can be */
  /* Use PLLB @ 616.66667MHz, then divide by 900 -> 685.185 KHz */
  /* then divide by 64 for 10.706 KHz */
  /* configured using either PLL in either integer or fractional mode */

  Serial.println("Set Output #2 to 10.706 KHz");  
  HSClockGen.setupMultisynth(2, SI5351_PLL_B, 900, 0, 1);
  HSClockGen.setupRdiv(2, SI5351_R_DIV_64);
    
  /* Enable the clocks */
  HSClockGen.enableOutputs(true);
  Serial.println("\nI2C Scanner");
  byte error, address;
  int nDevices;
  Serial.println("Scanning...");
  nDevices = 0;
  for(address = 1; address < 127; address++ ) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      if (address<16) {
        Serial.print("0");
      }
      Serial.println(address,HEX);
      nDevices++;
    }
    else if (error==4) {
      Serial.print("Unknow error at address 0x");
      if (address<16) {
        Serial.print("0");
      }
      Serial.println(address,HEX);
    }    
  }
  if (nDevices == 0) {
    Serial.println("No I2C devices found\n");
  }
  else {
    Serial.println("done\n");
  }
  delay(5000);          


  // //create a task that will be executed in the Task1code() function, with priority 1 and executed on core 0
  // xTaskCreatePinnedToCore(
  //                   DDSManagementCode,  /* Task function. */
  //                   "DDSManagement",    /* name of task. */
  //                   10000,              /* Stack size of task */
  //                   NULL,               /* parameter of the task */
  //                   1,                  /* priority of the task */
  //                   &DDSManagement,     /* Task handle to keep track of created task */
  //                   0);                 /* pin task to core 0 */                  

  // // put your setup code here, to run once:
  //   Serial.begin(115200);

  //   Serial.println(micros());
  //   sweepgenCreate(2000, 0);
  //   Serial.println(micros());

  //   Serial.println(micros());
  //   sweepgenCreate(2000, 1);
  //   Serial.println(micros());

  //   Serial.println(micros());
  //   sweepgenCreate(2000, 2);
  //   Serial.println(micros());

  //   Serial.println(micros());
  //   sweepgenCreate(2000, 4);
  //   Serial.println(micros());

  //   Serial.println(micros());
  //   sweepgenCreate(2000, 16);
  //   Serial.println(micros());

  //    // to use DMA buffer, use these methods to allocate buffer
  //   spi_slave_tx_buf = rxSPI.allocDMABuffer(BUFFER_SIZE);
  //   spi_slave_rx_buf = rxSPI.allocDMABuffer(BUFFER_SIZE);

  //   set_buffer();

  //   delay(5000);

  //   rxSPI.setDataMode(SPI_MODE1); // for DMA, only 1 or 3 is available
  //   rxSPI.setMaxTransferSize(BUFFER_SIZE);
  //   rxSPI.setDMAChannel(2); // 1 or 2 only
  //   rxSPI.setQueueSize(1); // transaction queue size
  //   // begin() after setting
  //   // HSPI = CS: 15, CLK: 14, MOSI: 13, MISO: 12
  //   rxSPI.begin(HSPI);

  //   printf("Main code running on core : %d\n", xPortGetCoreID());

  //   xTaskCreatePinnedToCore(task_wait_spi, "task_wait_spi", 2048, NULL, 2, &task_handle_wait_spi, CORE_TASK_SPI_SLAVE);
  //   xTaskNotifyGive(task_handle_wait_spi);

  //   xTaskCreatePinnedToCore(task_process_buffer, "task_process_buffer", 2048, NULL, 2, &task_handle_process_buffer, CORE_TASK_PROCESS_BUFFER);
}


#define SI5351_FREQ_MULT                100ULL
#define SI5351_PLL_FIXED                80000000000ULL

void loop() 
{

  // Read the Status Register and print it every 10 seconds
//  Serial.println(HSCFrequency);
//   HSClockGen.set_freq((HSCFrequency * SI5351_FREQ_MULT), SI5351_PLL_FIXED, SI5351_CLK1);
//   HSClockGen.set_freq(HSCFrequency, SI5351_CLK0);
//   HSClockGen.update_status();
//   Serial.print("corr: ");
// //  Serial.print(HSClockGen.get_correction());
//   Serial.print("  SYS_INIT: ");
//   Serial.print(HSClockGen.dev_status.SYS_INIT);
//   Serial.print("  LOL_A: ");
//   Serial.print(HSClockGen.dev_status.LOL_A);
//   Serial.print("  LOL_B: ");
//   Serial.print(HSClockGen.dev_status.LOL_B);
//   Serial.print("  LOS: ");
//   Serial.print(HSClockGen.dev_status.LOS);
//   Serial.print("  REVID: ");
//   Serial.println(HSClockGen.dev_status.REVID);

//   delay(10000);
//   if (HSCFrequency>1000000)
//   {
//     HSCFrequency=80000;
//   }
//   else
//   {
//     HSCFrequency=144000000;
//   }
  
//  HSCFrequency += 1000;
  //  Serial.println("ESP32");
  // put your main code here, to run repeatedly:
}