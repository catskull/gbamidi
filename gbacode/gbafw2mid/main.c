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

//Converts a gbamidi GBA binary to a midi file that can be used
//for a firmware upgrade. Also adds a correct checksum that's 
//checked in the gba code.

#define GBA_SYSEX_FILL_BUFF 0
#define GBA_SYSEX_PGM_PAGE 1

//Gets a character from the gba binary. Also keeps track of the checksum
//and if the last byte in the binary is passed, will additionally pass
//the 4 checksum bytes.
int getch() {
	static unsigned int chsum=0;
	static unsigned int n=0;
	static unsigned int no=0;
	static unsigned int v=0;
	int c;
	unsigned int t;

	c=getchar();
	if (c>=0) {
		n++;
		if (n==1) v=c;
		if (n==2) v|=(c<<8);
		if (n==3) v|=(c<<16);
		if (n>=4) {
			v|=(c<<24);
			if (no>=0xE0) chsum=(chsum+v)&0xFFFFFFFF;
			v=0;
			n=0;
		}
		no++;
		return c;
	}

	if (n>3) return -1;
	t=-chsum;
	if (n==0) fprintf(stderr, "Chsum: %x (-%x), n: %i, no=%i\n", chsum, t, n, no);

	if (n==0) c=(t>>0)&0xff;
	if (n==1) c=(t>>8)&0xff;
	if (n==2) c=(t>>16)&0xff;
	if (n==3) c=(t>>24)&0xff;

	n++;
	return c;
	if (n<=4) return c; else return -1;
}


#define CHUNKLEN 32
#define DELTA 2

int main(int argc, char **argv) {
	FILE *out;
	int done=0;
	int block=0;
	int pos=0, bpos=0;
	int note=24;
	int trackstart, trackend, tracksize;
	if (argc!=2) {
		printf("Usage: %s fwupdate.midi < gbacode.gba\n", argv[0]);
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
			fputc(GBA_SYSEX_FILL_BUFF, out); //command
			fputc(bpos>>4, out); fputc(bpos&0xf, out); //position
			for (x=0; x<CHUNKLEN; x++) {
				byte=getch();
				if (byte>=0) putchar(byte);
				if (byte<0) done=1;
				if (done) byte=0xff;
				fputc(byte>>4, out);
				fputc(byte&0xf, out);
				bpos++;
			}
			fputc(0xF7, out); //sysex end
		}

		//Program page consisting of uploaded chunks to memory
		fputc(DELTA, out); //delta; time between chunks
		fputc(0xF0, out); //sysex
		fputc(7, out); //sysex len
		fputc(0x7D, out); //manufacturer
		fputc('G', out); fputc('B', out); //magic
		fputc(GBA_SYSEX_PGM_PAGE, out); //command
		fputc(block>>4, out); fputc(block&0xf, out); //position
		fputc(0xF7, out); //sysex end

		//Add a bleep noise:
		fputc(DELTA, out); //delta; time between chunks
		fputc(0x90, out); //note on
		fputc(note, out); //note
		fputc(64, out); //velocity
		fputc(DELTA, out); //delta; time between chunks
		fputc(0x80, out); //note off
		fputc(note, out); //note
		fputc(0, out); //velocity

		note++;
		
		block++;
	}
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
