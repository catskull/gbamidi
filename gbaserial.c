#define F_CPU 20000000
#include <avr/io.h>
#include "util/delay.h"
#include <stdio.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>

//SI, SO, SD and SC are defined from the GBAs standpoint.
#define GBA_SC PC0
#define GBA_SD PB1
#define GBA_SI PB2
#define GBA_SO PB0

//The char should take (F_CPU/115200) cycles. Unfortunately, that doesn't completely work, so there's a small
//painstakenly measured value substracted...
#define CHARLEN ((F_CPU/115200)-13)

static void wait(unsigned int len) {
	while(TCNT1<=len) ;
	TCNT1=0;
}

unsigned int gbaSerRx(void) {
	unsigned int ret=0;
	int t=0;
	//Make SD pin input
	PORTB|=(1<<GBA_SD);
	DDRB&=~(1<<GBA_SD);
	wait(0);
//	PORTC&=~(1<<GBA_SC);
	//Tell GBA we want a word by lowering its SI
	wait(CHARLEN);
	PORTB&=~(1<<GBA_SI);
	//Wait for startbit
	while(PINB&(1<<GBA_SD)) {
		t++;
		if (t>10000) {
			DDRC|=(1<<GBA_SC);
			PORTC|=(1<<GBA_SC);
			PORTB|=(1<<GBA_SI);
			return 0xFFFF;
		}
		wait(CHARLEN/2);
	}
	wait(CHARLEN); //start bit
	for (int x=0; x<16; x++) {
		ret>>=1;
		if (PINB&(1<<GBA_SD)) ret|=(1<<15);
		wait(CHARLEN);
	}
	wait(CHARLEN); //stop bit
	//Tell GBA we're done receiving.
	PORTB|=(1<<GBA_SI);
	DDRC|=(1<<GBA_SC);
	PORTC|=(1<<GBA_SC);
	return ret;
}

void gbaSerTx(unsigned int data) {
	int t=0;
	//Make SD and SC input
	PORTB|=(1<<GBA_SD);
	DDRB&=~(1<<GBA_SD);
	PORTC|=(1<<GBA_SC);
	DDRC&=~(1<<GBA_SC);

	//Tell the GBA we're sending by raising its SI
	PORTB|=(1<<GBA_SI);
	wait(0);

	//Wait for SD and SC to go high
	while ((PINC&(1<<GBA_SC))==0 || (PINB&(1<<GBA_SD))==0) {
		wait(CHARLEN/2);
		t++;
		if (t>10000) {
			return;
		}
	}
	//Wait 8 bit-times
	//Make SD and SC output (& low)
	wait(CHARLEN*8);
	DDRC|=(1<<GBA_SC);
	DDRB|=(1<<GBA_SD);
	PORTC&=~(1<<GBA_SC);
	PORTB&=~(1<<GBA_SD);

	wait(CHARLEN); //start bit
	for (int x=0; x<16; x++) {
		if (data&1) {
			PORTB|=(1<<GBA_SD); 
		} else {
			PORTB&=~(1<<GBA_SD);
		}
		wait(CHARLEN); //data bit
		data>>=1;
	}
	PORTB|=(1<<GBA_SD);
	wait(CHARLEN); //stop bit
}

unsigned int gbaSerXfer(unsigned int data) {
	gbaSerTx(data);
	return gbaSerRx();
}


void gbaSerInit(void) {
	TCCR1B=1; //run timer0 at full speed
	DDRB|=(1<<GBA_SI);
	PORTB|=(1<<GBA_SD)|(1<<GBA_SI)|(1<<GBA_SO);
	PORTC|=(1<<GBA_SC);
}


//The GB also has a SPI mode, which is a bit less timing-intensive and
//quicker and can have the interrupts enabled. It only transmits 
//8 bits, tho'.

unsigned char gbaSerSpiTxRx(unsigned char data) {
	unsigned char datain=0;
	char x;
//	while(PINB&(1<<GBA_SO)); //wait till SO is low
	for (x=0; x<8; x++) {
		if (data&0x80) PORTB|=(1<<GBA_SI); else PORTB&=~(1<<GBA_SI);
		data<<=1;
		PORTC&=~(1<<GBA_SC);
		_delay_us(4);
		datain<<=1;
		if (PINB&(1<<GBA_SO)) datain|=1;
		PORTC|=(1<<GBA_SC);
		_delay_us(4);
	}
	_delay_us(40);
	return datain;
}

void gbaSerSpiInit(void) {
	TCCR1B=0; //timer isn't necessary anymore
	DDRB|=(1<<GBA_SI);
	DDRC|=(1<<GBA_SC);
	PORTB|=(1<<GBA_SI)|(1<<GBA_SO);
	PORTC|=(1<<GBA_SC);
}
