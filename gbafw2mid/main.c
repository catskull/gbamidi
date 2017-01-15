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
#include <stdlib.h>

//Converts a gbamidi AVR binary to a midi file that can be used
//for a firmware upgrade. Also adds a correct checksum that's 
//checked in the gba code.

//0 and 1 are mk1 flash commands; unused here
#define GBA_SYSEX_FILL_BUFF 0
#define GBA_SYSEX_PGM_PAGE 1
#define GBA_SYSEX_MK2_ATTN 2
#define GBA_SYSEX_MK2_FILL_BUFF 3
#define GBA_SYSEX_MK2_PGM_PAGE 4
#define GBA_SYSEX_MK2_METADATA 5 //Also boots into payload data

//Gets a character from the gba binary.
int getch() {
	int c=getchar();
	if (c>=0) return c;
	return -1;
}


#define CHUNKLEN 32
#define DELTA 2
#define ATTN_COUNT 100
#define ATTN_DELTA 10

int main(int argc, char **argv) {
	FILE *out;
	int done=0;
	int block=0;
	int pos=0, bpos=0;
	int plen=0, chsum=0xFFFF, chsbyte;
	int i;
	int trackstart, trackend, tracksize;
	if (argc!=2) {
		printf("Usage: %s fwupdate.midi < gbamidimk2.bin\n", argv[0]);
		exit(1);
	}
	out=fopen(argv[1], "w");
	if (out==NULL) {
		perror(argv[1]);
		exit(1);
	}
	
	//header
	fprintf(out, "MThd"); //chunk id
	fputc(0x00, out); fputc(0x00, out); fputc(0x00, out); fputc(0x06, out);  //chunk size
	fputc(0x00, out); fputc(0x00, out); //format type
	fputc(0x00, out); fputc(0x01, out); //no of tracks
	fputc(0x00, out); fputc(0x20, out); //time division

	//track header
	fprintf(out, "MTrk");
	fputc(0x00, out); fputc(0x00, out); fputc(0x00, out); fputc(0x00, out);  //chunk size, dunno yet

	trackstart=ftell(out);
	
	//Send attn calls
	for (i=0; i<ATTN_COUNT; i++) {
		fputc(ATTN_DELTA, out); //delta; time between chunks
		fputc(0xF0, out); //sysex
		fputc(5, out); //sysex len
		fputc(0x7D, out); //manufacturer
		fputc('G', out); fputc('B', out); //magic
		fputc(GBA_SYSEX_MK2_ATTN, out); //command
		fputc(0xF7, out); //sysex end
	}
	
	while (!done) {
		bpos=0;
		//Send chunks of data
		while (bpos<256) {
			int x, byte;
			fputc(DELTA, out); //delta; time between chunks
			fputc(0xF0, out); //sysex
			fputc((CHUNKLEN*2)+7, out); //sysex len
			fputc(0x7D, out); //manufacturer
			fputc('G', out); fputc('B', out); //magic
			fputc(GBA_SYSEX_MK2_FILL_BUFF, out); //command
			fputc(bpos>>4, out); fputc(bpos&0xf, out); //position
			for (x=0; x<CHUNKLEN; x++) {
				byte=getch();
				if (byte>=0) putchar(byte);
				if (byte<0) done=1;
				if (done) byte=0xff;
				fputc(byte>>4, out);
				fputc(byte&0xf, out);
				bpos++;
				plen++;
				if (plen&1) {
					chsbyte=byte;
				} else {
					chsbyte|=(byte<<8);
					chsum=(chsum>>1)+((chsum&1)<<15);
					chsum+=chsbyte;
					chsum&=0xFFFF;
				}
			}
			fputc(0xF7, out); //sysex end
		}

		//Program page consisting of uploaded chunks to memory
		fputc(DELTA, out); //delta; time between chunks
		fputc(0xF0, out); //sysex
		fputc(7, out); //sysex len
		fputc(0x7D, out); //manufacturer
		fputc('G', out); fputc('B', out); //magic
		fputc(GBA_SYSEX_MK2_PGM_PAGE, out); //command
		fputc(block>>4, out); fputc(block&0xf, out); //position
		fputc(0xF7, out); //sysex end

		block++;
	}

	//Send metadata (binary length and checksum)
	fprintf(stderr, "Metadata: len 0x%X, chsum 0x%X\n", plen, chsum);
	fputc(DELTA, out); //delta; time between chunks
	fputc(0xF0, out); //sysex
	fputc(13, out); //sysex len
	fputc(0x7D, out); //manufacturer
	fputc('G', out); fputc('B', out); //magic
	fputc(GBA_SYSEX_MK2_METADATA, out); //command
	for (i=0; i<4; i++) fputc(((plen<<(i*4))>>12)&0xf, out);
	for (i=0; i<4; i++) fputc(((chsum<<(i*4))>>12)&0xf, out);
	fputc(0xF7, out); //sysex end

	fputc(0x0, out);
	fputc(0xFF, out); fputc(0x2F, out); fputc(0, out); //Track end marker

	trackend=ftell(out);

	tracksize=(trackend-trackstart)-2;
	fprintf(stderr, "Start=%i end=%i size=%i\n",trackstart, trackend, tracksize);

	//Correct track length
	fseek(out, trackstart-4, SEEK_SET);
	fputc((tracksize>>24)&0xff, out);
	fputc((tracksize>>16)&0xff, out);
	fputc((tracksize>>8)&0xff, out);
	fputc((tracksize>>0)&0xff, out);

	return 0;
}
