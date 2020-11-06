#include <Adafruit_SI5351.h>
#include "Wire.h"

//Si5351 HSClockGen;
Adafruit_SI5351 HSClockGen = Adafruit_SI5351();



void FrequencyOut()
{
  HSClockGen.setupPLL(SI5351_PLL_B, 35, 1, 1);

  HSClockGen.setupMultisynth(0, 
                             SI5351_PLL_B,
                             int(900000000/FreqTX),
                             ((900000000.0/float(FreqTX)) - int((900000000.0/float(FreqTX)))/1000000),
                             1000000);
  HSClockGen.setupMultisynth(1, 
                             SI5351_PLL_B,
                             int(900000000/(FreqRX - IFreq)),
                             (((900000000.0/float(FreqRX - IFreq)) - int((900000000.0/float(FreqRX - IFreq))))/1000000),
                             1000000);
  HSClockGen.setupMultisynth(1, SI5351_PLL_B, 23, 1, 2);
  HSClockGen.setupMultisynth(0, SI5351_PLL_B, 24, 1, 2);
  HSClockGen.enableOutputs(true);
}
