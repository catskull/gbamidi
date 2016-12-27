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

static long gbaCrcVal;
static unsigned char gbaCrcVar1;
static int gbaCrcVar8;

void gbaCrcInit(unsigned char var1, int var8) {
	gbaCrcVal=0xFFF8L;
	gbaCrcVar1=var1;
	gbaCrcVar8=var8;
}

void gbaCrcAdd(unsigned long data) {
	int bit;
	unsigned long var30;
	for (bit=0; bit<32; bit++) {
		var30=gbaCrcVal^data;
		gbaCrcVal>>=1;
		data>>=1;
		if (var30&1) gbaCrcVal^=0x0A517;
	}
}

long gbaCrcFinalize(unsigned long data) {
	unsigned long crctemp;
	crctemp=((((data&0xFF00)+gbaCrcVar8)<<8)|0xFFFF0000L)+gbaCrcVar1;
	gbaCrcAdd(crctemp);
	return gbaCrcVal;
}
