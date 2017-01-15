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
#include <avr/eeprom.h>

void setBlackLogo(void) {
	unsigned int timeout;
	int rd;
	int i, var1, conf, len, var8;

	//Wait for GBA
	timeout=30;
	do {
		_delay_ms(50);
		rd=gbaSerXfer(0x6202);
		timeout--;
	} while (rd!=0x7202 && timeout>0);

	//Send 'header', unencrypted.
	rd=gbaSerXfer(0x6100);
	for (i=0; i<0x60; i++) {
		rd=gbaSerXfer(i<10?0:0xFFFF);
	}
	//Command: get encryption value
	gbaSerXfer(0x6202);
	gbaSerXfer(0x63C1);
	rd=gbaSerXfer(0x63C1);

	var1=((rd&0xFF)+0x20F)&0xff;
	conf=var1|0x6400L;
	rd=gbaSerXfer(conf);

	//Send length.
	len=((1000-0xC0L)>>2)-0x34L;
	var8=gbaSerXfer(len);

	for (i=0; i<100; i++) {
		var8=gbaSerXfer(0xffff);
	}
}
