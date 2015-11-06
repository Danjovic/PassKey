/***
*        __ __              ____                 
*       / //_/__  __  __   / __ \____ ___________
*      / ,< / _ \/ / / /  / /_/ / __ `/ ___/ ___/
*     / /| /  __/ /_/ /  / ____/ /_/ (__  |__  ) 
*    /_/ |_\___/\__, /  /_/    \__,_/____/____/  
*              /____/                            
*
* Keychain USB multiple password manager/generator/injector
* Author: Danie Jose Viana, danjovic@hotmail.com
*
* Based on "DIY USB password generator" 
* by  Joonas Pihlajamaa http://codeandlife.com/
* License: GNU GPL v3 (see License.txt)
* 
* Version .6 - november 4, 2015
* Basic Functionality
* 
*/


#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>
#include <util/delay.h>

#include "usbdrv.h"
#include "7segments.h"

// *********************************
// *** BASIC PROGRAM DEFINITIONS ***
// *********************************

#define MAX_PASSWORD_SLOTS 10  // maximum number of passwords
#define PASS_LENGTH 12 // password length for generated password
#define SEND_ENTER 0 // define to 1 if you want to send ENTER after password

//const PROGMEM uchar measuring_message[] = "Tap CAPS LOCK...";  
//const PROGMEM uchar finish_message[] = " New password saved.";

// The buffer needs to accommodate the messages above and the password
#define MSG_BUFFER_SIZE 14 // MAX_PASSWORD_SLOTS + 2 

EEMEM uchar stored_password[MAX_PASSWORD_SLOTS][MSG_BUFFER_SIZE];


// ************************
// *** USB HID ROUTINES ***
// ************************

// From Frank Zhao's USB Business Card project
// http://www.frank-zhao.com/cache/usbbusinesscard_details.php
const PROGMEM char usbHidReportDescriptor[USB_CFG_HID_REPORT_DESCRIPTOR_LENGTH] = {
	0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
	0x09, 0x06,                    // USAGE (Keyboard)
	0xa1, 0x01,                    // COLLECTION (Application)
	0x75, 0x01,                    //   REPORT_SIZE (1)
	0x95, 0x08,                    //   REPORT_COUNT (8)
	0x05, 0x07,                    //   USAGE_PAGE (Keyboard)(Key Codes)
	0x19, 0xe0,                    //   USAGE_MINIMUM (Keyboard LeftControl)(224)
	0x29, 0xe7,                    //   USAGE_MAXIMUM (Keyboard Right GUI)(231)
	0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
	0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
	0x81, 0x02,                    //   INPUT (Data,Var,Abs) ; Modifier byte
	0x95, 0x01,                    //   REPORT_COUNT (1)
	0x75, 0x08,                    //   REPORT_SIZE (8)
	0x81, 0x03,                    //   INPUT (Cnst,Var,Abs) ; Reserved byte
	0x95, 0x05,                    //   REPORT_COUNT (5)
	0x75, 0x01,                    //   REPORT_SIZE (1)
	0x05, 0x08,                    //   USAGE_PAGE (LEDs)
	0x19, 0x01,                    //   USAGE_MINIMUM (Num Lock)
	0x29, 0x05,                    //   USAGE_MAXIMUM (Kana)
	0x91, 0x02,                    //   OUTPUT (Data,Var,Abs) ; LED report
	0x95, 0x01,                    //   REPORT_COUNT (1)
	0x75, 0x03,                    //   REPORT_SIZE (3)
	0x91, 0x03,                    //   OUTPUT (Cnst,Var,Abs) ; LED report padding
	0x95, 0x06,                    //   REPORT_COUNT (6)
	0x75, 0x08,                    //   REPORT_SIZE (8)
	0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
	0x25, 0x65,                    //   LOGICAL_MAXIMUM (101)
	0x05, 0x07,                    //   USAGE_PAGE (Keyboard)(Key Codes)
	0x19, 0x00,                    //   USAGE_MINIMUM (Reserved (no event indicated))(0)
	0x29, 0x65,                    //   USAGE_MAXIMUM (Keyboard Application)(101)
	0x81, 0x00,                    //   INPUT (Data,Ary,Abs)
	0xc0                           // END_COLLECTION
};

typedef struct {
	uint8_t modifier;
	uint8_t reserved;
	uint8_t keycode[6];
} keyboard_report_t;

static keyboard_report_t keyboard_report; // sent to PC
volatile static uchar LED_state = 0xff; // received from PC
static uchar idleRate; // repeat rate for keyboards



