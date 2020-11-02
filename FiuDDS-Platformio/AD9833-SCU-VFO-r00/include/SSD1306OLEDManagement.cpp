
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

void displayInit();
void displayFrequency(uint_fast32_t Freq0, uint_fast32_t Freq1, uint32_t IFreq);
void displayFrequencyLoop(uint8_t Xx, uint8_t iiiY, uint8_t Size, uint32_t Freq, String Titolo);

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)

#define NumCifre 8

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

void displayInit()
{
  display.clearDisplay();
  display.display();
  display.setTextSize(2);             // Normal 1:1 pixel scale
  display.setCursor(0,2);
  display.setTextColor(SSD1306_WHITE);        // Draw white text
  display.print("OLED OK");
  display.display();
  delay(3000);
}

void displayFrequency(uint32_t Freq0, uint32_t Freq1, uint32_t IFreq)
//VCO 0 riga in alto
//VCO 1 riga in basso
{
  displayFrequencyLoop(0, 0, 2, Freq0, "");
  displayFrequencyLoop(58, 17, 1, Freq1, "");
  displayFrequencyLoop(58, 25, 1, IFreq, " IF");
  display.display();
}

void displayFrequencyLoop(uint8_t Xx, uint8_t iiiY, uint8_t Size, uint32_t Freq, String Titolo)
{
  uint8_t iiiXStep = 6 * Size;
  uint8_t iiiFlag = 0;//Marca la prima cifra significativa diversa da 0 (zero)
  uint8_t iiiValue = 0;//E' la cifra corrente
  uint8_t iiiX;//X
  uint8_t iiixSplit = 0;//E' lo spostamento per l'inserimento del punto
  display.setTextSize(Size);             // Large 2:1 pixel scale
  

for (uint32_t iii = 100000000; iii> 10; iii /= uint32_t (10))
  {
    iiiValue = Freq / iii;
    iiiX = Xx + (iiiXStep * (NumCifre - int(log10(iii)))) + iiixSplit;
    display.setCursor(iiiX, iiiY);
    if (iiiValue != 0)
    {
      if (iiiFlag == 0)
      {
        iiiFlag = 1;
      }
      display.print(iiiValue);
    }
    else
    {
      if (iiiFlag != 0)
      {
        display.print(iiiValue);
      }
    }
    Freq -= ((Freq / iii) * iii);
    if((iii == 1000000) & (iiiFlag != 0))
    {
      iiiX -= 3;//Sposta a sinistra di qualche punto per posizionare la virgola
      display.setCursor((iiiXStep + iiiX + (Size == 1)), (iiiY - (5 * Size)));
      display.print(".");
      iiixSplit = 3.5 * Size;//Sposta a destra per l'inserimento della virgola
    }
  }
  display.print("00");
  display.setTextSize(1);
  display.setCursor(iiiX + ((18 * Size) + (Size - 1)), (iiiY + (7 * (Size -1))));
  display.print("Hz");
  display.print(Titolo);
}