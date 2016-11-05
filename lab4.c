#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

/******
for ISR
*******/
#define en1A 0
#define en1B 1
//uint8_t mode = 1;               // holds value being incremented/decremented
uint8_t encoder = 0;
uint8_t prevEncoder = 1;
uint8_t encoderDir = 0;

/********************************
Modes the Alarm Clock will be in 
********************************/
enum modes{clk, setClk, setAlarm};

/*************
global variables
***************/
uint8_t count_7ms = 0;

/*************
for the 7-seg
*************/
volatile int16_t switch_count = 0;
int minute = 0, hour = 0, minOne = 0, minTen = 0, hOne = 0, hTen = 0;
uint8_t colon = 0xFF, one = 0, ten = 0;

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
 if(x > 9){    			// error indicator
  PORTA = 0x00;
 }
}
/***************************************************************
 saves the ones, tens, hundreds, thousands place of switch_count
***************************************************************/
uint8_t position0(uint16_t x){
  int value = x;
  one = value %10;
  return one;
//  value -= one;
//  
//  ten = (value %100)/10;
//  value -= ten;
//  
//  hundred = (value %1000)/100;
//  value -= hundred;
// 
//  thousand = (value %10000)/1000;
}
uint8_t position1(uint16_t x){
  int value = x;
  one = value %10;
  value -= one;
  
  ten = (value %100)/10;
  return ten;
}
/***************************************************
 checks encoders and changes hour/minute accordinly
****************************************************/
void buttonSense(){
  if(((encoder & (1<<en1A)) == 0) & (prevEncoder == 1)){ // checks for falling edge of
                                                         // encoder1.A 
    if((encoder & (1<<en1A)) != (encoder & (1<<en1B))){  // if encoder1.B is different from encoder1.A
      encoderDir = 1;                                    // encoder1 is turning clockwise
    }
    else{
      encoderDir = 0;
    }
// changing switch_count based on encoder direction
    if(encoderDir == 1){
      minute += 1;
    }
    else{
      minute -= 1;
    }
  }
  prevEncoder = (encoder & (1<<en1A));
}

void segButtonOutputSet(){
  DDRA = 0xFF;                           // segments on/pushbuttons off
  PORTA = 0xFF;                          // set segments to default off
  _delay_ms(1);
}
void segButtonInputSet(){
 PORTB = 0x70;                          // tristate buffer for pushbuttons enabled
 DDRA = 0x00;                           // PORTA set as input
 PORTA = 0xFF;                          // PORTA as pullups
 _delay_ms(1);
}

void segButtonInit(){
  DDRA = 0xFF; 		 		 // segments on/pushbuttons off
  DDRB = 0xF0;		 		 // pin 4-7 on portb set to output
  PORTB = 0x00;		 		 // digit 4. one = 0x00 ten = 0x10 
			 		 //hundred = 0x30 thousand = 0x40
}

void tcnt0_init(void){
  ASSR  |=  (1<<AS0);                //run off external 32khz osc (TOSC)
  //enable interrupts for output compare match 0
  TIMSK |= (1<<OCIE0);
  TCCR0 |=  (1<<WGM01) | (1<<CS00);  //CTC mode, no prescale
  OCR0  |=  0x07f;                   //compare at 128
}

void tcnt3_init(void){
  ASSR  |=  (1<<AS0);                //run off external 32khz osc (TOSC)
  //enable interrupts for output compare match 0
  TIMSK |= (1<<OCIE0);
  TCCR0 |=  (1<<WGM01) | (1<<CS00);  //CTC mode, no prescale
  OCR0  |=  0x07f;                   //compare at 128
}

/*************************************************************************
                           timer/counter0 ISR                          
 When the TCNT0 compare interrupt occurs, the count_7ms variable is    
 incremented. 
  1/32768         = 30.517578uS
 (1/32768)*128    = 3.90625ms
 (1/32768)*256*128 = 1000mS
*************************************************************************/
ISR(TIMER0_COMP_vect){
  count_7ms++;
  // determing time positions
  timeExtract();
}

