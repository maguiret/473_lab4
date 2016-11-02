#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

/*
for ISR
*/
#define en1A 0
#define en1B 1
uint8_t mode = 1; 		// holds value being incremented/decremented
uint8_t encoder = 0;
uint8_t prevEncoder = 1;
uint8_t encoderDir = 0;

/*
for the 7-seg
*/
volatile int16_t switch_count = 0;
int one = 0, ten = 0, hundred = 0, thousand = 0;

/************************************************************************
Just like bit_is_clear() but made it check a variable
************************************************************************/

uint8_t var_bit_is_clr(uint8_t test_var, uint8_t bit) {
 if((test_var & (1 << bit))  == 0){
  return  1;//TRUE;
 }
 else{
  return 0;//FALSE;
 }
}//var_bit_is_clr

/************************************************************************
Checks the state of the button number passed to it. It shifts in ones till
the button is pushed. Function returns a 1 only once per debounced button
push so a debounce and toggle function can be implemented at the same time.
Adapted to check all buttons from Ganssel's "Guide to Debouncing"
Expects active low pushbuttons on a port variable.  Debounce time is
determined by external loop delay times 12.
Note: that this function is checking a variable, not a port.
************************************************************************/

uint8_t debounceSwitch(uint8_t button_var, uint8_t button) {
 static uint16_t state[8]= {0,0,0,0,0,0,0,0};  //holds shifted in bits from buttons
 state[button] = (state[button] << 1) | (! var_bit_is_clr(button_var, button)) | 0xE000;
 if (state[button] == 0xF000){
  return 1; //if held true for 12 passes through
 }
 else{
  return 0;
 }
} //debounceSwitch


/***********************************************************
//senes specific button press and increments switch_count accordlying
***********************************************************/
void buttonSense(){
 if(((encoder & (1<<en1A)) == 0) & (prevEncoder == 1)){ // checks for falling edge of
							// encoder1.A 
  if((encoder & (1<<en1A)) != (encoder & (1<<en1B))){   // if encoder1.B is different from encoder1.A
   encoderDir = 1;                                      // encoder1 is turning clockwise
  }
  else{
   encoderDir = 0;
  }
// changing switch_count based on encoder direction
  if(encoderDir == 1){
   switch_count = switch_count + (mode);
  }
  if(encoderDir == 0){
   switch_count = switch_count - (mode);
  }
 }
  if(switch_count < 0){
   switch_count = 1023;
   return; 
  }
  else if(switch_count > 1023){
   switch_count = 0;
   return;
  }
 prevEncoder = (encoder & (1<<en1A));
}
// converts decimal value to 7-seg display
void LEDSegment(int x){ 
 if(x == 0){
 PORTA = 0xC0;
 }
 if(x == 1){
  PORTA = 0xF9; 
 } 
 if(x == 2){
  PORTA = 0xA4; 
 } 
 if(x == 3){
  PORTA = 0xB0; 
 } 
 if(x == 4){
  PORTA = 0x99; 
 } 
 if(x == 5){
  PORTA = 0x92; 
 } 
 if(x == 6){
  PORTA = 0x82; 
 } 
 if(x == 7){
  PORTA = 0xF8; 
 } 
 if(x == 8){
  PORTA = 0x80; 
 }
 if(x == 9){
  PORTA = 0x98;
 }
 if(x > 9){    			//error
  PORTA = 0x00;
 }
}
// saves the ones, tens, hundreds, thousands place of switch_count
position(uint16_t x){
 int value = x;
 one = value %10;
 value -= one;
 
 ten = (value %100)/10;
 value -= ten;
 
 hundred = (value %1000)/100;
 value -= hundred;

 thousand = (value %10000)/1000;
 
}
/* turns off the 7-seg and flips PORTB from output to input
*/
void segButtonInputSet(){
 PORTB = 0x70;                          // tristate buffer for pushbuttons enabled
 DDRA = 0x00;                           // PORTA set as input
 PORTA = 0xFF;                          // PORTA as pullups
 _delay_ms(1);
}

void segButtonOutputSet(){
 DDRA = 0xFF;                           // segments on/pushbuttons off
 PORTA = 0xFF;                          // set segments to default off
 _delay_ms(1);
}

