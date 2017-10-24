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
#include "gbaencrypt.h"

static unsigned long gbaEncryptSeed;

void gbaEncryptSetSeed(unsigned char data) {
	gbaEncryptSeed=((data)<<8)|0xFFFF00C1L;
}

unsigned long gbaEncrypt(unsigned long data, int offset) {
	unsigned long pos, out;
	gbaEncryptSeed=(gbaEncryptSeed*0x6F646573L)+1;
	pos=~(offset+0x02000000L)+1;
	out=(data^gbaEncryptSeed)^(pos^0x6465646FL);
	return out;
}
