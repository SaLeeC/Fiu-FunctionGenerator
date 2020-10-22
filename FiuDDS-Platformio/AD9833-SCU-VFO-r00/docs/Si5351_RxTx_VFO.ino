/*
  A part of this program is taken from Jason Mildrum, NT7S.
  All extra functions are written by me, Rob Engberts PA0RWE

  References:
  http://nt7s.com/
  http://sq9nje.pl/
  http://ak2b.blogspot.com/
  http://pa0rwe.nl/?page_id=804

 *  SI5351_VFO control program for Arduino NANO
 *  Copyright PA0RWE Rob Engberts
 *  
 *  Using the old Si5351 library by Jason Mildrun nt7s
 *
 *  Functions:
 *  - CLK0 - Tx frequency = Display frequency
 *  - CLK1 - Rx / RIT frequency = Tx +/- BFO (upper- or lower mixing)
 *           When RIT active, RIT frequency is displayed and is tunable.
 *           When RIT is inactive Rx = Tx +/- BFO
 *  - CLK2 - BFO frequency, tunable
 *
 *  - Stepsize:  select (pushbutton)
 *  - Calibrate: (pushbutton) calculates difference between X-tal and measured
 *               x-tal frequency, to correct x-tal frequency.
 *  - Selection: (pushbutton) Switch between TRx and BFO mode
 *  - RIT switch: tunable Rx frequency, while Tx frequency not changed
 *  - Programming PIC by ICSP
 *  
 *  Si5351 settings: I2C address is in the .h file
 *                   X-tal freq is in the .h file but set in line 354
 *
***************************************************************************
 *  02-04-2015   1.0    Start building program based on the PIC version
 *  18-06-2019   1.1    Extend frequency range to 10 KHz down
 *                      Update frequency display. Added 'MHz' and 'KHz'
 *                      Set max frequency to 100 MHz.
 *  21-06-2020   2.0    Changed text BFO to IF
 *                      Changed bfo_t => vfo_if, vfo_t => vfo_hf, vfo_r => vfo_lo
 *                      Make possible to use the vfo for upper or lower mixing
 *                        by using a #define statement (upper or lower)
 *                      Store of actual frequency after 5 seconds of not tuning  
 *                      Stepsize was not saved. Repaired
 *
***************************************************************************
*  Includes
**************************************************************************/
#include <Rotary.h>
#include <RWE_si5351.h> 
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>

/**************************************************************************
*  Define Upper / Lower mixing  (Remark // if not true)
**************************************************************************/
#define upper
//#define lower

/**************************************************************************
*  (Pin) Definitions
**************************************************************************/
#define ENCODER_A     2       // Encoder pin A INT0/PCINT18 D2
#define ENCODER_B     3       // Encoder pin B INT1/PCINT19 D3
#define ENCODER_BTN   4       // Encoder pushbutton D4
#define OLED_RESET    8       // OLED reset
#define Calibrbtn     5       // Calibrate
#define RIT_Switch    6       // RIT Switch
#define TX_Switch     7       // Select TRx or BFO
//      I2C-SDA       A4      // I2C-SDA
//      I2C-SCL       A5      // I2C-SCL

#define F_MIN            10000UL      // Lower frequency limit  10 KHz
#define F_MAX        100000000UL      // Upper frequency limit 100 MHz

/**************************************************************************
*  EEPROM data locations 
**************************************************************************/
#define EE_SAVED_RADIX  0   // Stepsize pointer
#define EE_SAVED_AFREQ  4   // Actual Tx Frequency (CLK0)
#define EE_SAVED_BFREQ  8   // BFO (IF) Frequency  (CLK2)
#define EE_SAVED_XFREQ  12  // X-tal frequency  (25 or 27 MHz)
#define EE_SAVED_OFSET  16  // store correction
#define EE_SAVED_CALBR  20  // calibrated indicator

Adafruit_SSD1306 display(OLED_RESET);
Si5351 si5351;
Rotary r = Rotary(ENCODER_A, ENCODER_B);

