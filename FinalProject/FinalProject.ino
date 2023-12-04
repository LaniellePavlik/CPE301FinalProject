/*
Names: John Glenn, Aleena Kureekattil, Lanielle Pavlik
CPE 301 Final Project
*/

#include <dht.h>
#include <LiquidCrystal.h>
#include <RTClib.h>
#include <Stepper.h>

#define RDA 0x80
#define TBE 0x20

// Temperature setup
dht DHT;
#define DHT11_PIN 3
#define TEMP_THRESHOLD 24

// Start/stop and reset button pins
#define START_STOP_PIN 19
#define RESET_PIN 18

// Water threshold
#define WATER_THRESHOLD 200

//RTC module
RTC_DS1307 myrtc;

// LCD setup
const int RS=53, EN=51, D4=49, D5=47, D6=45, D7=43;
LiquidCrystal lcd(RS, EN, D4, D5, D6, D7);

// Stepper motor setup
const int IN1=23, IN2=25, IN3=27, IN4=29;
const int stepsPerRevolution = 2038;
Stepper myStepper = Stepper(stepsPerRevolution, IN1, IN3, IN2, IN4);

//UART pointers:
volatile unsigned char *myUCSR0A  = (unsigned char *)0x00C0;
volatile unsigned char *myUCSR0B  = (unsigned char *)0x00C1;
volatile unsigned char *myUCSR0C  = (unsigned char *)0x00C2;
volatile unsigned int  *myUBRR0   = (unsigned int *) 0x00C4;
volatile unsigned char *myUDR0    = (unsigned char *)0x00C6;

//pin pointers:

//A0 = button (22)
volatile unsigned char *portA     = (unsigned char *) 0x22;
volatile unsigned char *portDDRA  = (unsigned char *) 0x21;
volatile unsigned char *pinA      = (unsigned char *) 0x20;

//B4-7 = Status LEDs (10-13)
volatile unsigned char *portB     = (unsigned char *) 0x25;
volatile unsigned char *portDDRB  = (unsigned char *) 0x24;

// C3 = vent up C7 = vent down
volatile unsigned char *portC     = (unsigned char *) 0x28;
volatile unsigned char *portDDRC  = (unsigned char *) 0x27;
volatile unsigned char *pinC      = (unsigned char *) 0x26;

//D2 = start button (19)
volatile unsigned char *portD     = (unsigned char *) 0x2B;
volatile unsigned char *portDDRD  = (unsigned char *) 0x2A;
volatile unsigned char *pinD      = (unsigned char *) 0x29;

//E3 = LCD Register Select (5), E5 = Temperature/Humididty (3)
volatile unsigned char *portE     = (unsigned char *) 0x2E;
volatile unsigned char *portDDRE  = (unsigned char *) 0x2D;
volatile unsigned char *pinE      = (unsigned char *) 0x2C;

//F0 = Water Sensor (A0)
volatile unsigned char *portF     = (unsigned char *) 0x31;
volatile unsigned char *portDDRF  = (unsigned char *) 0x30;
volatile unsigned char *pinF      = (unsigned char *) 0x2F;

//G5 = LCD Enable (4)
volatile unsigned char *portG     = (unsigned char *) 0x34;
volatile unsigned char *portDDRG  = (unsigned char *) 0x33;
volatile unsigned char *pinG      = (unsigned char *) 0x32;

//H3-6 = LCD Data Pins (6-9)
volatile unsigned char *portH     = (unsigned char *) 0x102;
volatile unsigned char *portDDRH  = (unsigned char *) 0x101;
volatile unsigned char *pinH      = (unsigned char *) 0x100;

//ADC pointers
volatile unsigned char* my_ADMUX = (unsigned char*) 0x7C;
volatile unsigned char* my_ADCSRB = (unsigned char*) 0x7B;
volatile unsigned char* my_ADCSRA = (unsigned char*) 0x7A;
volatile unsigned int* my_ADC_DATA = (unsigned int*) 0x78;

