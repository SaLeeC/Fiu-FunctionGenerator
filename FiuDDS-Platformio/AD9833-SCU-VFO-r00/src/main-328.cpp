#include <Arduino.h>

#include "SSD1306OLEDManagement.cpp"
#include "RTCManagement.cpp"
#include "IICScanner.cpp"
#include "AT24C32Management.cpp"

#include <SPI.h>
#include <Wire.h>

// it's only 1Kbit!!!
#define EE24LC01MAXBYTES 1024/8


//Indirizza il device di uscita del programma 
#define OUTPUT_Device display


void setup() 
{
  Serial.begin(115200);

  //IICScann();

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  displayInit();
  EEPROMsetup();
  RTCsetup();
  displayFrequency(FreqTX, FreqRX, IFreq);
}

void loop() 
{
  RTCloop();
}