void segButtonInit(){
 DDRA = 0xFF; 				// segments on/pushbuttons off
 DDRB = 0xF0;				// pin 4-7 on portb set to output
 PORTB = 0x00;				// digit 4. one = 0x00 ten = 0x10 
					//hundred = 0x30 thousand = 0x40
}
void spi_init(void){
  DDRB  |=   0x07;//Turn on SS, MOSI, SCLK
  SPCR  |=   (1<<SPE)|(1<<MSTR);//set up SPI mode
  SPSR  |=   (1<<SPI2X);// double speed operation
 }//spi_init

void tcnt0_init(void){
  //ASSR   |=  (1<<AS0);//ext osc TOSC
  TIMSK  |=  (1<<TOIE0);//enable timer/counter0 overflow interrupt
  TCCR0  |=  (1<<CS02)|(1<<CS00);//normal mode, prescale by 128
}

void encoder_init(){
 DDRE = 0xFF;			// Set PORTE as output
// PORTE = (0<<PE7);		// SETTING sin TO 0
 
}

ISR(TIMER0_OVF_vect){
 segButtonInputSet();			// change PORTA to read buttons
  if(debounceSwitch(PINA, 0)){
   if(mode == 2){			// if you're already incrementing by 2
    mode = 1;				// change to increment by 1
   }
    else if(mode == 4){		// if you're incrementing by 4
     mode = 0;			// change to increment by 0 
    }
   else{ 
    mode = 2;
   }
  }
  /* the code below changes mode when PINA.1 is pressed
  */
  else if(debounceSwitch(PINA,1)){
   if(mode == 4){
    mode = 1;
   }
   else if(mode == 2){
     mode = 0;	
   }
   else{
    mode = 4;
   }
  }
/****************************
get the encoder data ready
****************************/
// loading encoder data into shift register
 PORTE |= (1<<PE5);		// sets CLK INH high
 PORTE &= ~(1<<PE6);		// toggle low to high		
 PORTE |= (1<<PE6);		// Strobing SHLD
//shifting data out to MISO
 PORTE &= ~(1<<PE5);

/*
Translates from buttonState to LEDs being displayed (mode
*/
 if(mode == 0){
  SPDR = 0x00;
 }
 else if(mode == 1){
  SPDR = 0x01;
 }
 else if(mode == 2){
  SPDR = 0x03;
 }
 else{
  SPDR = 0x0F;		// displays 4 LEDs
 }
 
  
 while(bit_is_clear(SPSR,SPIF)) {}		// wait till data sent out (while spin loop)
 encoder = SPDR;				// collecting input from encoders
 PORTB |=  0x01;				// strobe HC595 output data reg - rising edge
 PORTB &=  ~0x01;				// falling edge
 PORTE |= (1<<PE5);				// setting CLK INH back to high

}

int main(){
// initialize
 segButtonInit();			// initialize the external pushButtons and 7-seg
 tcnt0_init();				// initialize counter timer zero
 spi_init();				// initialize SPI port
 encoder_init();
 sei();					// enable interrupts before entering loop

 while(1){
 segButtonInputSet();
 buttonSense();				// sets switch_count. Based on button press
 position(switch_count);               	// saves one, ten, hundred, thousand of switch_count. 
 segButtonOutputSet();			// switches from push buttons to display


// this section handles the 7-seg displaying segments
 PORTB &= (0<<PB6)|(0<<PB5)|(0<<PB4);//0x00;	// setting digit position 
 LEDSegment(one);				// settings segments based on digit position
 _delay_ms(1);					// without delay -> ghosting
 PORTA = 0xFF;			 		// eliminates all ghosting

//same as above step but for digit3
 PORTB = (0<<PB6)|(0<<PB5)|(1<<PB4);//0x10;
 if(switch_count <10){
  PORTA = 0xFF;
 }
 else{
  LEDSegment(ten);
 }
 _delay_ms(1);
 PORTA = 0xFF;			

 PORTB =(0<<PB6)|(1<<PB5)|(1<<PB4);// 0x30;
 if(switch_count <100){
  PORTA = 0xFF;
 }
 else{
  LEDSegment(hundred);
 }
 _delay_ms(1);
 PORTA = 0xFF;			 

 PORTB =(1<<PB6)|(0<<PB5)|(0<<PB4);// 0x40;
 if(switch_count <1000){
  PORTA = 0xFF;
 }
 else{
  LEDSegment(thousand);
 }
 _delay_ms(1); 
 PORTA = 0xFF;			 	
  

}//while


}//main
