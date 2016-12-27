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

//Load graphics, compressed by RLE-encoding, into memory. For the format of the
//gfx, check gentiles.php.

static int videoPos;
static int tilePos;

//Get the next nibble from the compressed tile data
static int getNibble() {
	int r;
	char *p=(char*)(tilePos>>1);
	if ((tilePos&1)==0) {
		r=(*p>>4)&0xf;
	} else {
		r=(*p)&0xf;
	}
	tilePos++;
	return r;
}

//Stupid vram can only be written in 16-bit increments.
static void putNibble(int n) {
	short *p=(short*)((videoPos>>1)&0xFFFFFFFE);
	if ((videoPos&3)==0) {
		*p=(n<<4);
	}else if ((videoPos&3)==1) {
		*p|=(n);
	} else if ((videoPos&3)==2) {
		*p|=(n<<12);
	} else {
		*p|=(n<<8);
	}
	videoPos++;
}

//Load the RLE-compressed tile data to the VRAM.
void gfxLoadTiles(char* videoRam, char* tileData) {
	int n, x, d;
	videoPos=((int)videoRam)<<1;
	tilePos=((int)tileData)<<1;
	while(1) {
		n=getNibble();
		if (n==0) {
			return;
		} else if (n<9) {
			d=getNibble();
			for (x=0; x<n; x++) putNibble(d);
		} else {
			n=n-8;
			for (x=0; x<n; x++) putNibble(getNibble());
		}
	}
}
