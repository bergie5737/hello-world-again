/* Field tech version of recording values
/*  The number of samples required will be calculated based on the sample frequency
/*  and the sample period. By a compression technique 4 samples require 5 bytes.
/*  (A normal sample will require 10bits, or two bytes per sample.) Thus the max number
/*  of samples is calculated by EEPROM_size / 5. { 512 / 5} = 102 samples with two spare bytes.
/*  The max_time is then determined by the sample frequency. max_samples * READ_INTERVAL * 4
/*  {102 * 10sec * 4 = 68 minutes}. Thus the sampling frequency can be increased or decreased
/*  based on how long you need to test for.
/*  For fixed read intervals, more or less EEPROM can be used.
/*  

*/

#include <EEPROM.h>

#include "DigiKeyboard.h"

#define ANALOG_PIN 1
#define LED_PIN 0
#define READ_INTERVAL 500L //10000L     //LOG_TIME / READ_INTERVAL my not exceed 102
#define LOG_TIME 25000L                 //Time in ms to log voltage curve
#define ANALOG_READS 10                 //Number of samples to average out
#define EEPROM_SIZE 512
#define ALL_DONE_BYTE 511           //Store the sample counter on this byte in EEPROM - unused

//calculate number of samples to be taken. Add and extra interval to be safe with rounding off
//Ensure that LOG_TIME / READ_INTERVAL + <= 102
#define SAMPLES 10//(LOG_TIME / READ_INTERVAL + 1)

unsigned long last_time = 0;
uint8_t ADC_high = 0;               //hold value of high byte
uint8_t hi_byte_count = 0;          //keep track of which hi-byte bits to be written
uint16_t EEPROM_count = 0;          //track EEPROM bytes
uint16_t sample_count = 0;
uint32_t led_timer = 0;             //used to time led flashes
uint16_t flash_interval = 250;      //the time the LED will flash on and off
uint8_t usb_present = 0;            //check if usb is plugged in or not. USB present will pull pin 2 high.

void setup() {

	// if the eeprom has all its values from a previous read, print eeprom
	//ALL_DONE_BYTE is written as 0x0F when all samples were collected 
	if(EEPROM.read(ALL_DONE_BYTE) == 0x0F && digitalRead(1) == HIGH)    //read extra pin to determine if running of Vbat or usb
	{
		// usb_present = 0xFF;
		// DigiKeyboard.sendKeyStroke(0);
		dummy_keystroke();
		led_timer = millis();
		// while (millis() < led_timer + 5000){}       //a simple delay function to get usb to sync
		 display_EEPROM();
		 // // DigiKeyboard.println("Done");
		 clear_eeprom();
		 end_code();
	} 
	//  show_EEPROM();

	//  DigiKeyboard.println("Done");
	//  DigiKeyboard.delay(2000);
	randomSeed(101);
	last_time = millis();
	pinMode(LED_PIN, OUTPUT);
	led_timer = last_time;
	digitalWrite(LED_PIN,HIGH);
	if (digitalRead(1) == HIGH)     //read extra pin to determine if running of Vbat or usb
	{
		usb_present = 0x0F;
		dummy_keystroke();
	}
	
}


void loop() {
	
	//check if pin 1 (physical pin 6) is high. Then it will display the eeprom and end the code
	//Pin 1 is a pushbutton connected to Vcc
	// if(digitalRead(1) == HIGH)
	// {
		// display_EEPROM();
		// clear_eeprom();
		// end_code();
	// }
	DigiKeyboard.update();
	//This will time and read voltages at a set interval
	if(millis() > last_time + READ_INTERVAL)    
	{
		// DigiKeyboard.update();
		last_time = millis();                               //restart second timer
		uint16_t voltage = get_voltage(ANALOG_READS);       //get voltage
		if(usb_present)
		{
			
			print_now(voltage);                                 //print to screen if plugged in
		}
		
		write_EEPROM(voltage);                              //write to EEPROM
		sample_count++;                                     //increment samples taken
		// EEPROM.write(SAMPLE_COUNT_BYTE, sample_count);      //write the counter to EEPROM - unused
	}
	flash_led(flash_interval);


	
if(sample_count >= SAMPLES)
	{
		// DigiKeyboard.println();
		
		//prevent incomplete hi bytes from being written. The last block will be finished written
		//although voltages will be zero's
		while (hi_byte_count != 0)
		{
			write_EEPROM(0);
		}
		// DigiKeyboard.println();
		// display_EEPROM();
		//    show_EEPROM();
		// DigiKeyboard.println("Done");
		// flash_interval /= 4;
		EEPROM.write(ALL_DONE_BYTE, 0x0F);      //write to all done byte to show al samples were collected
		end_code();
	}
}

