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
#include "gba.h"
#include "serial.h"
#include "int.h"
#include "seq.h"
#include "ui.h"
#include "sound.h"

#define RGB16(r,g,b)  ((r)+(g<<5)+(b<<10))

extern int __end__;

static void checkChsum() {
	unsigned short* Screen;
	Screen= (unsigned short*)0x6000000; 
	unsigned int chs;
	unsigned int *start=(unsigned int *) 0x20000E0;
	unsigned int *end=(unsigned int *)&__end__;
	unsigned int *p;
	int x;

	//Calculate checksum, which is basically every 32-bit word in the program
	//added, plus the final word which gets tacked on by the bin->midi converter.
	chs=0;
	p=start;
	for (p=start; p<=end; p++) {
		chs+=*p;
	}

	if (chs!= 0) {
		//Checksum failed! Display funky screen.
		*(unsigned long*)0x4000000 = 0x403; // mode3, bg2 on 
		for(x = 0; x<240*160; x++) *Screen++=x;
		while(1);
	}
}

int main() __attribute((noreturn));

//Main loop
int main() {
	int x;
	checkChsum();
	interruptsInit();
	soundInit();
	serialInit();
	uiInit();
	seqInit();
	while(1){
		serialTick();
		soundTick();
		uiTick();
	}
}
