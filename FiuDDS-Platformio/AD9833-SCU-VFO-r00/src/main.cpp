#include <Arduino.h>

#include <ESP32DMASPISlave.h>

void sweepgenCreate(float FL, float FH, float TSweep);
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

void sweepgenCreate(uint32_t FL, uint32_t FH, uint32_t TSweep)
/*
Genera la sequenza di frequenze per far sweepare il DDS
Le sequenze di frequenze limite sono FL e FH le quali sono espresse in Hz.
Il tempo di ciclo è definito da TSweep il quale è espresso in uS.
La sequenza è salvata nell'array freqSerie[] che può avere sino a 2048 elementi
Il numero di elementi utilizzato per descrivere la serie è in freqSerieEndSerie
Il tempo fra uno step e il successivo è in freqSerieTStep ed è espresso in uS
*/
{
  //Calcola il deltaF
  uint32_t deltaF = FH - FL;
  
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