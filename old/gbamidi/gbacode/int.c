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

//Interrupt logic. Gets called from crt0.S

#include "gba.h"
#include "int.h"
#include "serial.h"
#include "sound.h"


void interruptsInit() {
	REG_IE=INT_COMUNICATION|INT_TIMER2;
	REG_IME=1;
}

void InterruptProcess() {
	REG_IME=0;
	if (REG_IF&INT_COMUNICATION) {
		serialInt();
		REG_IF=INT_COMUNICATION;
	} else if (REG_IF&INT_TIMER2) {
		soundDirectInt();
		REG_IF=INT_TIMER2;
	} else {
		//Wheuh? Didn't expect this int. Just confirm & return.
		REG_IF=REG_IF;
	}

	REG_IME=1;
}