/**************************************************************************
*  Declarations
**************************************************************************/
volatile uint32_t vfo_if =  900000000ULL / SI5351_FREQ_MULT;   // CLK0 start IF
volatile uint32_t vfo_hf = 1420000000ULL / SI5351_FREQ_MULT;   // CLK2 start Tx freq
volatile uint32_t vfo_lo = vfo_hf - vfo_if;                    // CLK1 start Rx freq
volatile uint32_t vfo_s = vfo_hf;                              // Saved for RIT
uint32_t vco_c = 0;                                            // X-tal correction factor
uint32_t xt_freq;
uint32_t test = 0;
uint32_t radix = 100ULL, old_radix = 100ULL;                   //start step size
boolean changed_f = 0, stepflag = 0, calflag = 0, modeflag = 0, ritset = 0;
boolean calibrate = 0;
int  act_clk = 0, disp_txt = 0;

unsigned long startTime;
unsigned long endTime;
unsigned long duration;
byte timerRunning;
unsigned long currentMillis = millis();
unsigned long startMillis = 0;
boolean  actupdt = false;

/**************************************/
/* Interrupt service routine for      */
/* encoder frequency change           */
/**************************************/
ISR(PCINT2_vect) {
  char result = r.process();
  if (result == DIR_CW)
    set_frequency(1);
  else if (result == DIR_CCW)
    set_frequency(-1);
}


/**************************************/
/* Change the frequency               */
/* dir = 1    Increment               */
/* dir = -1   Decrement               */
/**************************************/
void set_frequency(short dir)
{
  switch (act_clk)
  {
    case 0:                 // HF frequency
      if (dir == 1)
        vfo_hf += radix;
      if (dir == -1) {
        if (vfo_hf < radix) break; // to prevent negative value
        vfo_hf -= radix;
      }
      break;
    case 1:                 // HF frequency (only if RIT is on)
      if (dir == 1)
        vfo_hf += radix;
      if (dir == -1) {
        if (vfo_hf < radix) break; // to prevent negative value      
        vfo_hf -= radix;
      }
      break;
    case 2:                 // IF frequency
      if (dir == 1)
        vfo_if += radix;
      if (dir == -1) {
        if (vfo_if < radix) break; // to prevent negative value      
        vfo_if -= radix;
      }
      break;
  }

  if(vfo_hf > F_MAX)
    vfo_hf = F_MAX;
  if(vfo_hf < F_MIN)
    vfo_hf = F_MIN;

  changed_f = 1;
}


/**************************************/
/* Read the buttons with debouncing   */
/**************************************/
boolean get_button()
{
  if (!digitalRead(ENCODER_BTN))            // Stepsize
  {
    delay(20);
    if (!digitalRead(ENCODER_BTN))
    {
      while (!digitalRead(ENCODER_BTN));
      stepflag = 1;      
    }
  }
  else if (!digitalRead(Calibrbtn))         // Calibrate
  {
    delay(20);
    if (!digitalRead(Calibrbtn))
    {
      while (!digitalRead(Calibrbtn));
      calflag = 1;
    }
  }
  else if (!digitalRead(TX_Switch))         // Selection
  {
    delay(20);
    if (!digitalRead(TX_Switch))
    {
      while (!digitalRead(TX_Switch));
      modeflag = 1;
    }
  }  
  if (stepflag | calflag | modeflag) return 1;
  else return 0;
}


/********************************************************************************
 * RIT switch handling
 * Switch to small stepsize (100 Hz)
 *******************************************************************************/
void RIT_switch()                               // Read RIT_switch
{
  if (!digitalRead(RIT_Switch) && ritset == 0){     // RIT on
    act_clk = 1;
    ritset = 1;
    vfo_s = vfo_hf;                             // Save Tx freq
    old_radix = radix;                          // Save actual stepsize
    radix = 100;                                // Set stepsize to 100 Hz
  }
  else if (digitalRead(RIT_Switch) && ritset == 1){ // RIT 0ff
    act_clk = 0;                                // RTx mode
    ritset = 0;
    vfo_hf = vfo_s;                             // Restore to original vco_t
    radix = old_radix;                          // Back to old stepsize
    disp_txt = 0;                               // Clear line
    
// Update LO frequency based on the restored LO frequency    
    #if defined(upper)
      vfo_lo = vfo_hf + vfo_if;                   // Upper / lower mixing
    #endif
    #if defined(lower) 
      vfo_lo = vfo_hf - vfo_if;    
    #endif
    si5351.set_freq((vfo_lo * SI5351_FREQ_MULT), SI5351_PLL_FIXED, SI5351_CLK1);
  }
}

