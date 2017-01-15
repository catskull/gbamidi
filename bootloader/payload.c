//Routines to check and boot the main program (=payload).
//We check CRC and length to make sure we don't boot a faulty firmware
//image, introducing all kinds of weird errors.

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include "payload.h"

//Payload metadata gets saved at end of EEPROM mem.
const char *eep_metadata=(const char *)(1024-4);

//Check payload against a simple 
char payloadCheck() {
	unsigned int len, i;
	unsigned int chsum=0xFFFF, tchsum;
	unsigned int *p PROGMEM=(unsigned int *)0;
	len=eeprom_read_word(&eep_metadata[0]);
	tchsum=eeprom_read_word(&eep_metadata[2]);
	if (len<32 || len>32*1024) return 0; //Impossible length.
	for (i=0; i<len; i+=2) {
		chsum=(chsum>>1)+((chsum&1)<<15);
		chsum+=pgm_read_word(p++);
	}
	if (chsum!=tchsum) return 0;
	return 1;
}

//Idea borrowed from the usbasp-compatible bootloader
//http://www.obdev.at/products/vusb/usbasploader.html
static void (*nullVector)(void) __attribute__((__noreturn__));

char payloadBoot() {
	//Fix int vectors to userspace range
	MCUCR=1; MCUCR=0;
	//Reset the registers we touched
	PORTC=0; DDRC=0;
	TCCR1A=0; TCCR1B=0;
	TIMSK1=0; TIMSK0=0;
	UCSR0A=0; UCSR0B=0; UCSR0C=6;
	//Boot into userspace
	nullVector();
}

void payloadSetMeta(int size, unsigned int chsum) {
	eeprom_write_word(&eep_metadata[0], size);
	eeprom_write_word(&eep_metadata[2], chsum);
}

