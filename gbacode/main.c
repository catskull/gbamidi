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
#include "nvmem.h"

int main() __attribute((noreturn));


//Main loop
int main() {
	int x;
	interruptsInit();
	soundInit();
	seqInit();
	serialInit();
	nvmemLoadAll();
	uiInit();
	while(1){
		while(serialTick());
		soundTick();
		uiTick();
	}
}
