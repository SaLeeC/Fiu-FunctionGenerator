#include <Arduino.h>

#include <ESP32DMASPISlave.h>

void sweepgenCreate(float FL, float FH, float TSweep, uint8_t SweepMode);
void DACIncrementStep(uint16_t ii);
void DACDecrementStep(uint16_t ii);
    
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

void sweepgenCreate(float FL, float FH, float TSweep, uint8_t SweepMode)
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

  //Apre il loop di popolamento dellarray per il DAC
  //Questa srie va popolata sempre tutta. La sequenza deve seguire il senso di crescita della frequenza
  //così da assegnare sullasse delle X una posizione fissa ad ogni frequenza per cui, se la sequenza di sweep
  //prevede un andamento misto della frequenza (triangolare o sinusoide), la tensione generata dal DAC
  //seguirà lo stesso andamento.
  //Il fattore di cresita di questo valore è legato SOLO al tempo trascorso e non all valore della frequenza
  //esattamente come sarebbe il movimento dell'asse X di un oscilloscopio, ne consegue che nell'andamento
  //logaritmico le curve derivanti saranno rappresentate come in un diagramma logaritmico e non linearizzate

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
    
}

void setup()
{
  // put your setup code here, to run once:
    Serial.begin(115200);

    Serial.println(micros());
    sweepgenCreate(10, 1000,2000, 0);
    Serial.println(micros());

    Serial.println(micros());
    sweepgenCreate(10, 1000,2000, 1);
    Serial.println(micros());

    Serial.println(micros());
    sweepgenCreate(10, 1000,2000, 2);
    Serial.println(micros());

    Serial.println(micros());
    sweepgenCreate(10, 1000,2000, 4);
    Serial.println(micros());

    Serial.println(micros());
    sweepgenCreate(10, 1000,2000, 16);
    Serial.println(micros());

/*     // to use DMA buffer, use these methods to allocate buffer
    spi_slave_tx_buf = rxSPI.allocDMABuffer(BUFFER_SIZE);
    spi_slave_rx_buf = rxSPI.allocDMABuffer(BUFFER_SIZE);

    set_buffer();

    delay(5000);

    rxSPI.setDataMode(SPI_MODE1); // for DMA, only 1 or 3 is available
    rxSPI.setMaxTransferSize(BUFFER_SIZE);
    rxSPI.setDMAChannel(2); // 1 or 2 only
    rxSPI.setQueueSize(1); // transaction queue size
    // begin() after setting
    // HSPI = CS: 15, CLK: 14, MOSI: 13, MISO: 12
    rxSPI.begin(HSPI);

    printf("Main code running on core : %d\n", xPortGetCoreID());

    xTaskCreatePinnedToCore(task_wait_spi, "task_wait_spi", 2048, NULL, 2, &task_handle_wait_spi, CORE_TASK_SPI_SLAVE);
    xTaskNotifyGive(task_handle_wait_spi);

    xTaskCreatePinnedToCore(task_process_buffer, "task_process_buffer", 2048, NULL, 2, &task_handle_process_buffer, CORE_TASK_PROCESS_BUFFER);
 */}

void loop() {
  //  Serial.println("ESP32");
  // put your main code here, to run repeatedly:
}