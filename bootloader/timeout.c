//Timer1 is used as the timeout interrupt: after a certain while, the AVR
//will stop to try to receive MIDI messages and boot into userspace.

#include <avr/io.h>
#include <avr/interrupt.h>
#include "payload.h"
#include "errorlogo.h"

ISR(TIMER1_COMPA_vect) {
	if (payloadCheck()) payloadBoot();
	//show error logo
	setBlackLogo();
	//Kill this int so the device will stay in bootloader mode
	TIMSK1&=~(2); //disable oc1a int
}

void timeoutInit(void) {
	TCCR1A=0;
	TCCR1B=5; // /1024
	TCNT1=0;
	TIMSK1=2; //enable oc1a int
	OCR1A=40000;  //about 2 seconds at 20KHz
}

void timeoutReset(void) {
	TCNT1=0;
}