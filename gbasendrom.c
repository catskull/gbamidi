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
#include "gbaserial.h"
#include "gbaencrypt.h"
#include "gbacrc.h"

#ifdef DEBUG
#define debugprintf(...) printf(__VA_ARGS__)
#else
#define debugprintf(...)
#endif

//Sends a ROM image stored in AVR program space to the GBA
void gbaSendRom(unsigned char *pgmSpaceStart, int length) {
	unsigned char *ptr;
	unsigned int rd, romsize, pos, var1, var8, conf, len, crc;
	unsigned long d;
	unsigned int timeout;
	int i;
	ptr=pgmSpaceStart;
	debugprintf("%04x\n",ptr);
	romsize=(length&0xfff0)+16; //pad to 16 bytes multiple

	//Wait for GBA
	debugprintf("Waiting for GBA...\n");
	timeout=30;
	do {
		_delay_ms(100);
		rd=gbaSerXfer(0x6202);
		timeout--;
	} while (rd!=0x7202 && timeout>0);
	debugprintf("Detected GBA!\n");

	//Send header, unencrypted.
	rd=gbaSerXfer(0x6100);
	for (i=0; i<0x60; i++) {
		d=pgm_read_byte(ptr++);
		d|=pgm_read_byte(ptr++)<<8;
		rd=gbaSerXfer(d);
		debugprintf("%x ", rd);
	}
	debugprintf("\nSent header\n");

	//Command: get encryption value
	gbaSerXfer(0x6202);
//	_delay_ms(100);
	gbaSerXfer(0x63C1);
//	_delay_ms(100);
	rd=gbaSerXfer(0x63C1);
	gbaEncryptSetSeed(rd);
	debugprintf("Encryption seed: %x\n", rd);

	//Calculate and confirm seed
	var1=((rd&0xFF)+0x20F)&0xff;
	conf=var1|0x6400L;
	rd=gbaSerXfer(conf);
	debugprintf("Confirmed with %04x (ret=%04x)\n", conf, rd);

	//Send length.
//	_delay_ms(1000);
	len=((romsize-0xC0L)>>2)-0x34L;
	var8=gbaSerXfer(len);
	debugprintf("Sending length: %x (var8=%04x)\n", len, var8);
	//Initialize crc engine
	gbaCrcInit(var1, var8);

	//Send encrypted data.
	debugprintf("Sending %i bytes of data (%i padded to 16b).\n", romsize, length);
	ptr=pgmSpaceStart+0xc0; 
	for (pos=0xc0; pos<romsize; pos+=4) {
		//Grab 32 bits of data.
		d=(long)pgm_read_byte(ptr++);
		d|=(long)pgm_read_byte(ptr++)<<8;
		d|=(long)pgm_read_byte(ptr++)<<16;
		d|=(long)pgm_read_byte(ptr++)<<24;
		gbaCrcAdd(d); //Handle crc
		debugprintf("%08lx ",d);
		d=gbaEncrypt(d, pos); //Encrypt the data
		rd=gbaSerXfer(d&0xffff);
//		debugprintf("%04x ", rd);
		rd=gbaSerXfer(d>>16);
//		debugprintf("%04x ", rd);
	}
	//Finish up
	debugprintf("\nFinishing up transfer...\n");
	gbaSerXfer(0x65); gbaSerXfer(0x65);
	timeout=30;
	while (gbaSerXfer(0x65)!=0x75 && timeout>0) {
		 _delay_ms(100);
		timeout--;
	}
	rd=gbaSerXfer(0x66);
	//Check CRC
	crc=gbaCrcFinalize(rd);
	rd=gbaSerXfer(crc);
	debugprintf("GBA crc: %04x my crc: %04x\n", rd, crc);
	//All done, gba sould be running the program now.
}
