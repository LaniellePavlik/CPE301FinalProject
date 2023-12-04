/*
Names: John Glenn, Aleena Kureekattil, Lanielle Pavlik
CPE 301 Final Project
*/

#include <dht.h>
#include <LiquidCrystal.h>
#include <RTClib.h>

#define RDA 0x80
#define TBE 0x20

// Temperature setup
dht DHT;
#define DHT11_PIN 3
#define TEMP_THRESHOLD 25

// Start button pin
#define START_PIN 19

//RTC module
RTC_DS1307 myrtc;

// LCD setup
const int RS=5, EN=4, D4=6, D5=7, D6=8, D7=9;
LiquidCrystal lcd(RS, EN, D4, D5, D6, D7);

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


/* States:
0: Disabled
1: Idle
2: Running
3: Error */
int state, nextState;
char stateNames[4][9] = {"Disabled", "Idle", "Running", "Error"};

void setup() {
  *portDDRB |= 0b11110000; // set PB4-7 to output (LEDs)
  *portB &= 0b00001111; // set PB4-7 to low 

  *portDDRA &= 0b11111110; // set PA0 to input
  *portB &= 0b11111110; // disable pullup on PA0

  *portDDRD &= 0b00000100; // set PD2 to input
  *portD &= 0b11111011; // disable pullup on PD2
  //U0Init(9600);
  Serial.begin(9600);

  // Initialize states
  state = 0;
  nextState = 0;

  // attach interrupt for start button
  attachInterrupt(digitalPinToInterrupt(START_PIN), startButton, RISING);

  //CLOCK
  if (!myrtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    while (1);
  }
  if(! myrtc.isrunning()){
    Serial.println("RTC is NOT running!");
    myrtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}

void loop() {
  // put your main code here, to run repeatedly:
  if(state != nextState){
    reportTransition();
  }
  state = nextState;
  Serial.print("State: ");
  Serial.println(state);
  if(state == 0){
    // Disabled
    // update LEDS
    write_pb(7, 1);
    write_pb(6, 0);
    write_pb(5, 0);
    write_pb(4, 0);
  }else if (state == 1){
    // Idle
    // update LEDS
    write_pb(7, 0);
    write_pb(6, 1);
    write_pb(5, 0);
    write_pb(4, 0);
    // Read temperature
    int chk = DHT.read11(DHT11_PIN);

    // serial library usage for testing
    Serial.print("Temperature = ");
    Serial.println(DHT.temperature);
    Serial.print("Humidity = ");
    Serial.println(DHT.humidity);

    // Test for state transitions
    // if temp > threshold, go to running
    if(DHT.temperature > TEMP_THRESHOLD){
      nextState = 2;
    }
  }else if(state == 2){
    // Running
    // update LEDS
    write_pb(7, 0);
    write_pb(6, 0);
    write_pb(5, 1);
    write_pb(4, 0);
    // Read temperature
    int chk = DHT.read11(DHT11_PIN);

    // serial library usage for testing
    Serial.print("Temperature = ");
    Serial.println(DHT.temperature);
    Serial.print("Humidity = ");
    Serial.println(DHT.humidity);
    // if temp <= threshold, go to idle
    if(DHT.temperature <= TEMP_THRESHOLD){
      nextState = 1;
    }
  }else if (state == 3){
    // Error
    // update LEDS
    write_pb(7, 0);
    write_pb(6, 0);
    write_pb(5, 0);
    write_pb(4, 1);
  }
  /*
  if(*pinA & 0b00000001){
    nextState = state + 1;
    nextState = nextState % 4;
  }
  */
  delay(1000);
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

//RTC to serial monitor
void reportTransition(){
  DateTime now = myrtc.now();
  /*
  char timeStr[8];// 8 characters
  timeStr[0] = 0x30 + (now.hour()/10)%10;
  timeStr[1] = 0x30 + now.hour()%10;
  timeStr[2] = ':';
  timeStr[3] = 0x30 + (now.minute()/10)%10;
  timeStr[4] = 0x30 + now.minute()%10;
  timeStr[5] = ':';
  timeStr[6] = 0x30 + (now.second()/10)%10;
  timeStr[7] = 0x30 + now.second()%10; 
  //sprintf(timeStr, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
  for(int i = 0; i < 8; i++){
    putChar(timeStr[i]);
  }
  putChar('\n');
  */
  Serial.print("Transition from ");
  Serial.print(stateNames[state]);
  Serial.print(" to ");
  Serial.print(stateNames[nextState]);
  Serial.print(" at: ");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.print(now.second(), DEC);
  Serial.println();
}

// ISR for the start button;
void startButton(){
  // if state == Disabled
  if(state == 0){
    // Set state to Idle;
    state = 1;
    nextState = 1;
  }
}


