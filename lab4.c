#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

uint8_t a_current = 0, b_current = 0, a_past = 0, b_past = 0;


/******
for ISR
*******/
#define en1A 0
#define en1B 1
#define en2A 2
#define en2B 3
uint8_t encoder = 0;
uint8_t prevEncoder0 = 1;
uint8_t prevEncoder1 = 1;

/************************
  for brightness of 7-seg
*************************/
#define bright 0xFF		// 00 = off, ff = on

/************************
 for alarm 
*************************/
#define freq 10096		// between 62000 and 10096
#define volume 0x00

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
  if(((encoder & (1<<en1A)) == 0) & (prevEncoder0 == 1)){ // checks for falling edge of
                                                         // encoder1.A 
    if((encoder & (1<<en1A)) != (encoder & (1<<en1B))){  // if encoder1.B is different from encoder1.A
      //increase;                                    // encoder1 is turning clockwise
    }
    else{
      //decrease
    }
  }
  prevEncoder0 = (encoder & (1<<en1A));
}

void segButtonOutputSet(){
  DDRA = 0xFF;                           // segments on/pushbuttons off
  PORTA = 0xFF;                          // set segments to default off
  _delay_ms(1);
}
void segButtonInputSet(){
 PORTF = 0x70;                          // tristate buffer for pushbuttons enabled
 DDRA = 0x00;                           // PORTA set as input
 PORTA = 0xFF;                          // PORTA as pullups
 _delay_ms(1);
}

void segButtonInit(){
  DDRA = 0xFF; 		 		 // segments on/pushbuttons off
  DDRF = 0x70;		 		 // pin 4-7 on portb set to output
  PORTF = 0x00;		 		 // digit 4. one = 0x00 ten = 0x10 
			 		 //hundred = 0x30 thousand = 0x40
}

/******************************
 initialize clock
*******************************/
void tcnt0_init(void){
  ASSR  |=  (1<<AS0);                //run off external 32khz osc (TOSC)
  TIMSK |= (1<<OCIE0);		     //enable interrupts for output compare match 0
  TCCR0 |=  (1<<WGM01) | (1<<CS00);  //CTC mode, no prescale
  OCR0  |=  0x07f;                   //compare at 128
}

/******************************
 initialize dimming 
*******************************/
void tcnt2_init(void){
  // fast PWM, no prescale, inverting mode
  TCCR2 |= (1<<WGM21)|(1<<WGM20)|(1<<CS20)|(1<<COM21)|(1<<COM20);
//    TCCR2 =  (1<<WGM21) | (1<<WGM20) | (1<<COM21) | (1<<COM20) | (1<<CS20) | (1<<CS21);
  OCR2 = bright;		//compare @ 123
}

/******************************
 initialize alarm noise 
*******************************/
void tcnt1_init(void){
  TCCR1A |= (1<<COM1A0);
  TCCR1B |= (1<<WGM12)|(1<<CS10);
  TCCR1C = 0x00;
  TCNT1  = 0;
  OCR1A  = freq;
}
/******************************
 initialize alarm volume 
*******************************/

void tcnt3_init(void){
 //inverting, fast PWM, 64 prescaler
  TCCR3A |= (1<<COM3A1)|(1<<COM3A0)|(1<<WGM31)|(1<<WGM30);
//  TCCR3B |= (1<<WGM33)|(1<<WGM32)|(1<<CS31)|(1<<CS30);
  TCCR3B |= (1<<WGM33)|(1<<WGM32)|(1<<CS30);
  TCCR3C = 0x00;
  TCNT3  = 0;
  OCR3A  = volume;
}



