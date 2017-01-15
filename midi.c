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
#include <stdio.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>

#define UBRVAL 39 //20MHz -> 31.25KBit.

#define BUFSZ 32

//Yes, everything has to be really, really volatile :)
volatile char midiInBuff[BUFSZ];
volatile char * volatile wpos, * volatile rpos;

ISR(USART_RX_vect) {
	*wpos=UDR0;
	wpos++;
	if (wpos>=midiInBuff+BUFSZ) wpos=midiInBuff;
}

int midiGetChar(void) {
	unsigned char ret;

	if (wpos==rpos) return -1;

	ret=*rpos;
	rpos++;
	if (rpos>=midiInBuff+BUFSZ) rpos=midiInBuff;
	return ret;
}

void midiInit(void) {
	wpos=midiInBuff;
	rpos=midiInBuff;
	UCSR0B=0x98;
	UBRR0L=UBRVAL&0xff;
	UBRR0H=UBRVAL>>8;
}
