#include <Arduino.h>

#include <ESP32DMASPISlave.h>

void sweepgenCreate(float FL, float FH, float TSweep, uint8_t SweepMode);
#define freqSerieMaxLeng 2048
uint32_t freqSerie[freqSerieMaxLeng];//Sequenza di frequenze per lo sweep
uint16_t freqSerieEndSerie = 0;//Numero degli elementi caricati per lo sweep
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

void sweepgenCreate(uint32_t FL, uint32_t FH, uint32_t TSweep, uint8_t SweepMode)
/*
Genera la sequenza di frequenze per far sweepare il DDS
Le sequenze di frequenze limite sono FL e FH le quali sono espresse in Hz.
Il tempo di ciclo è definito da TSweep il quale è espresso in uS.
La sequenza è salvata nell'array freqSerie[] che può avere sino a 2048 elementi
Il numero di elementi utilizzato per descrivere la serie è in freqSerieEndSerie
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
  freqSerieEndSerie = freqSerieMaxLeng; 
  //Calcola il deltaF
  deltaF = FH - FL;
  //Se deltaF espresso in Hz è inferiore a freqSerieMaxLeng i valori della serie
  //dello sweep sono inferiori alla dimensione dell'Array.
  //Fissa il numero di celle dell'Array utilizzate
  if (deltaF < freqSerieMaxLeng)
  {
    freqSerieEndSerie = deltaF;
  }
  //Inizializza la prima casella della serie
  //e calcola lo step di avanzamento per la frequenza nei casi in
  //cui il processo sia lineare
  switch (SweepMode)
  {
  case 0://Rampa ascendente
      freqSerie[0] = FL;
      //Step di avanzamento calcolato cme (FH-FL)/numero passi
      deltaF /= freqSerieEndSerie;
      break;
  case 1://
      freqSerie[0] = FH;
      //Step di avanzamento calcolato cme (FH-FL)/numero passi
      deltaF /= freqSerieEndSerie;
      break;
  case 2://
      freqSerie[0] = FL + (deltaF/2);
      deltaF /= freqSerieEndSerie;
      break;
  case 4://
      freqSerie[0] = FL + (deltaF/2);
      deltaF = 0;//Deve essere stabilito step per step dalla funzione di caricamento
      break;
  default:
      break;
  }
  //Apre il loop di popolamento dell'Array
  for(uint16_t ii = 1; ii < freqSerieEndSerie; ii++)//per tutte le celle dell'Array
  {
    //Sceglie il processo di popolamento dell'Array
    switch (SweepMode)
    {
    case 0://Rampa ascendente
      freqSerie[ii] = freqSerie[ii - 1] + deltaF;
      break;
    case 1://Rampa discendente
      freqSerie[ii] = freqSerie[ii - 1] - deltaF;
      break;
    case 2://Triangolare (simmetrica)
      
      freqSerie[ii] = freqSerie[ii] - deltaF;
      break;
    case 4://Sinusoidale
      /* code */
      break;
    default:
      break;
    }
  }
  //Calcola il deltaF/T
  float deltaFT = deltaF / TSweep;
  
}

void setup()
{
  // put your setup code here, to run once:
    Serial.begin(115200);

    // to use DMA buffer, use these methods to allocate buffer
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
}

void loop() {
    Serial.println("ESP32");
  // put your main code here, to run repeatedly:
}