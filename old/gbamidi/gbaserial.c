/*
Firmware for a gba-to-midi-cable with integrated sequencer/synthesizer
(C) 2011 Jeroen Domburg (jeroen AT spritesmods.com)

This program is free software: you can redistribute it and/or modify
t under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
	    
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
			    
You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#define F_CPU 20000000
#include <avr/io.h>
#include "util/delay.h"
#include <stdio.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>

//Port C
//SI, SO, SD and SC are defined from the GBAs standpoint.
#define GBA_SC 0
#define GBA_SD 1
#define GBA_SI 2
#define GBA_SO 3

//The char should take (F_CPU/115200) cycles. Unfortunately, that doesn't completely work, so there's a small
//painstakenly measured value substracted...
#define CHARLEN ((F_CPU/115200)-16)

static void wait(unsigned int len) {
	while(TCNT1<=len) ;
	TCNT1=0;
}

unsigned int gbaSerRx(void) {
	unsigned int ret=0;
	int t=0;
	//Make SD pin input
	PORTC|=(1<<GBA_SD);
	DDRC&=~(1<<GBA_SD);
	wait(0);
//	PORTC&=~(1<<GBA_SC);
	//Tell GBA we want a word by lowering its SI
	wait(CHARLEN);
	PORTC&=~(1<<GBA_SI);
	//Wait for startbit
	do {
		while(PINC&(1<<GBA_SD)) {
			t++;
			if (t>10000) {
				DDRC|=(1<<GBA_SC);
				PORTC|=(1<<GBA_SC);
				PORTC|=(1<<GBA_SI);
				return 0xFFFF;
			}
		}
		wait(CHARLEN/2);
	} while (PINC&(1<<GBA_SD));
	wait(CHARLEN); //start bit
	for (int x=0; x<16; x++) {
		ret>>=1;
		if (PINC&(1<<GBA_SD)) ret|=(1<<15);
		wait(CHARLEN);
	}
	wait(CHARLEN); //stop bit
	//Tell GBA we're done receiving.
	PORTC|=(1<<GBA_SI);
	DDRC|=(1<<GBA_SC);
	PORTC|=(1<<GBA_SC);
	return ret;
}

void gbaSerTx(unsigned int data) {
	int t=0;
	//Make SD and SC input
	PORTC|=(1<<GBA_SD);
	DDRC&=~(1<<GBA_SD);
	PORTC|=(1<<GBA_SC);
	DDRC&=~(1<<GBA_SC);

	//Tell the GBA we're sending by raising its SI
	PORTC|=(1<<GBA_SI);
	wait(0);

	//Wait for SD and SC to go high
	while(((PINC&((1<<GBA_SC)|(1<<GBA_SD)))!=((1<<GBA_SC)|(1<<GBA_SD)))) {
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
	DDRC|=(1<<GBA_SD);
	PORTC&=~(1<<GBA_SC);
	PORTC&=~(1<<GBA_SD);

	wait(CHARLEN); //start bit
	for (int x=0; x<16; x++) {
		if (data&1) {
			PORTC|=(1<<GBA_SD); 
		} else {
			PORTC&=~(1<<GBA_SD);
		}
		wait(CHARLEN); //data bit
		data>>=1;
	}
	PORTC|=(1<<GBA_SD);
	wait(CHARLEN); //stop bit
}

unsigned int gbaSerXfer(unsigned int data) {
	gbaSerTx(data);
	return gbaSerRx();
}


void gbaSerInit(void) {
	TCCR1B=1; //run timer0 at full speed
	DDRC|=(1<<GBA_SI);
	PORTC|=(1<<GBA_SD)|(1<<GBA_SC)|(1<<GBA_SI)|(1<<GBA_SO);
}


