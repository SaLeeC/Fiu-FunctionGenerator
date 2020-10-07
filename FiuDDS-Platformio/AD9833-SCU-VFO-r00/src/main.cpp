#include <Arduino.h>

#include <ESP32DMASPISlave.h>

void sweepgenCreate(float FL, float FH, float TSweep, uint8_t SweepMode);
//#define freqSerieMaxLeng 2048
#define freqSerieMaxLeng 64
uint32_t freqSerie[freqSerieMaxLeng];//Sequenza di frequenze per lo sweep
uint16_t freqSerieNumActiveElement = 0;//Numero degli elementi caricati per lo sweep
uint32_t freqSerieTStep = 0;//Tempo in uS fra uno step e il successivo

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
Genera la sequenza di frequenze per far sweepare il DDS
Le sequenze di frequenze limite sono FL e FH le quali sono espresse in Hz.
Il tempo di ciclo è definito da TSweep il quale è espresso in uS.
La sequenza è salvata nell'array freqSerie[] che può avere sino a 2048 elementi
Il numero di elementi utilizzato per descrivere la serie è in freqSerieNumActiveElement
Il tempo fra uno step e il successivo è in freqSerieTStep ed è espresso in uS
SweepMode definisce la tipologia di distribuzione delle frequenze durante lo sweep
0 = Rampa Ascendente
1 = Rampa discendente
2 = trinagolare (simmetrica)
4 = sinusoidale
16 = andamento logaritmico
*/
{
  uint32_t deltaF;
  //Inizializza il numero di elementi caricati nell'array al massimo valore caricabile
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
  //Inizializza la prima casella della serie
  //e calcola lo step di avanzamento per la frequenza nei casi in
  //cui il processo sia lineare
  deltaF /= freqSerieNumActiveElement;
  //Se la ssequenza e inversa fa partire la serie dal valire massimo
  if (SweepMode & B00000001)
  {
    freqSerie[0] = FH;
  }
  //altrimenti parte dal valire minimo
  else
  {
      freqSerie[0] = FL;
  }
  //Calcola il passo angolare in frazioni di 2Pigreco per la sinusoide
  float passoAngolare = (2.0 * PI) / freqSerieNumActiveElement;
  //Controlla se è richiesta la sinusoide nel qual caso setta deltaF a delta/2 per snellire il calcolo
  if((SweepMode & B00000100) != 0)
  {
    deltaF = (FH - FL) / 2.0;
  }
  //Inizializza i riferimenti per la curva logaritmica
  float stepLog = (log10(FH) - log10(FL)) / freqSerieNumActiveElement;//calcola il passo per coprire l'intero sweep
  //Apre il loop di popolamento dell'Array
  for(int32_t ii = 1; ii < freqSerieNumActiveElement; ii++)//per tutte le celle dell'Array
  {
    //Sceglie il processo di popolamento dell'Array
    switch (SweepMode)
    {
    case 0://Rampa ascendente-popola per somma con andamento lineare o logaritmico
      //Se la progressione è lineare
      if((SweepMode & B00010000) == 0)
      {
        freqSerie[ii] = freqSerie[ii - 1] + deltaF;
      }
      //Se la progressione è logaritmica
      else
      {
        freqSerie[ii] = pow(10,(log10(freqSerie[ii-1]) + stepLog));
      }
      break;
    case 1://Rampa discendente-popola per differenza
      freqSerie[ii] = freqSerie[ii - 1] - deltaF;
      break;
    case 2://Triangolare (simmetrica)-popola con un doppio passo
      deltaF += deltaF;//Raddopia il passo
      //Per la prima metà dei passi va in salita
      if (ii >= (freqSerieNumActiveElement/2))
      {
        freqSerie[ii] = freqSerie[ii-1] + deltaF;
      }
      //Per la seconda metà dei passi va in discesa
      else
      {
          freqSerie[ii] = freqSerie[ii-1] - deltaF;
      }
      break;
    case 4://Sinusoidale
      //In questa modalità deltaF vale delta/2
      //Lo split di (freqSerieNumActiveElement/4) serve a ritardare di 90° la simulazione
      //della sinusoide così da partire dal valore minimo (FL)
      //freqSerie[ii] = (float(ii) - (freqSerieNumActiveElement/4.0)) * passoAngolare);
      freqSerie[ii] = (FL + deltaF) + (deltaF * sin((float(ii) - (freqSerieNumActiveElement/4.0)) * passoAngolare));
      Serial.print(float(ii));
      Serial.print(" - ");
      Serial.print((freqSerieNumActiveElement/4.0));
      Serial.print(" * ");
      Serial.print(passoAngolare,8);
      Serial.print(" | ");
      Serial.println(sin((float(ii) - (freqSerieNumActiveElement/4.0)) * passoAngolare),8);
      break;
    default:
      break;
    }
  }
  
}

void setup()
{
  // put your setup code here, to run once:
    Serial.begin(115200);

    sweepgenCreate(1000, 10000,2000, 4);
    for(int ii = 0; ii < freqSerieNumActiveElement; ii++)
    {
      Serial.print(ii);
      Serial.print(" - ");
      Serial.println(freqSerie[ii]);
      delay(500);
    }

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