/**************************************/
/* Displays the frequency and stepsize*/
/**************************************/
void display_frequency()
{
  char LCDstr[10];
  char Hertz[7];
  int p,q = 0;
  unsigned long freq;
  display.clearDisplay();

  switch(act_clk)
  {
    case 0:                               // HF frequency
      freq = vfo_hf;
      break;
    case 1:                               // HF frequency (Used in RIT Mode)
      freq = vfo_hf;
      break;
    case 2:                               // IF frequency
      freq = vfo_if;
      break;
  }

  Hertz[1]='\0';                           // empty array

  sprintf(LCDstr, "%ld", freq);           // convert freq to string
  p=strlen(LCDstr);                       // determine length
  display.setCursor(100,16);
  display.setTextSize(1);  
  if (p>6){                               // MHz
    display.print(F("MHz"));
    q=p-6;
    strcpy(Hertz,LCDstr);                 // get Herz digits (6)
    strcpy(LCDstr+q,Hertz+(q-1));         // copy into LCDstr and add to MHz
    LCDstr[q]='.';                        // decimal point
  }
  else {                                  // KHz
    display.print(F("KHz"));
    q=p-3;
    strcpy(Hertz,LCDstr);                 // get Herz digits (3)
    strcpy(LCDstr+q,Hertz+(q-1));         // copy into LCDstr and add to KHz
    LCDstr[q]='.';                        // decimal point
  }

  switch (p)                              // display stepsize
  {
    case 5:                               //  10 KHZ
      display.setCursor(36,0);
      break;
    case 6:                               // 100 KHZ
      display.setCursor(24,0);
      break;
    case 7:                               //   1 MHZ
      display.setCursor(12,0);
      break;
    case 8:                               //  10 MHZ
      display.setCursor(0,0);
      break;
    case 9:                               // 100 MHZ
      display.setCursor(0,0);
      break;
  }
    
  display.setTextSize(2);  
  display.println(LCDstr);
  display_settings();
}


/**************************************/
/* Displays step, mode and version    */
/**************************************/
void display_settings()
{
// Stepsize  
  display.setCursor(8, 40);  
  display.setTextSize(1);  
  display.print(F("Step:"));
  switch (radix)
  {
    case 1:
      display.println(F("   1Hz"));
      break;
    case 10:
      display.println(F("  10Hz"));
      break;
    case 100:
      display.println(F(" 100Hz"));
      break;
    case 1000:
      display.println(F("  1kHz"));
      break;
    case 10000:
      display.println(F(" 10kHz"));
      break;
    case 100000:
      display.println(F("100kHz"));
      break;
    case 1000000:
      display.println(F("  1MHz"));
      break;
  }

// Mode
  display.setCursor(100, 40);  
  switch (act_clk)
  {
    case 0:
      display.println(F("TRx"));
      break;
    case 1:
      display.println(F("RIT"));
      break;
    case 2:
      display.println(F("IF "));
      break;
  }

// Version  
  display.setCursor(15, 55);  
  display.print(F("Si5351 vfo V.2.0"));

// Messages
  display.setCursor(12, 25);
  switch (disp_txt)
  {
    case 0:
      display.print(F("                 "));   // clear line    
      break;
    case 1:
      display.print(F("** Turn RIT Off *"));
      break;
    case 2:
      display.print(F("*** Set to TRx **"));
      break;
    case 3:
      display.print(F("** Calibration **"));
      break;      
    case 4:
      display.print(F("* Calibration OK!"));
      break;      
  }
  display.display();
}


/**************************************/
/*            S E T U P               */
/**************************************/
void setup()
{
  Serial.begin(115200);
  Wire.begin();
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x64)