/************************************
 collects the tens and ones place of hour and minutes
 based on count_7ms
************************/
void timeExtract(){
  if((count_7ms %128) == 0){ 		// if one second has passed
//  if((count_7ms %32) == 0){ 		// for debugging. REMOVE on final
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


/******************************************************
 This function displays each digit's value on the 7-seg
*******************************************************/
void segmentDisplay(){
  // this section handles the 7-seg displaying segments
    PORTF &= (0<<PF6)|(0<<PF5)|(0<<PF4);//0x00;		// setting digit position 
    LEDSegment(minOne);					// settings segments based on digit position
    _delay_us(300);					// without delay -> ghosting
    PORTA = 0xFF;			 		// eliminates all ghosting
  
  // displaying tens
    PORTF = (0<<PF6)|(0<<PF5)|(1<<PF4);//0x10;
      LEDSegment(minTen);
    _delay_us(300);					
    PORTA = 0xFF;
  			
  // displaying colon
    PORTF =(0<<PF6)|(1<<PF5)|(0<<PF4);// 0x30;
    if(colon){
      PORTA = 0xFC; 
    }
    else{
      PORTA = 0xFF;
    }
    _delay_us(300);				
    PORTA = 0xFF;			 
  
  //displaying hundreds
    PORTF =(0<<PF6)|(1<<PF5)|(1<<PF4);// 0x30;
//    if(hour <10){
//      PORTA = 0xFF;
//    }
//    else{
      LEDSegment(hOne);
//    }
    _delay_us(300);			
    PORTA = 0xFF;			 
  
    PORTF =(1<<PF6)|(0<<PF5)|(0<<PF4);// 0x40;
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
  DDRB  |=   (0x07)|(1<<PB7)|(1<<PB5);//Turn on SS, MOSI, SCLK, pwm for 7-seg, volume for alarm
  SPCR  |=   (1<<SPE)|(1<<MSTR);//set up SPI mode
  SPSR  |=   (1<<SPI2X);// double speed operation
 }//spi_init

void encoder_init(){
  DDRE = 0xFF;
} 

int main(){
  // initialize
  segButtonInit();					// (must be in, why?)initialize the
							//  external pushButtons and 7-seg
  tcnt3_init();						// alarm volume
  tcnt2_init();						// dimming
  tcnt1_init();						// alarm noise
  tcnt0_init();						// initialize clock
  spi_init();
  encoder_init();
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
          DDRC = 0xFF;
          PORTC = 0x00;
          DDRD = 0x00;
          PORTD = 0xFF;
        segButtonInputSet();
        while(!(debounceSwitch(PINA, 0))){ 		// user confirmation may change to button 7
          // loading encoder data into shift register
          PORTE |= (1<<PE5);             // sets CLK INH high
          PORTE &= ~(1<<PE6);            // toggle SHLD low to high           
          PORTE |= (1<<PE6);             
          //shifting data out to MISO
          PORTE &= ~(1<<PE5);
          SPDR = 0x00;		// garbage data
          while(bit_is_clear(SPSR,SPIF)) {}              // wait till data sent out 
          encoder = SPDR;                                  // collecting input from encoders
          PORTE |= (1<<PE5);                               // setting CLK INH back to high

          if(((encoder & (1<<en1A)) == 0) && (prevEncoder0 == 1)){ // checks for falling edge of
                                                                 // encoder1.A 
            if((encoder & (1<<en1A)) != (encoder & (1<<en1B))){  // if encoder1.B is different from encoder1.A, clockwise
              hour++;
            }
            else{
              hour--;
            }
          }
          prevEncoder0 = (encoder & (1<<en1A));

           a_current = ((encoder>>2) & 0x01);
           b_current = ((encoder>>3) & 0x01);
          
          if(a_past == a_current){
            if((a_current==1) && (b_past < b_current)){
              minute++;
            }
            if((a_current == 1) && (b_past > b_current)){
              minute--;
            }
            if((a_current == 0) && (b_past > b_current)){
              minute++;
            }
            if((a_current == 0) && (b_past < b_current)){
              minute--;
            }
          } 
          a_past = a_current;
          b_past = b_current;

 
                                                                 // encoder1.A 
//          if(((encoder & (1<<en2A)) == 0) && (prevEncoder1 == 1)){ // checks for falling edge of
//                                                                 // encoder1.A 
//            if((encoder & (1<<en2A)) != (encoder & (1<<en2B))){  // if encoder1.B is different from encoder1.A, clockwise
//              hour++;
//            }
//            else{
//              hour--;
//            }
//          }
//          prevEncoder1 = (encoder & (1<<en2A));

          if(minute == 60){
            hour++;
            minute = 0;
          }
          else if (minute == -1){
           hour--;
           minute = 59; 
          }
          if(hour == 24){
              hour = 0;
          }
          else if(hour == -1){
            hour = 23;
          }
          minOne = position0(minute);               	 
          minTen = position1(minute);
          hOne = position0(hour);
          hTen = position1(hour);
          segButtonOutputSet();				// switches from push buttons to display
          segmentDisplay();				// displaying the 7-seg
          segButtonInputSet();
        }
        segButtonOutputSet();
        mode = clk;        
        break;
      }
      case setAlarm:{
      
        break;
      }
    }
    //  checks buttons
    segButtonInputSet();
    if(debounceSwitch(PINA,0)){
      mode = setClk;
    }
    else if(debounceSwitch(PINA,1)){
      mode = setAlarm;
    }
    segButtonOutputSet();
  }//while

}//main