/************************************
 collects the tens and ones place of hour and minutes
 based on count_7ms
************************/
void timeExtract(){
//  if((count_7ms %128) == 0){ 		// if one second has passed
  if((count_7ms %32) == 0){ 		// for debugging. REMOVE on final
    switch_count++;
    colon ^= 0xFF;			// toggling the colon every second
  }
  if(switch_count == 60){		// if 60 seconds have passed
    minute++;
    switch_count = 0;
    if(minute == 60){
      hour++;
      minute = 0;
      if(hour == 24){
        hour = 0;
      }
    }
  }
  
}

/******************************************************
 This function displays each digit's value on the 7-seg
*******************************************************/
segmentDisplay(){
  // this section handles the 7-seg displaying segments
    PORTB &= (0<<PB6)|(0<<PB5)|(0<<PB4);//0x00;		// setting digit position 
    LEDSegment(minOne);					// settings segments based on digit position
    _delay_us(300);					// without delay -> ghosting
    PORTA = 0xFF;			 		// eliminates all ghosting
  
  // displaying tens
    PORTB = (0<<PB6)|(0<<PB5)|(1<<PB4);//0x10;
      LEDSegment(minTen);
    _delay_us(300);					
    PORTA = 0xFF;
  			
  // displaying colon
    PORTB =(0<<PB6)|(1<<PB5)|(0<<PB4);// 0x30;
    if(colon){
      PORTA = 0xFC; 
    }
    else{
      PORTA = 0xFF;
    }
    _delay_us(300);				
    PORTA = 0xFF;			 
  
  //displaying hundreds
    PORTB =(0<<PB6)|(1<<PB5)|(1<<PB4);// 0x30;
//    if(hour <10){
//      PORTA = 0xFF;
//    }
//    else{
      LEDSegment(hOne);
//    }
    _delay_us(300);			
    PORTA = 0xFF;			 
  
    PORTB =(1<<PB6)|(0<<PB5)|(0<<PB4);// 0x40;
    if(hour<10){
      PORTA = 0xFF;
    }
    else{
      LEDSegment(hTen);
    }
    _delay_us(300);		
    PORTA = 0xFF;			 	
}

/*******************************
Initialize Spi
********************************/
void spi_init(void){
  DDRB  |=   0x07;//Turn on SS, MOSI, SCLK
  SPCR  |=   (1<<SPE)|(1<<MSTR);//set up SPI mode
  SPSR  |=   (1<<SPI2X);// double speed operation
 }//spi_init
 

int main(){
  // initialize
  segButtonInit();					// (must be in, why?)initialize the
							//  external pushButtons and 7-seg
  spi_init();
  tcnt0_init();						// initialize counter timer zero
  sei();						// enable interrupts before entering loop
  //set default mode
  enum modes mode = clk;  

  while(1){
    // implements mode
    switch(mode){
      case clk:{
        segButtonOutputSet();
        // saving the hour and minute digits
        minOne = position0(minute);               	 
        minTen = position1(minute);
        hOne = position0(hour);
        hTen = position1(hour);
        segButtonOutputSet();				// switches from push buttons to display
        segmentDisplay();				// displaying the 7-seg
      
        break;
      }
      case setClk:{
        segButtonInputSet();
        while(!(debounceSwitch(PINA, 0))){
//          // loading encoder data into shift register
//          PORTE |= (1<<PE5);             // sets CLK INH high
//          PORTE &= ~(1<<PE6);            // toggle SHLD low to high           
//          PORTE |= (1<<PE6);             
//          //shifting data out to MISO
//          PORTE &= ~(1<<PE5);
//          SPDR = 0x00;		// garbage data
//          while(bit_is_clear(SPSR,SPIF)) {}              // wait till data sent out 
//          encoder = SPDR;                                  // collecting input from encoders
//          PORTE |= (1<<PE5);                               // setting CLK INH back to high
//          if(debounceSwitch(encoder, 0)){
//            buttonSense(); 
//            timeExtract(); 
//          }
        }
        segButtonOutputSet();
        mode = clk;        
        break;
      }
//      case setAlarm:{
//      
//        break;
//      }
    }
    //  checks buttons
    segButtonInputSet();
    if(debounceSwitch(PINA,0)){
      mode = setClk;
    }
    segButtonOutputSet();
  }//while

}//main