/*  TODO: 
	Convert eeprom values to actual voltage levels
	Add output indicators - LEDs?
	Add fucntionality to check direct print to USB or log to EEPROM
	Fix required. Unable to run code when stand alone.... >> Must check of usb is present before attempting to write to USB
	Cop-out code to display line that say eeprom is empty

 /*  Build and write values to EEPROM
 The low bytes of the ADC is always written to EEPROM with increasing count.
  The high byte value consist of only two bits. To fill up a byte, the two bits get
  shifted up, until a full byte is build.
  Once there is four bit pairs of the ADC high byte available (a full byte), this 
  value gets written to EEPROM.
  Thus five EEPROM bytes will have the following structure:
  4 x ADC_low:: 1 x (4 x bit pairs of ADC_hi)
*/

void write_EEPROM(uint16_t voltage)
{
   // print_now(voltage);
  uint8_t ADC_low = 0x00FF & voltage;                 //extract low byte
  uint8_t ADC_high_temp = 0x03 & (voltage >> 8);      //extract high byte - learn to do this in one line
  ADC_high = (ADC_high << 2) | ADC_high_temp;

  EEPROM.write(EEPROM_count, ADC_low);                //write to EEPROM

  //write hi bytes to EEPROM when there are four of them
  if(hi_byte_count >= 3)
  {
    hi_byte_count = 0;                              //reset the counter
    EEPROM_count++;                                 //write the byte to the next available eeprom space
    EEPROM.write(EEPROM_count, ADC_high);
    ADC_high = 0;
  }
  //update ADC high byte as required
  else
  {
    hi_byte_count++;
  }
  EEPROM_count++;

}

/*  This will extract values from the EEPROM

*/
void display_EEPROM()
{
  DigiKeyboard.sendKeyStroke(0);
  uint16_t voltage = 0;           //hold temp voltage value
  uint8_t  mask = 0x03;           //holds the mask to extract hi byte pairs
  uint16_t hi_byte_position = 4;  //keep track of where the high bytes is stored
  uint8_t hi_bytes[4];            //array to keep hi byte pairs that is extracted
  int8_t sample_count = SAMPLES;  //set up a counter to extract the correct number of samples
  uint16_t byte_number = 0;       //Keep track of which EEPROM byte is worked on

  while (sample_count > 0)
  {

    uint8_t working_hi_byte = EEPROM.read(hi_byte_position);    //byte to shift bits around with
    
    //extracting hi-byte pairs to seperate variables in array
    //Do in reverse, for easy usage
    for(int8_t j = 3; j >= 0; j--)
    {
      hi_bytes[j] =  working_hi_byte & mask;
      working_hi_byte = working_hi_byte >> 2;
    }
    
    //run inner loop again to add hi_byte
    for(int8_t j = 0; j <= 3; j++)
    {
      voltage = hi_bytes[j] << 8;
      voltage |= EEPROM.read(byte_number);    //read lo byte
      print_now(voltage);
      byte_number++;                  //next byte to get from EEPROM
      sample_count--;                 //decrement number of samples for every write-out
    }
    hi_byte_position += 5;              //this is where the next hi byte pair is saved in EEPROM
    byte_number++;                      //skip one EEPROM byte because its a hi byte pair

  }

}


//get value and average out, reads should not exceed 100 to fit into a single integer
uint16_t get_voltage (uint8_t reads)
{
  //int V_now = analogRead(ANALOG_PIN);
  int V_now = 0;  
  for (byte i = 1; i < reads; i++)
  { 
    //V_now += analogRead(ANALOG_PIN);
    V_now += simulate_v();
  }
  //calculate average
  V_now /= reads;

  return V_now;  
  //  return simulate_v();
}


//Controls the print out line. Keeps memory usage low to repeatedly call same function 
void print_now(unsigned int valueP)
{
  DigiKeyboard.print(valueP,DEC);         //print the value
  DigiKeyboard.print(",");                //print seperator
}

void flash_led(uint16_t flash_rate)
{
  if(millis() > led_timer + flash_rate)
  {
    PINB |= (1 << PB0);        //toggles the pin by writing a high to it
    led_timer = millis();
  }
}


//Led flash pattern that indicate the program is finished
void end_code()
{
  while(1)
    {
      flash_led(flash_interval / 4);
    }
  
}

void dummy_keystroke()
{
    DigiKeyboard.sendKeyStroke(0);
    while (millis() < led_timer + 5000){}       //a simple delay function to get usb to sync
}


/*  Page used to keep debug and testing code. 
  To be excluded from final build
*/
int simulate_v()
{

  return random(0,1023);

}

void clear_eeprom()
{
  for (int i = 0; i < 512; i++)
  {
    EEPROM.write(i,0xFF);           //0xFF is "charged EEPROM, thus less strain on write/read cycles
  }
}

void show_EEPROM()
{
  for(int i = 0; i < EEPROM_SIZE; i++)
  print_now(EEPROM.read(i));
  while(1);
}
