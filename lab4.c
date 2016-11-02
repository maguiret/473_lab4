/*
 This code is for lab4
 Tim Maguire
*/
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
/*************
for the 7-seg
*************/
volatile int16_t displaying = 0;
int one = 0, ten = 0, hundred = 0, thousand = 0;

/**************************************** 
converts parameter to 7-seg display value
****************************************/
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
  if(x > 9){                   
    PORTA = 0x00;
  }
}

/************************************************************** 
   extracts the ones, tens ... into individual global variables
***************************************************************/
void position(uint16_t x){
  int value = x;
  one = value %10;
  value -= one;
 
  ten = (value %100)/10;
  value -= ten;
 
  hundred = (value %1000)/100;
  value -= hundred;
 
  thousand = (value %10000)/1000;
}

/*****************************
switches PORTA to output mode
*****************************/
void segButtonOutputSet(){
  DDRA = 0xFF;                           // segments on/pushbuttons off
  PORTA = 0xFF;                          // set segments to default off
  _delay_us(300);
}

/***********************************************************************/
//                            spi_init                               
//Initalizes the SPI port on the mega128. Does not do any further   
//external device specific initalizations.  Sets up SPI to be:                        
//master mode, clock=clk/2, cycle half phase, low polarity, MSB first
//interrupts disabled, poll SPIF bit in SPSR to check xmit completion
/***********************************************************************/
//void spi_init(void){
//  DDRB  |=   0x07;//Turn on SS, MOSI, SCLK
//  SPCR  |=   (1<<SPE)|(1<<MSTR);//set up SPI mode
//  SPSR  |=   (1<<SPI2X);// double speed operation
// }//spi_init

/***********************************************************************/
//                              tcnt0_init                             
//Initalizes timer/counter0 (TCNT0). TCNT0 is running in async mode
//with external 32khz crystal.  Runs in normal mode with no prescaling.
//Interrupt occurs at overflow 0xFF.
//
void tcnt0_init(void){
  ASSR   |=  (1<<AS0);		// ext osc TOSC
  TIMSK  |=  (1<<TOIE0);	// enable timer/counter0 overflow interrupt
  TCCR0  |=  (1<<CS00);		// normal mode, no prescale
}

/*************************************************************************/
//                           timer/counter0 ISR                          
//When the TCNT0 overflow interrupt occurs, the count_7ms variable is    
//incremented.  Every 7680 interrupts the minutes counter is incremented.
//tcnt0 interrupts come at 7.8125ms internals.
// 1/32768         = 30.517578uS
//(1/32768)*256    = 7.8125ms
//(1/32768)*256*64 = 500mS
/*************************************************************************/
ISR(TIMER0_OVF_vect){
  static uint8_t count_7ms = 0;        			//holds 7ms tick count in binary
//  static uint8_t display_count = 0x01; 			//holds count for display 

  count_7ms++;                				// increment count every 7.8125 ms 
  if ((count_7ms % 128 == 0)){ 				// ?? interrupts equals one half second 
    displaying++;
//    SPDR = display_count;				// send to display 
//    while(bit_is_clear(SPSR,SPIF)) {}			// wait till data sent out (while spin loop)
//    PORTB |=  0x01;					// strobe HC595 output data reg - rising edge
//    PORTB &=  ~0x01;					// falling edge
//    display_count = (display_count << 1);		// shift display bit for next time 
  }
//  if (display_count == 0x00){display_count=0x01;} 	// back to 1st positon
}

/***********************************************************************/
//                                main                                 
/***********************************************************************/
int main(){     
  tcnt0_init();  				// initalize counter timer zero
  segButtonOutputSet();				// initialized PORTA to output
//  spi_init();    				// initalize SPI port
  sei();         				// enable interrupts before entering loop
  while(1){
    position(displaying);                	// saves one, ten, hundred, thousand of displaying.
    // this section handles the 7-seg displaying segments
    PORTB &= (0<<PB6)|(0<<PB5)|(0<<PB4);//0x00; // setting digit position 
    LEDSegment(one);                            // settings segments based on digit position
    _delay_us(300);                             // without delay -> ghosting
    PORTA = 0xFF;                               // eliminates all ghosting
   
   //same as above step but for digit3
    PORTB = (0<<PB6)|(0<<PB5)|(1<<PB4);//0x10;
    if(displaying <10){
     PORTA = 0xFF;
    }
    else{
     LEDSegment(ten);
    }
    _delay_us(300);
    PORTA = 0xFF;
   
    PORTB =(0<<PB6)|(1<<PB5)|(1<<PB4);// 0x30;
    if(displaying <100){
     PORTA = 0xFF;
    }
    else{
     LEDSegment(hundred);
    }
    _delay_us(300);
    PORTA = 0xFF;
   
    PORTB =(1<<PB6)|(0<<PB5)|(0<<PB4);// 0x40;
    if(displaying <1000){
     PORTA = 0xFF;
    }
    else{
     LEDSegment(thousand);
    }
    _delay_us(300);
    PORTA = 0xFF;
   
 }	     		// empty main while loop

} //main
