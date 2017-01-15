
#define F_CPU 20000000
#include <avr/io.h>
#include <stdio.h>
#include <avr/wdt.h>
#include <avr/boot.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "gbaserial.h"
#include "midi.h"
#include "callable.h"
#include <avr/eeprom.h>
#include "timeout.h"
#include "payload.h"

void mainLoop(void)  __attribute__ ((noreturn));
int main(void)  __attribute__ ((noreturn));

static char canFlash=0;

//0 and 1 are mk1 flash commands
#define GBA_SYSEX_MK2_ATTN 2
#define GBA_SYSEX_MK2_FILL_BUFF 3
#define GBA_SYSEX_MK2_PGM_PAGE 4
#define GBA_SYSEX_MK2_METADATA 5


void mainLoop(void) {
	unsigned char c, b;
	unsigned char hist[4];
	unsigned char pgmbuff[256];
	unsigned char cmd;
	int pos;
	while(1) {
		c=midiGetChar();

		hist[0]=hist[1]; hist[1]=hist[2]; hist[2]=hist[3]; hist[3]=c;
		//Check the GBA fw update sysex command
		if (hist[0]==0xF0 && hist[1]==0x7D && hist[2]=='G' && hist[3]=='B') {
			cmd=midiGetChar();
			if (cmd==GBA_SYSEX_MK2_ATTN) {
				//The midi file for the update is playing.
				timeoutReset();
				canFlash=1;
				PINC|=(1<<PC1);
			} else if (cmd==GBA_SYSEX_MK2_FILL_BUFF && canFlash) {
				timeoutReset();
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
			} else if (cmd==GBA_SYSEX_MK2_PGM_PAGE && canFlash) {
				timeoutReset();
				//Program the page into the AVRs memory
				pos=midiGetChar()<<4;
				pos|=midiGetChar();
				int ppos=0;
				unsigned int pageaddr=(pos*256);
				while (ppos<256) {
					flashCopyPage(pageaddr+ppos, &pgmbuff[ppos]);
					ppos+=SPM_PAGESIZE;
				}
			} else if (cmd==GBA_SYSEX_MK2_METADATA && canFlash) {
				unsigned int len=0;
				unsigned int chsum=0;
				int x;
				for (x=0; x<4; x++) len=(len<<4)|midiGetChar();
				for (x=0; x<4; x++) chsum=(chsum<<4)|midiGetChar();
				payloadSetMeta(len, chsum);
				if (payloadCheck()) payloadBoot();
			}
		}
	}
}


int main(void) {
	//Init stuff
	_delay_ms(600);
	gbaSerInit();
	midiInit();
	timeoutInit();

	DDRC|=(1<<PC1);
	PORTC|=(1<<PC1);

	//Fix int vectors to boot loader range
	MCUCR=1; MCUCR=2;

	//Go wait for flash commands. The timer will take us to userspace if this takes
	//too long.
	sei();
	mainLoop();
}
