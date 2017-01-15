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
#include "gbarom.h"
#include "gbaserial.h"
#include "midi.h"
#include "bootloader/jumptable.h"

void mainLoop(void)  __attribute__ ((noreturn));
int main(void)  __attribute__ ((noreturn));

#define GBA_SYSEX_FILL_BUFF 0
#define GBA_SYSEX_PGM_PAGE 1

//ToDo:
// - Detect SD line, GBA gets attention by that
// - escape midi stuff so we can put EEPROM bytes there too if needed
// - Devise a better protocol to do that?
// - ...
// - Profit!

/*

GB Link Protocol:
AVR->GB:
[0-0xFD] - MIDI data
0xFE - filler, gets ignored by GB
0xFF - escape. Following byte:
 01 xx yy - stored controller value for controller xx is yy
 1x dd dd .. - stored 1K of data for track x
 0xFE - literal 0xFE
 0xFF - literal 0xFF

GB -> AVR:
FE - filler, gets sent when GB doesn't need anything
01 xx yy - save value yy for controller xx
02 xx - request value for controller xx
1x dd dd .. dd - save 1K of data for sequencer slot x
2x - request data for sequencer slot x
*/

#define LINK_ESCAPE 0xFF
#define LINK_FILLER 0xFE
#define LINK_STORE_CTR 0x1
#define LINK_REQ_CTR 0x2
#define LINK_STORE_TRACK 0x10
#define LINK_REQ_TRACK 0x20

#define FLASH_TRACK_OFFSET (1024*22)
#define FLASH_TRACK_SIZE (1024)

void storeSeqTrackData(int track) {
	//If this is called, the GB will send us 1024 bytes of data. We'll need to store these into the flash memory.
	int i, n=0;
	char page[SPM_PAGESIZE];
	int addr=(FLASH_TRACK_SIZE*track)+FLASH_TRACK_OFFSET; //Data will get stored from 22K to 30K in flash mem.
	if (track<0 || track>7) return;
	while (n<FLASH_TRACK_SIZE) {
		for (i=0; i<SPM_PAGESIZE; i++) {
			page[i]=gbaSerSpiTxRx(LINK_FILLER);
		}
		cli();
		call_flashCopyPage(addr, page);
		sei();
		addr+=SPM_PAGESIZE;
		n+=SPM_PAGESIZE;
	}
}


void sendSeqTrackData(int track) {
	int i;
	int addr=(FLASH_TRACK_SIZE*track)+FLASH_TRACK_OFFSET; //Data will get stored from 22K to 30K in flash mem.
	gbaSerSpiTxRx(LINK_ESCAPE);
	gbaSerSpiTxRx(LINK_REQ_TRACK|track);
	for (i=0; i<1024; i++) {
		gbaSerSpiTxRx(pgm_read_byte(addr+i));
	}
}

char actOnGbaChar(char b) {
	unsigned char c, d;
	if (b==LINK_STORE_CTR) {
		unsigned int adr=0;
		c=gbaSerSpiTxRx(LINK_FILLER);
		d=gbaSerSpiTxRx(LINK_FILLER);
		adr=c+(d<<8);
		b=gbaSerSpiTxRx(LINK_FILLER);
		eeprom_update_byte(adr+1, b); //skip the 1st byte of the eeprom, may be corrupted
	} else if (b==LINK_REQ_CTR) {
		unsigned int adr=0;
		c=gbaSerSpiTxRx(LINK_FILLER);
		d=gbaSerSpiTxRx(LINK_FILLER);
		adr=c+(d<<8);
		b=eeprom_read_byte(adr+1);
		gbaSerSpiTxRx(LINK_ESCAPE);
		gbaSerSpiTxRx(LINK_REQ_CTR);
		gbaSerSpiTxRx(c);
		gbaSerSpiTxRx(b);
	} else if ((b&0xf8)==LINK_STORE_TRACK) { //store track data
		cli();
		storeSeqTrackData(b&0x7);
		sei();
	} else if ((b&0xf8)==LINK_REQ_TRACK) { //request track data
		sendSeqTrackData(b&0x7);
	} else {
		//No idea. Probably a filler char.
		return 0;
	}
	return 1;
}

void mainLoop(void) {
	unsigned char b;
	unsigned char cmd;
	unsigned char hist[4];
	unsigned char pgmbuff[256];
	int c;
	int pos;
	int sendMoreIdleChars;
	//Timer 0 is used to send link idle chars: if the GBA link has been idle for >10ms, a
	//link idle byte is sent so the GBA has a chance to send data to the AVR.
	TCCR0A=0;
	TCCR0B=5; //20KHz timer0
	TCNT0=0;

#if 0
	cli();
	storeSeqTrackData(0);
	storeSeqTrackData(1);
	storeSeqTrackData(2);
	storeSeqTrackData(3);
	storeSeqTrackData(4);
	storeSeqTrackData(5);
	storeSeqTrackData(6);
	storeSeqTrackData(7);
	sei();
#endif

	while(1) {
		do {
			c=midiGetChar();
		} while (c==-1 && TCNT0<200 && sendMoreIdleChars==0);
		TCNT0=0;
		if (c==-1) {
			b=gbaSerSpiTxRx(0xFE); //send link idle char, to allow the GB to send stuff.
		} else {
			if (c==0xFF || c==0xFE) {
				b=gbaSerSpiTxRx(0xFF); //escape escape/idle character
				actOnGbaChar(b);
			}
			b=gbaSerSpiTxRx(c);
		}
		if (actOnGbaChar(b)) {
			//If GBA wants something from us, chances are it'll want more in the future. 
			//Send a few idle characters without delaying 10ms in between to make things go
			//faster.
			sendMoreIdleChars=10;
		} else {
			if (sendMoreIdleChars>0) sendMoreIdleChars--;
		}
	}
}


int main(void) {
	gbaSerInit();
	_delay_ms(600); //GBA boot up delay
	gbaSendRom(gbarom_data, gbarom_size); //Send GBA payload

	gbaSerSpiInit(); //Go to SPI mode for MIDI-data
	_delay_ms(800); //GBA hates to receive non-multiboot stuff while still booting.

	//Ok, GBA program oughtta be running now. Go be a midi<->gbaserial converter.
	sei();
	midiInit();
	mainLoop();
}