#define STATE_SEND 1
#define STATE_DONE 0

static uchar messageState = STATE_DONE;
static uchar messageBuffer[MSG_BUFFER_SIZE] = "";
static uchar messagePtr = 0;
static uchar messageCharNext = 1;

static uchar slot=0;
static uchar display_state=0;   // keep track of the segments lit (plus decimal point)

static uchar usage_flags=0x03;  // Bit 2-> Use Scroll Lock, Bit 1-> Use Caps Lock, Bit 0->Use Num Lock
enum {
	_no_press = 0,
	_short_press,    
	_long_press,
} button_state;



///////////////////////////////////////////////////////////////////////////////////////////////////
// Debouncing Routine. This routine uses timer2 as a time base, counting up
// when the bit 5 of the timer toggles and the button stays pressed. 
// 1 tick at each 1024/16Mhz=64us -> 64us/32=2ms.  255 ticks=510ms

#define B1_Pressed ((PINB & (1<<1))==0) // SELECT
#define B2_Pressed ((PIND & (1<<1))==0) // SEND
#define treshold_short_press 50  
#define treshold_long_press  250  
uint8_t debounce_B1 (void) {

	static uint8_t last_timer_toggle=0;
	static uint8_t time_button_pressed=0;
	uint8_t temp;

	if (B1_Pressed) { // count how many cycles button remained press
		temp=TCNT2 & (1<<5); // 
		if ( (temp!=last_timer_toggle) & (time_button_pressed <255) ) {// advance and saturate VX7333b6450X
			time_button_pressed++; 
			last_timer_toggle=temp;
		}  
		return _no_press;     
	} else {  // button is released  
		temp=time_button_pressed;
		time_button_pressed=0;
		if (temp < treshold_short_press) return _no_press;     // return no press when time below noise treshold 
		if (temp < treshold_long_press)  return _short_press;  // return short press when time above noise treshold but below long press treshold
		else return _long_press ;                           // return long press wuen time is above long press treshold 
	}
	return _no_press; // redundant  
}

uint8_t debounce_B2 (void) {

	static uint8_t last_timer_toggle=0;
	static uint8_t time_button_pressed=0;
	uint8_t temp;

	if (B2_Pressed) { // count how many cycles button remained press
		temp=TCNT2 & (1<<5); // 
		if ( (temp!=last_timer_toggle) & (time_button_pressed <255) ) {// advance and saturate VX7333b6450X
			time_button_pressed++; 
			last_timer_toggle=temp;
		}  
		return _no_press;     
	} else {  // button is released
		temp=time_button_pressed;
		time_button_pressed=0;
		if (temp < treshold_short_press) return _no_press;     // return no press when time below noise treshold 
		if (temp < treshold_long_press)  return _short_press;  // return short press when time above noise treshold but below long press treshold
		else return _long_press ;                           // return long press wuen time is above long press treshold 
	}
	return _no_press; // redundant  
}

#define MOD_SHIFT_LEFT (1<<1)



// The buildReport is called by main loop and it starts transmitting
// characters when messageState == STATE_SEND. The message is stored
// in messageBuffer and messagePtr tells the next character to send.
// Remember to reset messagePtr to 0 after populating the buffer!
uchar buildReport() {
	uchar ch;
	
	if(messageState == STATE_DONE || messagePtr >= sizeof(messageBuffer) || messageBuffer[messagePtr] == 0) {
		keyboard_report.modifier = 0;
		keyboard_report.keycode[0] = 0;
		return STATE_DONE;
	}

	if(messageCharNext) { // send a keypress
		ch = messageBuffer[messagePtr++];
		
		// convert character to modifier + keycode
		if(ch >= '0' && ch <= '9') {
			keyboard_report.modifier = 0;
			keyboard_report.keycode[0] = (ch == '0') ? 39 : 30+(ch-'1');
		} else if(ch >= 'a' && ch <= 'z') {
			keyboard_report.modifier = (LED_state & 2) ? MOD_SHIFT_LEFT : 0;
			keyboard_report.keycode[0] = 4+(ch-'a');
		} else if(ch >= 'A' && ch <= 'Z') {
			keyboard_report.modifier = (LED_state & 2) ? 0 : MOD_SHIFT_LEFT;
			keyboard_report.keycode[0] = 4+(ch-'A');
		} else {
			keyboard_report.modifier = 0;
			keyboard_report.keycode[0] = 0;
			switch(ch) {
			case '.':
				keyboard_report.keycode[0] = 0x37;
				break;
			case '_':
				keyboard_report.modifier = MOD_SHIFT_LEFT;
			case '-':
				keyboard_report.keycode[0] = 0x2D;
				break;
			case ' ':
				keyboard_report.keycode[0] = 0x2C;
				break;
			case '\t':
				keyboard_report.keycode[0] = 0x2B;
				break;
			case '\n':
				keyboard_report.keycode[0] = 0x28;
				break;
			}
		}
	} else { // key release before the next keypress!
		keyboard_report.modifier = 0;
		keyboard_report.keycode[0] = 0;
	}
	
	messageCharNext = !messageCharNext; // invert
	
	return STATE_SEND;
}

