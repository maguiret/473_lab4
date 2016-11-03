#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

/*************
for the 7-seg
*************/
volatile int16_t switch_count = 0;
int minOne = 0, minTen = 0, hundred = 0, thousand = 0;
uint8_t colon = 0xFF;

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
 static uint16_t state[8]= {0,0,0,0,0,0,0,0};  					// holds shifted in bits from buttons
 state[button] = (state[button] << 1) | (! var_bit_is_clr(button_var, button)) | 0xE000;
 if (state[button] == 0xF000){
  return 1; 									// if held true for 12 passes through
 }
 else{
  return 0;
 }
} //debounceSwitch

/***************************************
 converts decimal value to 7-seg display
***************************************/
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
/***************************************************************
 saves the ones, tens, hundreds, thousands place of switch_count
***************************************************************/
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

void segButtonOutputSet(){
  DDRA = 0xFF;                           // segments on/pushbuttons off
  PORTA = 0xFF;                          // set segments to default off
  _delay_ms(1);
}

void segButtonInit(){
  DDRA = 0xFF; 		 		 // segments on/pushbuttons off
  DDRB = 0xF0;		 		 // pin 4-7 on portb set to output
  PORTB = 0x00;		 		 // digit 4. one = 0x00 ten = 0x10 
			 		 //hundred = 0x30 thousand = 0x40
}

void tcnt0_init(void){
  ASSR   |=  (1<<AS0);			//ext osc TOSC
  TIMSK  |=  (1<<TOIE0);		//enable timer/counter0 overflow interrupt
  TCCR0  |=  (1<<CS00);			//normal mode, no prescale
}

/*************************************************************************
                           timer/counter0 ISR                          
 When the TCNT0 overflow interrupt occurs, the count_7ms variable is    
 incremented.  Every 7680 interrupts the minutes counter is incremented.
 tcnt0 interrupts come at 7.8125ms internals.
  1/32768         = 30.517578uS
 (1/32768)*256    = 7.8125ms
 (1/32768)*256*128 = 1000mS
*************************************************************************/
ISR(TIMER0_OVF_vect){
  static uint8_t count_7ms = 0;
  count_7ms++;
  if((count_7ms %128) == 0){ 		// if one second has passed
    switch_count++;
    colon ^= 0xFF;
  }
}

int main(){
// initialize
  segButtonInit();			// (must be in, why?)initialize the external pushButtons and 7-seg
  tcnt0_init();				// initialize counter timer zero
  sei();					// enable interrupts before entering loop

  while(1){
  // buttonSense();				// sets switch_count. Based on button press
    position(switch_count);               	// saves one, ten, hundred, thousand of switch_count. 
    segButtonOutputSet();			// switches from push buttons to display
  
  // this section handles the 7-seg displaying segments
    PORTB &= (0<<PB6)|(0<<PB5)|(0<<PB4);//0x00;	// setting digit position 
    LEDSegment(one);				// settings segments based on digit position
    _delay_us(300);					// without delay -> ghosting
    PORTA = 0xFF;			 		// eliminates all ghosting
  
  // displaying tens
    PORTB = (0<<PB6)|(0<<PB5)|(1<<PB4);//0x10;
    if(switch_count <10){
      PORTA = 0xFF;
    }
    else{
      LEDSegment(ten);
    }
    _delay_us(300);					// without delay -> ghosting
    PORTA = 0xFF;
  			
  // displaying colon
    PORTB =(0<<PB6)|(1<<PB5)|(0<<PB4);// 0x30;
    if(colon){
      PORTA = 0xFC; 
    }
    else{
      PORTA = 0xFF;
    }
    _delay_us(300);					// without delay -> ghosting
    PORTA = 0xFF;			 
  
  //displaying hundreds
    PORTB =(0<<PB6)|(1<<PB5)|(1<<PB4);// 0x30;
    if(switch_count <100){
      PORTA = 0xFF;
    }
    else{
      LEDSegment(hundred);
    }
    _delay_us(300);					// without delay -> ghosting
    PORTA = 0xFF;			 
  
    PORTB =(1<<PB6)|(0<<PB5)|(0<<PB4);// 0x40;
    if(switch_count <1000){
      PORTA = 0xFF;
    }
    else{
      LEDSegment(thousand);
    }
    _delay_us(300);					// without delay -> ghosting
    PORTA = 0xFF;			 	
  
  }//while

}//main
