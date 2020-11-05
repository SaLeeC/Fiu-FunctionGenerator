#include <I2C_eeprom.h>

void EEPROMWriteIntValue(uint16_t eepromAddress, uint32_t eepromValue, uint8_t eepromVLength);
uint32_t EEPROMReadIntValue(uint16_t eepromAddress, uint8_t eepromVLength);

// the address of your EEPROM
#define DEVICEADDRESS (0x57)


#define FZeroTx 0x00
#define FZeroRx 0x04
#define IntermediateFreq 0x08
#define Ch0Tx   0x100
#define ChTotNumber 128
#define FreqByteLeng 4

uint32_t IFreq,
         FreqTX,
         FreqRX;

uint32_t FrequencyLimit[3][2] = {{27000000,29000000},
                        {26000000,29000000},
                          {400000,  500000}
                       };

//Puntatore della EEPROM da 32K
uint16_t eeprom32Pointer[270] = {
                                 FZeroTx,
                                 FZeroRx,
                                 IntermediateFreq
                                };


I2C_eeprom eeprom32(DEVICEADDRESS, 0x1000);

void EEPROMsetup()
{
  //Inizializza gli indirizzi dei canali in memoria
  for (uint16_t ii = 0; ii < (2 * ChTotNumber); ii += 2)
  {
    eeprom32Pointer[ii - Ch0Tx] = Ch0Tx;
    eeprom32Pointer[ii - Ch0Tx] = Ch0Tx + 4;
    //Recuper la IF
  }

  eeprom32.begin();
  int size = eeprom32.determineSize();
  //Presenta il Memory test
  display.clearDisplay();
  display.display();
  display.setTextSize(2);
  display.setCursor(0,2);
  display.setTextColor(SSD1306_WHITE);        // Draw white text
  display.print("Mem: ");
  display.display();
  if (size > 0)
  {
    display.print(size);
    display.println(" KB");
  } 
  else
  {
    display.println("FAULT");
    while(1);
  }

  display.display();
  //Carica il valore della frequenza intermedia
  IFreq = EEPROMReadIntValue(IntermediateFreq, 4);
  //Se è la prima volta inizializza la IF a 455kHz 
  if (IFreq == 0)
  {
    EEPROMWriteIntValue(IntermediateFreq, 455000, 4);
    IFreq = EEPROMReadIntValue(IntermediateFreq, 4);
  }
  //Carica l'ultima frequenza TX corrente
  FreqTX = EEPROMReadIntValue(FZeroTx, 4);
  //Se è la prima volta inizializza la TX corrente a 27MHz 
  if (FreqTX == 0)
  {
    EEPROMWriteIntValue(FZeroTx, 27000000, 4);
    FreqTX = EEPROMReadIntValue(FZeroTx, 4);
  }
  //Carica l'ultima frequenza RX corrente
  FreqRX = EEPROMReadIntValue(FZeroRx, 4);
  //Se è la prima volta inizializza la RX corrente a 27MHz 
  if (FreqRX == 0)
  {
    EEPROMWriteIntValue(FZeroRx, 27000000, 4);
    FreqRX = EEPROMReadIntValue(FZeroRx, 4);
  }

  delay(3000);
  display.clearDisplay();
  display.display();
}

void EEPROMWriteIntValue(uint16_t eepromAddress, uint32_t eepromValue, uint8_t eepromVLength)
{
  uint8_t appo = 0;
  for (uint8_t ii = 0; ii < eepromVLength; ii++)
  {
    appo = eepromValue % 256;
    eepromValue = eepromValue >> 8;
    eeprom32.writeByte((eepromAddress + ii), appo);
  }
}

uint32_t EEPROMReadIntValue(uint16_t eepromAddress, uint8_t eepromVLength)
{
  uint32_t multiplayer = 256;
  uint32_t appo = eeprom32.readByte(eepromAddress);
  for (uint8_t ii = 1; ii < eepromVLength; ii++)
  {
    appo += (eeprom32.readByte((eepromAddress + ii)) * multiplayer);
    multiplayer *= 256;
  }
  return(appo);
}