usbMsgLen_t usbFunctionSetup(uchar data[8]) {
	usbRequest_t *rq = (void *)data;

	if((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS) {
		switch(rq->bRequest) {
		case USBRQ_HID_GET_REPORT: // send "no keys pressed" if asked here
			// wValue: ReportType (highbyte), ReportID (lowbyte)
			usbMsgPtr = (void *)&keyboard_report; // we only have this one
			keyboard_report.modifier = 0;
			keyboard_report.keycode[0] = 0;
			return sizeof(keyboard_report);
		case USBRQ_HID_SET_REPORT: // if wLength == 1, should be LED state
			return (rq->wLength.word == 1) ? USB_NO_MSG : 0;
		case USBRQ_HID_GET_IDLE: // send idle rate to PC as required by spec
			usbMsgPtr = &idleRate;
			return 1;
		case USBRQ_HID_SET_IDLE: // save idle rate as required by spec
			idleRate = rq->wValue.bytes[1];
			return 0;
		}
	}
	
	return 0; // by default don't return any data
}

void caps_toggle(); // defined later in program logic 

//
// Advance to the next slot available
//
void advance_slot (void) {
		if (slot < MAX_PASSWORD_SLOTS-1) slot++; else slot=0;
		display_state &= 0x80;                                  // isolate decimal point bit
		display_state |= pgm_read_byte ( digitos + slot);      // update number being displayed		
}

//
// Setup for sending the password
//
void set_to_send_password(void) {
		eeprom_read_block(messageBuffer, &stored_password[slot][0], sizeof(messageBuffer));
		messagePtr = 0;
		messageState = STATE_SEND;		
}


#define NUM_LOCK (1<<0)
#define CAPS_LOCK (1<<1)
#define SCROLL_LOCK (1<<2)


usbMsgLen_t usbFunctionWrite(uint8_t * data, uchar len) {
	uint8_t changed; // = data[0] ^ LED_state;
	
	if (LED_state == 0xff) LED_state = data [0]; 
	
	changed = data[0] ^ LED_state;
	
	if (!changed)
	return 1;
	else
	LED_state = data[0];
	if (changed & CAPS_LOCK ) caps_toggle();
	
	if (changed & SCROLL_LOCK ) {     // scroll lock 
        // if (USE_SCROLL_LOCK)	
		set_to_send_password();		
	}
	
	if (changed & NUM_LOCK ) {  // advance slot, roll back at maximum 
	    // if (USE_NUM_LOCK)
		advance_slot();			
	}
	
	return 1; // Data read, not expecting more
}

#define abs(x) ((x) > 0 ? (x) : (-x))



// *********************
// *** PROGRAM LOGIC ***
// *********************

// Routine to return a random character - currently uses the timer 0 as
// a source for randomness - it goes from 0 to 255 over 7800 times a second
// so it should be random enough (remember, this is called after user
// presses caps lock). Of course USB timings may decrease this randomness 
// - replace with a better one if you want.
uchar generate_character() {
	uchar counter = TCNT0 & 63;
	
	if(counter < 26)
	return 'a' + counter;
	else if(counter < 52)
	return 'A' + counter - 26;
	else if(counter < 62)
	return '0' + counter - 52;
	else if(counter == 62)
	return '-';
	else
	return '_';
}

#define CAPS_COUNTING 0
#define CAPS_MEASURING 1



// This routine is called by usbFunctionWrite every time the keyboard CAPS LOCK LED 
// toggle - basically we count 4 toggles and then start regenerating
void caps_toggle() {

static uchar capsCount = 0;
static uchar capsState = CAPS_COUNTING;

	if (B2_Pressed) {
		if(capsState == CAPS_COUNTING  ) {     
			if(capsCount++ < 4)
			return;
			
			capsCount = 0;
			capsState = CAPS_MEASURING;

			// Turn on Decimal Point to show we're regenerating the password
			display_state |= 0x80;
			
		} else {
			messageBuffer[capsCount++] = generate_character();
			
			if(capsCount >= PASS_LENGTH) { // enough characters generated
#if SEND_ENTER
				messageBuffer[capsCount++] = '\n';
#endif
				messageBuffer[capsCount] = '\0';
				
				// Turn off Decimal Point to show a new password has been generated
				display_state &= 0x7F;
				
				// Store password to EEPROM - might lose the USB connection, but so what
				eeprom_write_block(messageBuffer, &stored_password[slot][0], sizeof(messageBuffer));
				
				capsCount = 0;
				capsState = CAPS_COUNTING;
			}
		}    
	} else {
		// Send Not pressed, return to CAPS_MEASURING state
		
		display_state &= 0x7F;  // Turn OFF led XAyqb1fiMhCOXAyqb1fiMhCO
		capsCount = 0;                  
		capsState = CAPS_COUNTING;		
	}
}




void refresh_display( uint8_t segments) {

	uint8_t segment_now = (TCNT2 >> 5);
	
	// turn on/off segment according with state of digit
	switch (segment_now) {
	case _A:   if (segments & (1<<_A) )  _A_ON; else _A_OFF;
		break;
	case _B:   if (segments & (1<<_B) )  _B_ON; else _B_OFF;
		break;
	case _C:   if (segments & (1<<_C) )  _C_ON; else _C_OFF;
		break;
	case _D:   if (segments & (1<<_D) )  _D_ON; else _D_OFF;
		break;
	case _E:   if (segments & (1<<_E) )  _E_ON; else _E_OFF;
		break;
	case _F:   if (segments & (1<<_F) )  _F_ON; else _F_OFF;
		break;
	case _G:   if (segments & (1<<_G) )  _G_ON; else _G_OFF;
		break;
	case _DP:  if (segments & (1<<_DP) )  _DP_ON; else _DP_OFF;
		break;
	}
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//    __  __      _       
//   |  \/  |__ _(_)_ _   
//   | |\/| / _` | | ' \  
//   |_|  |_\__,_|_|_||_| 
//  
                      
int main() {
	uchar i;

	// Fetch password from EEPROM and send it
	//set_to_send_password();
	
	for(i=0; i<sizeof(keyboard_report); i++) // clear report initially
	((uchar *)&keyboard_report)[i] = 0;
	
	wdt_enable(WDTO_1S); // enable 1s watchdog timer

	usbInit();
	
	usbDeviceDisconnect(); // enforce re-enumeration
	for(i = 0; i<250; i++) { // wait 500 ms
		wdt_reset(); // keep the watchdog happy
		_delay_ms(2);
	}
	usbDeviceConnect();
	
	//TCCR0B |= (1 << CS01); // timer 0 at clk/8 will generate randomness
	TCCR0 |= (1 << CS01); // timer 0 at clk/8 will generate randomness
	
	TCCR2 = (1<<CS20) | (1<<CS21) | (1<<CS22);  // Timer 2 prescaler 1024
	// bits 7,6,5 turn at a rate of 488Hz, 
	// thus refreshing the display at 61Hz
	
	DDRC |= 0x33; // bits 0,1,4,5 as outputs
	DDRD |= 0xE0; // bits 5,6,7 as outputs
	DDRB |= 0x01; // bit 0 as output
	
	PORTB |= (1<<1) ; // button 2 pullup
	PORTD |= (1<<1) ; // button 1 pullup
	
	// Initialize display state
	display_state = pgm_read_byte ( digitos + slot);      // update number being displayed
	
	
	
	sei(); // Enable interrupts after re-enumeration 
	
	while(1) {
		wdt_reset(); // keep the watchdog happy   
		usbPoll();
		
		refresh_display(display_state);  // passar para global.
		
		if (debounce_B1() == _short_press) {
			advance_slot();
		}

		if (debounce_B2() == _short_press) {
			set_to_send_password();
		}		
		
		// characters are sent when messageState == STATE_SEND and after receiving  
		// the initial LED state from PC (good way to wait until device is recognized)
		if(usbInterruptIsReady() && messageState == STATE_SEND && LED_state != 0xff){
			messageState = buildReport();
			usbSetInterrupt((void *)&keyboard_report, sizeof(keyboard_report));
		}
	}
	
	return 0; 
}




