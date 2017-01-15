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

//Generates the frequencies for the dds routines.

int main() {
	double freq, reg;
	double x;
	int ireg;
	int a = 440; // a is 440 hz...
	for (x = 0; x < 128; ++x) {
		freq = (a / 32.0F) * pow(2.0F, ((x - 2) / 12));
		reg=((freq*256.0F)/16386.0F)*((1<<24)-1);
		ireg=(int)(reg+0.5F);
		if (x>=24 && x<=96) printf("    0x%08x, //%u (%u)\n", ireg, (int)x, ((unsigned int)reg)>>24);
	}
}