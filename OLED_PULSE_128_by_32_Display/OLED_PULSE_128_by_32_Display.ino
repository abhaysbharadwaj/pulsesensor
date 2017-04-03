
/*
  >> Pulse Sensor Amped 1.2 <<
  This code is for Pulse Sensor Amped by Joel Murphy and Yury Gitman
    www.pulsesensor.com
    >>> Pulse Sensor purple wire goes to Analog Pin 0 <<<
  Pulse Sensor sample aquisition and processing happens in the background via Ticker Routine
  The following variables are automatically updated:
  Signal :    int that holds the analog signal data straight from the sensor. updated every 2mS.
  IBI  :      int that holds the time interval between beats. 2mS resolution.
  BPM  :      int that holds the heart rate value, derived every beat, from averaging previous 10 IBI values.
  QS  :       boolean that is made true whenever Pulse is found and BPM is updated. User must reset.
  Pulse :     boolean that is true when a heartbeat is sensed then false in time with pin13 LED going out.

*/
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define pulsePin A0
#define OLED_RESET 0  // This is a dummy pin, as the display I have only has 4 Pins-SDA,SCL,GND,Vcc...
Adafruit_SSD1306 display(OLED_RESET);

// Set the Screen for the Pulse display
const int WIDTH = 128;
const int HEIGHT = 32;
const int LENGTH = WIDTH;

//  VARIABLES
int blinkPin = LED_BUILTIN;                // pin to blink led at each beat
//int fadePin = 12;                 // pin to do fancy classy fading blink at each beat
//int fadeRate = 0;                 // used to fade LED on with PWM on fadePin


// these variables are volatile because they are used during the interrupt service routine!
volatile int BPM;                   // used to hold the pulse rate
volatile int Signal;                // holds the incoming raw data
volatile int IBI = 600;             // holds the time between beats, must be seeded!
volatile boolean Pulse = false;     // true when pulse wave is high, false when it's low
volatile boolean QS = false;        // becomes true when Arduoino finds a beat.

// For the display

int x;
int y[LENGTH];

void clearY()
{
  for (int i = 0; i < LENGTH; i++)
  {
    y[i] = -1;
  }
}

void drawY()
{
  display.drawPixel(0, y[0], WHITE);
  for (int i = 1; i < LENGTH; i++)
  {
    if (y[i] != -1)
    {
      display.drawLine(i - 1, y[i - 1], i, y[i], WHITE);
    } else
    {
      break;
    }
  }
}

volatile int rate[10];                    // array to hold last ten IBI values
volatile unsigned long sampleCounter = 0;          // used to determine pulse timing
volatile unsigned long lastBeatTime = 0;           // used to find IBI
volatile int P =512;                      // used to find peak in pulse wave, seeded
volatile int T = 512;                     // used to find trough in pulse wave, seeded
volatile int thresh = 525;                // used to find instant moment of heart beat, seeded
volatile int amp = 100;                   // used to hold amplitude of pulse waveform, seeded
volatile boolean firstBeat = true;        // used to seed rate array so we startup with reasonable BPM
volatile boolean secondBeat = false;      // used to seed rate array so we startup with reasonable BPM


void interruptSetup(){     
  // Initializes Timer2 to throw an interrupt every 2mS.
  TCCR2A = 0x02;     // DISABLE PWM ON DIGITAL PINS 3 AND 11, AND GO INTO CTC MODE
  TCCR2B = 0x06;     // DON'T FORCE COMPARE, 256 PRESCALER 
  OCR2A = 0X7C;      // SET THE TOP OF THE COUNT TO 124 FOR 500Hz SAMPLE RATE
  TIMSK2 = 0x02;     // ENABLE INTERRUPT ON MATCH BETWEEN TIMER2 AND OCR2A
  sei();             // MAKE SURE GLOBAL INTERRUPTS ARE ENABLED      
} 