//timer pointers
volatile unsigned char *myTCCR1A = (unsigned char *) 0x80;
volatile unsigned char *myTCCR1B = (unsigned char *) 0x81;
volatile unsigned char *myTCCR1C = (unsigned char *) 0x82;
volatile unsigned char *myTIMSK1 = (unsigned char *) 0x6F;
volatile unsigned int  *myTCNT1  = (unsigned  int *) 0x84;
volatile unsigned char *myTIFR1 =  (unsigned char *) 0x36;

/* States:
0: Disabled
1: Idle
2: Running
3: Error */
int state, nextState;
char stateNames[4][9] = {"Disabled", "Idle", "Running", "Error"};
int waterLevel;

void setup() {
  // Initialize pins
  *portDDRB |= 0b11110000; // set PB4-7 to output (LEDs)
  *portB &= 0b00001111; // set PB4-7 to low 

  *portDDRA &= 0b11111110; // set PA0 to input
  *portA &= 0b11111110; // disable pullup on PA0

  *portDDRA |= 0b00000100; // set PA2 to output
  *portA &= 0b11111011; // set PA2 to low

  *portDDRC &=0b01110111; // set PC3 and PC7 to input
  *portC &= 0b01110111; // disable pullup

  *portDDRD &= 0b00000100; // set PD2 to input
  *portD &= 0b11111011; // disable pullup on PD2

  // Initialize serial
  U0Init(9600);

  // Initialize states
  state = 0;
  nextState = 0;

  // attach interrupt for start/stop and reset buttons
  attachInterrupt(digitalPinToInterrupt(START_STOP_PIN), startStopButton, RISING);
  attachInterrupt(digitalPinToInterrupt(RESET_PIN), resetButton, RISING);
  
  // Initialize clock
  myrtc.begin();
  myrtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  
  // Initialize LCD display
  lcd.begin(16,2);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.write("working");

  // Initialize ADC
  adc_init();

  // Initialize stepper speed
  myStepper.setSpeed(5);
}

void loop() {
  // put your main code here, to run repeatedly:
  if(state != nextState){
    reportTransition();
    if(nextState == 2){
      reportFanOn();
    }else if(state == 2){
      reportFanOff();
    }
  }
  state = nextState;
  if(state == 0){
    // Disabled
    // update LEDS
    write_pb(7, 1);
    write_pb(6, 0);
    write_pb(5, 0);
    write_pb(4, 0);

    // Stop fan motor
    *portA &= 0b11111011; // set PA2 to low

    // Update screen
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Disabled");
    // do not detect transitions, start button implemented through ISR
  }else if (state == 1){
    // Idle
    // update LEDS
    write_pb(7, 0);
    write_pb(6, 1);
    write_pb(5, 0);
    write_pb(4, 0);

    // Stop fan motor
    *portA &= 0b11111011; // set PA2 to low

    // Read and display temperature and humidity
    int chk = DHT.read11(DHT11_PIN);
    displayTempHumidity();

    // Read water level
    waterLevel = adc_read(0);

    // Read vent button
    // move vent up
    while(*pinC & 0b10000000){
      reportVentUp();
      myStepper.step(100);
    }
    // move vent down
    while(*pinC & 0b00001000){
      reportVentDown();
      myStepper.step(-100);
    }

    // Test for state transitions
    // if waterLevel <= threshold, go to error
    if(waterLevel <= WATER_THRESHOLD){
      nextState = 3;
    // if temp > threshold, go to running
    }else if(DHT.temperature > TEMP_THRESHOLD){
      nextState = 2;
    }
  }else if(state == 2){
    // Running
    // update LEDS
    write_pb(7, 0);
    write_pb(6, 0);
    write_pb(5, 1);
    write_pb(4, 0);

    // Start fan motor
    *portA |= 0b00000100; // set PA2 to high

    // Read and display temperature and humidity
    int chk = DHT.read11(DHT11_PIN);
    displayTempHumidity();

    // Read vent button
    // move vent up
    while(*pinC & 0b10000000){
      reportVentUp();
      myStepper.step(100);
    }
    // move vent down
    while(*pinC & 0b00001000){
      reportVentDown();
      myStepper.step(-100);
    }

    // Read water level
    waterLevel = adc_read(0);

    // Test for state transitions
    // if waterLevel < threshold, go to error
    if(waterLevel < WATER_THRESHOLD){
      nextState = 3;
    // if temp <= threshold, go to idle
    }else if(DHT.temperature <= TEMP_THRESHOLD){
      nextState = 1;
    }
  }else if (state == 3){
    // Error
    // update LEDS
    write_pb(7, 0);
    write_pb(6, 0);
    write_pb(5, 0);
    write_pb(4, 1);

    // Stop fan motor
    *portA &= 0b11111011; // set PA2 to low

    // update LCD
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("ERROR: Water");
    lcd.setCursor(0,1);
    lcd.print("level is too low");

    // Read vent button
    // move vent up
    while(*pinC & 0b10000000){
      reportVentUp();
      myStepper.step(100);
    }
    // move vent down
    while(*pinC & 0b00001000){
      reportVentDown();
      myStepper.step(-100);
    }
  }
  my_delay(1);
}

