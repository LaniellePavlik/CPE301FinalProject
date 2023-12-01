/*
Names: John Glenn, Aleena Kureekattil, Lanielle Pavlik
CPE 301 Final Project
*/

#include <dht.h>
#include <LiquidCrystal.h>
#include <RTClib.h>

#define RDA 0x80
#define TBE 0x20

//UART pointers:
volatile unsigned char *myUCSR0A  = (unsigned char *)0x00C0;
volatile unsigned char *myUCSR0B  = (unsigned char *)0x00C1;
volatile unsigned char *myUCSR0C  = (unsigned char *)0x00C2;
volatile unsigned int  *myUBRR0   = (unsigned int *) 0x00C4;
volatile unsigned char *myUDR0    = (unsigned char *)0x00C6;

//pin pointers:

//what's port A?
volatile unsigned char *portA     = (unsigned char *) 0x22;
volatile unsigned char *portDDRA  = (unsigned char *) 0x21;
volatile unsigned char *pinA      = (unsigned char *) 0x20;

//B4-7 = Status LEDs (10-13)
volatile unsigned char *portB     = (unsigned char *) 0x25;
volatile unsigned char *portDDRB  = (unsigned char *) 0x24;

//E3 = LCD Register Select (5), E4-5 = Clock Module (using library)


//G5 LCD Enable (4)


//H3-6 = LCD Data Pins (6-9)

/* States:
0: Disabled
1: Idle
2: Running
3: Error */
int state;

void setup() {
  *portDDRB |= 0b11110000; // set PB4-7 to output (LEDs)
  *portB &= 0b00001111; // set PB4-7 to low 

  *portDDRA &= 0b11111110; // set PA0 to input
  *portB &= 0b11111110; // disable pullup on PA0
  U0Init(9600);
  state = 0;
}

void loop() {
  // put your main code here, to run repeatedly:
  if(state == 0){
    write_pb(7, 1);
    write_pb(6, 0);
    write_pb(5, 0);
    write_pb(4, 0);
  }else if (state == 1){
    write_pb(7, 0);
    write_pb(6, 1);
    write_pb(5, 0);
    write_pb(4, 0);
  }else if(state == 2){
    write_pb(7, 0);
    write_pb(6, 0);
    write_pb(5, 1);
    write_pb(4, 0);
  }else if (state == 3){
    write_pb(7, 0);
    write_pb(6, 0);
    write_pb(5, 0);
    write_pb(4, 1);
  }
  if(*pinA & 0b00000001){
    state++;
    state = state % 4;
  }
  delay(500);
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