// THIS IS THE TIMER 2 INTERRUPT SERVICE ROUTINE. 
// Timer 2 makes sure that we take a reading every 2 miliseconds
ISR(TIMER2_COMPA_vect){                         // triggered when Timer2 counts to 124
  cli();                                      // disable interrupts while we do this
  Signal = analogRead(pulsePin);              // read the Pulse Sensor 
  sampleCounter += 2;                         // keep track of the time in mS with this variable
  int N = sampleCounter - lastBeatTime;       // monitor the time since the last beat to avoid noise

    //  find the peak and trough of the pulse wave
  if(Signal < thresh && N > (IBI/5)*3){       // avoid dichrotic noise by waiting 3/5 of last IBI
    if (Signal < T){                        // T is the trough
      T = Signal;                         // keep track of lowest point in pulse wave 
    }
  }

  if(Signal > thresh && Signal > P){          // thresh condition helps avoid noise
    P = Signal;                             // P is the peak
  }                                        // keep track of highest point in pulse wave

  //  NOW IT'S TIME TO LOOK FOR THE HEART BEAT
  // signal surges up in value every time there is a pulse
  if (N > 250){                                   // avoid high frequency noise
    if ( (Signal > thresh) && (Pulse == false) && (N > (IBI/5)*3) ){        
      Pulse = true;                               // set the Pulse flag when we think there is a pulse
      digitalWrite(blinkPin,HIGH);                // turn on pin 13 LED
      IBI = sampleCounter - lastBeatTime;         // measure time between beats in mS
      lastBeatTime = sampleCounter;               // keep track of time for next pulse

      if(secondBeat){                        // if this is the second beat, if secondBeat == TRUE
        secondBeat = false;                  // clear secondBeat flag
        for(int i=0; i<=9; i++){             // seed the running total to get a realisitic BPM at startup
          rate[i] = IBI;                      
        }
      }

      if(firstBeat){                         // if it's the first time we found a beat, if firstBeat == TRUE
        firstBeat = false;                   // clear firstBeat flag
        secondBeat = true;                   // set the second beat flag
        sei();                               // enable interrupts again
        return;                              // IBI value is unreliable so discard it
      }   


      // keep a running total of the last 10 IBI values
      word runningTotal = 0;                  // clear the runningTotal variable    

      for(int i=0; i<=8; i++){                // shift data in the rate array
        rate[i] = rate[i+1];                  // and drop the oldest IBI value 
        runningTotal += rate[i];              // add up the 9 oldest IBI values
      }

      rate[9] = IBI;                          // add the latest IBI to the rate array
      runningTotal += rate[9];                // add the latest IBI to runningTotal
      runningTotal /= 10;                     // average the last 10 IBI values 
      BPM = 60000/runningTotal;               // how many beats can fit into a minute? that's BPM!
      QS = true;                              // set Quantified Self flag 
      // QS FLAG IS NOT CLEARED INSIDE THIS ISR
    }                       
  }

  if (Signal < thresh && Pulse == true){   // when the values are going down, the beat is over
    digitalWrite(blinkPin,LOW);            // turn off pin 13 LED
    Pulse = false;                         // reset the Pulse flag so we can do it again
    amp = P - T;                           // get amplitude of the pulse wave
    thresh = amp/2 + T;                    // set thresh at 50% of the amplitude
    P = thresh;                            // reset these for next time
    T = thresh;
  }

  if (N > 2500){                           // if 2.5 seconds go by without a beat
    thresh = 512;                          // set thresh default
    P = 512;                               // set P default
    T = 512;                               // set T default
    lastBeatTime = sampleCounter;          // bring the lastBeatTime up to date        
    firstBeat = true;                      // set these to avoid noise
    secondBeat = false;                    // when we get the heartbeat back
  }

  sei();                                   // enable interrupts when youre done!
}// end isr
void setup()
{
  Serial.begin(19200);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x64)
  delay(20);
  // Clear the buffer.
  display.clearDisplay();

  x = 0;
  clearY();
  pinMode(blinkPin, OUTPUT);        // pin that will blink to your heartbeat!
  //pinMode(fadePin, OUTPUT);         // pin that will fade to your heartbeat!
  Serial.begin(115200);             // we agree to talk fast!
  interruptSetup();                 // sets up to read Pulse Sensor signal every 2mS
  display.setCursor(0, 0);
  display.print("  Calculating BPM ");
  Serial.println(" ");
  Serial.println("  Calculating BPM ");
  display.display();
}

void loop()
{
  y[x] = map(Signal, 0, 1023, HEIGHT+10, 0); // Leave some screen for the text.....
  drawY();
  x++;
  if (x >= WIDTH)
  {
    display.clearDisplay();
    display.drawLine(0, 51, 127, 51, WHITE);
    display.drawLine(0, 31, 127, 31, WHITE);
    display.setTextSize(0);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.print(" Heart Rate = ");
    display.print(BPM);
    display.println(" BPM");
    Serial.print(" BPM = ");
    Serial.println(BPM);
    x = 0;
    clearY();
  }
  display.display();
  delay(10);                             //  take a break
}