// UART functions
// copied from Lab 8
void U0Init(int U0baud)
{
 unsigned long FCPU = 16000000;
 unsigned int tbaud;
 tbaud = (FCPU / 16 / U0baud - 1);
 // Same as (FCPU / (16 * U0baud)) - 1;
 *myUCSR0A = 0x20;
 *myUCSR0B = 0x18;
 *myUCSR0C = 0x06;
 *myUBRR0  = tbaud;
}
unsigned char kbhit()
{
  return *myUCSR0A & RDA;
}
unsigned char getChar()
{
  return *myUDR0;
}
void putChar(unsigned char U0pdata)
{
  while((*myUCSR0A & TBE)==0);
  *myUDR0 = U0pdata;
}

//port B output function
void write_pb(unsigned char pin_num, unsigned char state){
  if(state == 0){
    *portB &= ~(0x01 << pin_num);
  }
  else{
    *portB |= 0x01 << pin_num;
  }
}

void reportTime(){
  DateTime now = myrtc.now();
  putChar(0x30 + (now.hour()/10)%10);
  putChar(0x30 + now.hour()%10);
  putChar(':');
  putChar(0x30 + (now.minute()/10)%10);
  putChar(0x30 + now.minute()%10);
  putChar(':');
  putChar(0x30 + (now.second()/10)%10);
  putChar(0x30 + now.second()%10);
}

//RTC to serial monitor
void reportTransition(){
  const char string1[] = "Transition from state ";
  const char string2[] = " to state ";
  const char string3[] = " at time ";
  for(int i =0; i < strlen(string1); i++ ) {
    char c = string1[i];
    putChar(c);
  }
  putChar(0x30 + state);
  for(int i =0; i < strlen(string2); i++ ) {
    char c = string2[i];
    putChar(c);
  }
  putChar(0x30 + nextState);
  for(int i =0; i < strlen(string3); i++ ) {
    char c = string3[i];
    putChar(c);
  }
  reportTime();
  putChar('\n');
}

void reportVentUp(){
  const char string1[] = "Moving vent up at time ";
  for(int i =0; i < strlen(string1); i++ ) {
    char c = string1[i];
    putChar(c);
  }
  reportTime();
  putChar('\n');
}

void reportVentDown(){
  const char string1[] = "Moving vent down at time ";
  for(int i =0; i < strlen(string1); i++ ) {
    char c = string1[i];
    putChar(c);
  }
  reportTime();
  putChar('\n');
}

void reportFanOn(){
  const char string1[] = "Fan turned on at time ";
  for(int i =0; i < strlen(string1); i++ ) {
    char c = string1[i];
    putChar(c);
  }
  reportTime();
  putChar('\n');
}

