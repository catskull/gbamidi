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
#include <avr/boot.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include "gbasendrom.h"
#include "gbaserial.h"
#include "midi.h"

void mainLoop(void)  __attribute__ ((noreturn));
int main(void)  __attribute__ ((noreturn));

#define GBA_SYSEX_FILL_BUFF 0
#define GBA_SYSEX_PGM_PAGE 1

void mainLoop(void) {
	unsigned char c, b;
	unsigned char cmd;
	unsigned char hist[4];
	unsigned char pgmbuff[256];
	int pos;
	while(1) {
		c=midiGetChar();
		gbaSerTx(c);
		hist[0]=hist[1]; hist[1]=hist[2]; hist[2]=hist[3]; hist[3]=c;

		if (hist[0]==0xF0 && hist[1]==0x7D && hist[2]=='G' && hist[3]=='B') {
			//Special sysex command: update GBA program
			cmd=midiGetChar();
			if (cmd==GBA_SYSEX_FILL_BUFF) {
				//Fill the page buffer with some amount of nibbles.
				pos=midiGetChar()<<4;
				pos|=midiGetChar();
				while (1) {
					c=midiGetChar();
					if (c&0x80) break;
					b=(c<<4);
					c=midiGetChar();
					if (c&0x80) break;
					b|=c;
					pgmbuff[pos++]=b;
				}
			} else if (cmd==GBA_SYSEX_PGM_PAGE) {
				//Program the page into the AVRs memory
				pos=midiGetChar()<<4;
				pos|=midiGetChar();
				int ppos=0;
				int x;
				unsigned int w;
				unsigned int pageaddr=(pos*256);
				while (ppos<256) {
					cli();
					eeprom_busy_wait();
					boot_page_erase(pageaddr);
					boot_spm_busy_wait();
					for (x=0; x<SPM_PAGESIZE; x+=2) {
						w=pgmbuff[ppos++];
						w|=pgmbuff[ppos++]<<8;
						boot_page_fill(pageaddr+x, w);
					}
					boot_page_write(pageaddr);
					boot_spm_busy_wait();
					pageaddr+=SPM_PAGESIZE;
					sei();
				}
				boot_rww_enable();
			}
		}
	}
}


int main(void) {
	gbaSerInit();
	_delay_ms(600); //GBA boot up delay
	gbaSendRom(0, 1024*14);
	_delay_ms(3000); //GBA hates to receive non-multiboot stuff while still booting.

	//Ok, GBA program oughtta be running now. Go be a midi<->gbaserial converter.
	//Fix int vectors to boot loader range
	MCUCR=1; MCUCR=2;
	sei();
	//and go!
	midiInit();
	mainLoop();
}