// Read EEPROM
  radix = eeprom_read_dword((const uint32_t *)EE_SAVED_RADIX);
  Serial.println(radix);
  if ((radix < 10ULL) | (radix > 1000000ULL)) radix = 100ULL;  
  
  vfo_hf = eeprom_read_dword((const uint32_t *)EE_SAVED_AFREQ);
  if ((vfo_hf < F_MIN) | (vfo_hf > F_MAX)) vfo_hf = 14000000ULL;
  
  test = (vfo_hf / 10);                       // Round to 10Hz
  vfo_hf = test * 10;

  vfo_if = eeprom_read_dword((const uint32_t *)EE_SAVED_BFREQ);
  if ((vfo_if < F_MIN) | (vfo_if > F_MAX)) vfo_if = 9000000ULL;  

  test = (vfo_if / 10);                       // Round to 10Hz
  vfo_if = test * 10;

  vco_c = 0;
  if (eeprom_read_dword((const uint32_t *)EE_SAVED_CALBR) == 0x60)  {
    vco_c = eeprom_read_dword((const uint32_t *)EE_SAVED_OFSET);
  }  
  xt_freq = SI5351_XTAL_FREQ + vco_c;

//initialize the Si5351
  si5351.set_correction(0); // Set to zero because I'm using an other calibration method
  si5351.init(SI5351_CRYSTAL_LOAD_8PF, xt_freq);    // Frequency get from settings in VFO_si5351.h file
  si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLA);
  
// Set CLK0 to output the starting "HF" frequency as set above by vfo = ?
  si5351.set_freq((vfo_hf * SI5351_FREQ_MULT), SI5351_PLL_FIXED, SI5351_CLK0);
  si5351.drive_strength(SI5351_CLK0,SI5351_DRIVE_2MA);
// Set CLK1 to output the LO frequncy = HF +/- IF frequency
  #if defined(upper)
    vfo_lo = vfo_hf + vfo_if;                   // Upper / lower mixing
  #endif
  #if defined(lower) 
    vfo_lo = vfo_hf - vfo_if;    
  #endif
  si5351.set_freq((vfo_lo * SI5351_FREQ_MULT), SI5351_PLL_FIXED, SI5351_CLK1);
  si5351.drive_strength(SI5351_CLK1,SI5351_DRIVE_2MA);
// Set CLK2 to output bfo frequency
  si5351.set_freq((vfo_if * SI5351_FREQ_MULT), SI5351_PLL_FIXED, SI5351_CLK2);
  si5351.drive_strength(SI5351_CLK2,SI5351_DRIVE_2MA);

// Clear the buffer.
  display.clearDisplay();
  display.display();
  
// text display tests
  display.setTextSize(1);
  display.setTextColor(WHITE);
  
// Encoder setup
  pinMode(ENCODER_BTN, INPUT_PULLUP);
  PCICR |= (1 << PCIE2);                       // Enable pin change interrupt for the encoder
  PCMSK2 |= (1 << PCINT18) | (1 << PCINT19);

  sei();

// Pin Setup
  pinMode(Calibrbtn, INPUT_PULLUP);   // Calibrate
  pinMode(RIT_Switch, INPUT_PULLUP);  // RIT Switch
  pinMode(TX_Switch, INPUT_PULLUP);   // Select TRx or BFO

//  Timer
  startMillis = millis();               // Reset actual freq store timer
  actupdt = true;    
  
// Display first time  
  display_frequency();  // Update the display
}