void reportFanOff(){
  const char string1[] = "Fan turned off at time ";
  for(int i =0; i < strlen(string1); i++ ) {
    char c = string1[i];
    putChar(c);
  }
  reportTime();
  putChar('\n');
}

// ISR for the start button
void startStopButton(){
  // if state == Disabled
  if(state == 0){
    // Set state to Idle;
    nextState = 1;
  // Otherwise
  }else{
    // set state to disabled
    nextState = 0;
  }
}

// ISR for error reset button
void resetButton(){
  waterLevel = adc_read(0);
  // if state == Error and water level is above threshold
  if(state == 3 && waterLevel > WATER_THRESHOLD){
    // go to Idle
    nextState = 1;
  }
}

void displayTempHumidity(){
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Temp: ");
  lcd.print(DHT.temperature);
  lcd.print((char)223);
  lcd.print("C");
  lcd.setCursor(0, 1);
  lcd.print("Humidity: ");
  lcd.print(DHT.humidity);
  lcd.print("%");
}

void adc_init(){
  // setup the A register
  *my_ADCSRA |= 0b10000000; // set bit   7 to 1 to enable the ADC
  *my_ADCSRA &= 0b11011111; // clear bit 6 to 0 to disable the ADC trigger mode
  *my_ADCSRA &= 0b11110111; // clear bit 5 to 0 to disable the ADC interrupt
  *my_ADCSRA &= 0b11111000; // clear bit 0-2 to 0 to set prescaler selection to slow reading
  // setup the B register
  *my_ADCSRB &= 0b11110111; // clear bit 3 to 0 to reset the channel and gain bits
  *my_ADCSRB &= 0b11111000; // clear bit 2-0 to 0 to set free running mode
  // setup the MUX Register
  *my_ADMUX  &= 0b01111111; // clear bit 7 to 0 for AVCC analog reference
  *my_ADMUX  |= 0b01000000; // set bit   6 to 1 for AVCC analog reference
  *my_ADMUX  &= 0b11011111; // clear bit 5 to 0 for right adjust result
  *my_ADMUX  &= 0b11100000; // clear bit 4-0 to 0 to reset the channel and gain bits
}

unsigned int adc_read(unsigned char adc_channel_num){
  // clear the channel selection bits (MUX 4:0)
  *my_ADMUX  &= 0b11100000;
  // clear the channel selection bits (MUX 5)
  *my_ADCSRB &= 0b11110111;
  // set the channel number
  if(adc_channel_num > 7)
  {
    // set the channel selection bits, but remove the most significant bit (bit 3)
    adc_channel_num -= 8;
    // set MUX bit 5
    *my_ADCSRB |= 0b00001000;
  }
  // set the channel selection bits
  *my_ADMUX  += adc_channel_num;
  // set bit 6 of ADCSRA to 1 to start a conversion
  *my_ADCSRA |= 0x40;
  // wait for the conversion to complete
  while((*my_ADCSRA & 0x40) != 0);
  // return the result in the ADC data register
  return *my_ADC_DATA;
}

void my_delay(unsigned int freq)
{
  // calc period
  double period = 1.0/double(freq);
  // 50% duty cycle
  double half_period = period/ 2.0f;
  // clock period def
  double clk_period = 0.0000000625;
  // calc ticks
  unsigned int ticks = half_period / clk_period;
  // stop the timer
  *myTCCR1B &= 0xF8;
  // set the counts
  *myTCNT1 = (unsigned int) (65536 - ticks);
  // start the timer
  * myTCCR1A = 0x0;
  * myTCCR1B |= 0b00000001;
  // wait for overflow
  while((*myTIFR1 & 0x01)==0); // 0b 0000 0000
  // stop the timer
  *myTCCR1B &= 0xF8;   // 0b 0000 0000
  // reset TOV           
  *myTIFR1 |= 0x01;
}