/**************************************/
/*             L O O P                */
/**************************************/
void loop()
{
  if (disp_txt == 4) {
    delay(3000);                  // Display calibration OK and wait 3 seconds
    disp_txt = 0;
  }

//  get_button();
  
// Update the display if the frequency has been changed
  if (changed_f)  {
    display_frequency();

    if (act_clk == 0 && !calibrate)                   // No Tx update during calibrate
      si5351.set_freq((vfo_hf * SI5351_FREQ_MULT), SI5351_PLL_FIXED, SI5351_CLK0);
    else if (act_clk == 2)                            // BFO update
      si5351.set_freq((vfo_if * SI5351_FREQ_MULT), SI5351_PLL_FIXED, SI5351_CLK2);      
// Update LO frequency    
    #if defined(upper)
      vfo_lo = vfo_hf + vfo_if;                   // Upper / lower mixing
    #endif
    #if defined(lower) 
      vfo_lo = vfo_hf - vfo_if;    
    #endif
    si5351.set_freq((vfo_lo * SI5351_FREQ_MULT), SI5351_PLL_FIXED, SI5351_CLK1);

    changed_f = 0;
    disp_txt = 0;                   // Clear line
    startMillis = millis();         // Reset actual freq store timer
    actupdt = false;  
  }

  RIT_switch();                     // read RIT switch

// Store actual freq and other settings once per 5 seceonds after frequency change
  if((millis() - startMillis) > 5000 && actupdt == false) {
    eeprom_write_dword((uint32_t *)EE_SAVED_AFREQ, vfo_hf);
    startMillis = millis();                   // Reset actual freq store timer   
    actupdt = true;
    Serial.println("Act freq stored"); 
  }
  
// Button press
// Also stored the last used frequency together with the step size before store
//
  if (get_button()) {
    if (stepflag) {                 // Stepsize button
      switch (radix)
      {
      case 1:
        radix = 10;
        break;
      case 10:
        radix = 100;
        break;
      case 100:
        radix = 1000;
        break;
      case 1000:
        radix = 10000;
        break;
      case 10000:
        radix = 100000;
        break;
      case 100000:
        radix = 1000000;
        break;
      case 1000000:
        radix = 10;
        break;       
      }
      eeprom_write_dword((uint32_t *)EE_SAVED_RADIX, radix);  // Store stepsize
      stepflag  = 0;
    }
    else if (modeflag)  {         // Mode button
      if (act_clk == 0) act_clk = 2; else act_clk = 0;
      eeprom_write_dword((uint32_t *)EE_SAVED_BFREQ, vfo_if);
      modeflag = 0;  
      disp_txt = 0;                                 // Clear line
    }
    else if (calflag) {                             // Calibrate button
      if (!digitalRead(RIT_Switch)){                // RIT is on
        disp_txt = 1;                               // Message RIT off
      }
      else if (act_clk == 2){                       // BFO mode on
        disp_txt = 2;                               // Message BFO off        
      }
      else if (!calibrate)  {                       // Start calibrate
        vfo_s = vfo_hf;                             // Save actual freq
        old_radix = radix;                          // and stepsize
        vfo_hf = SI5351_XTAL_FREQ;                  // en set to default x-tal
        disp_txt = 3;                               // Message Calibrate
        calibrate = 1;
        radix = 10;                                 // Set to 10 Hz        
        si5351.set_freq((vfo_hf * SI5351_FREQ_MULT), SI5351_PLL_FIXED, SI5351_CLK0); // Set HF Freq
      }
      else if (calibrate) {                         // after tuning x-tal freq
        calibrate = 0;
        vco_c = vfo_hf - SI5351_XTAL_FREQ;          // difference
        vfo_hf = vfo_s;                             // restore freq
        radix = old_radix;                          // and stepsize
        disp_txt = 4;                               // Message Calibrate OK
        
        eeprom_write_dword((uint32_t *)EE_SAVED_OFSET, vco_c);        // store correction
        xt_freq = SI5351_XTAL_FREQ + vco_c;                           // Calibrated x-tal freq
        eeprom_write_dword((uint32_t *)EE_SAVED_CALBR, 0x60);         // Calibrated
        si5351.init(SI5351_CRYSTAL_LOAD_8PF, xt_freq);                // Initialize
        si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLA);        
        si5351.set_freq(vfo_if * SI5351_FREQ_MULT, SI5351_PLL_FIXED, SI5351_CLK2);   // correct BFO frequency
        si5351.set_freq(vfo_hf * SI5351_FREQ_MULT, SI5351_PLL_FIXED, SI5351_CLK0);   // Correct HF freq
        #if defined(upper)
          vfo_lo = vfo_hf + vfo_if;                 // Upper / lower mixing
        #endif
        #if defined(lower) 
          vfo_lo = vfo_hf - vfo_if;    
        #endif
        si5351.set_freq(vfo_lo * SI5351_FREQ_MULT, SI5351_PLL_FIXED, SI5351_CLK1);   // correct LO frequency
      }
      calflag = 0;
    }
  }    
  display_frequency();                              // Update display
} // end while loop
