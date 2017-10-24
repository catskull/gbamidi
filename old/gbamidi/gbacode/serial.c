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


//Serial routines. Grab a byte from the serial port and interpret it as midi data.

#include "gba.h"
#include "int.h"
#include "ccs.h"
#include "seq.h"
#include "sound.h"

#define SERBUFFSZ 16

static volatile char serialBuff[SERBUFFSZ];
static volatile char *serialBuffWptr=serialBuff, *serialBuffRptr=serialBuff;

void serialInt() {
	char recved=REG_SCD0;
	*serialBuffWptr=recved;
	serialBuffWptr++;
	if (serialBuffWptr==serialBuff+SERBUFFSZ) serialBuffWptr=serialBuff;
}

static int serialGetChar() {
	int r;
	if (serialBuffWptr==serialBuffRptr) return -1;
	r=*serialBuffRptr;
	serialBuffRptr++;
	if (serialBuffRptr==serialBuff+SERBUFFSZ) serialBuffRptr=serialBuff;
	return r;
}

void serialInit() {
	REG_SCCNT|=(1<<0xE); //enable int on completion
}

void serialTick() {
	static int pos=0;
	static char recved[7];
	static char channel;
	int r;

	r=serialGetChar();
	if (r==-1) return;

	recved[pos]=r;
	if (pos==0) {
		channel=(recved[0]&0xF)+1;
		recved[0]&=0xf0; //We already know this is our channel.
	}
	//Throw away messages we don't react to
	if (recved[0]!=0x80 && recved[0]!=0x90 && recved[0]!=0xB0 && recved[0]!=0xE0) {
		pos=0;
		return;
	}
	pos++;
	if (pos>7) pos=0;

	//See if we have to handle something
	if ((recved[0]==0x80 || recved[0]==0x90) && pos==3) { //note on/off
		if (recved[0]==0x80) recved[2]=0; //note off = note on with velocity of 0
		pos=0;
		if (seqNote(channel, recved[1], recved[2])) return; //seqNote may eat up the note
		if (channel==1) {
			soundPlayNote(recved[1], recved[2]);
		} else if (channel==2) {
			soundPlaySample(recved[1], recved[2]);
		} else if (channel==3) {
			soundPlayNoteSweep(recved[1], recved[2]);
		} else if (channel==4) {
			soundPlayDirect(recved[1], recved[2]);
		} else if (channel==10) {
			soundPlayNoise(recved[1], recved[2]);
		}
	} else if (recved[0]==0xB0 && pos==3) { //controller change
		unsigned int controllerVal=recved[2];
		unsigned char controllerNo=recved[1];
		if (controllerNo==123 || controllerNo==120) { //all sound/notes off
			int x;
			for (x=0; x<127; x++) {
				if (channel==1) {
					soundPlayNote(x, 0);
				} else if (channel==2) {
					soundPlaySample(x, 0);
				} else if (channel==3) {
					soundPlayNoteSweep(x, 0);
				} else if (channel==4) {
					soundPlayDirect(x, 0);
				} else if (channel==10) {
					soundPlayNoise(x, 0);
				}
			}
		} else {
			int sndfxno=ccPlusChanToSndfxNo(controllerNo, channel);
			if (sndfxno>0) {
				soundSetEffect(sndfxno, controllerVal);
				seqSndfx(sndfxno, controllerVal);
			}
		}
		pos=0;
	} else if (recved[0]==0xE0 && pos==3) { //pitch wheel
		int pitch;
		pitch=(recved[2]<<7)+recved[1];
		if (pitch>0x4000) pitch=0x4000;
		//Pitch now is 0x0000-0x4000. Convert to 0-128, cause that's what the rest of the sw understands.
		pitch>>=7;
		if (channel==1) {
			soundSetEffect(SNDFX_PITCH, pitch);
			seqSndfx(SNDFX_PITCH, pitch);
		} else if (channel==2) {
			soundSetEffect(SNDFX_SAMPLE_PITCH, pitch);
			seqSndfx(SNDFX_SAMPLE_PITCH, pitch);
		} else if (channel==3) {
			soundSetEffect(SNDFX_SWEEP_PITCH, pitch);
			seqSndfx(SNDFX_SWEEP_PITCH, pitch);
		} else if (channel==4) {
			soundSetEffect(SNDFX_DSOUND_PITCH, pitch);
			seqSndfx(SNDFX_DSOUND_PITCH, pitch);
		} else if (channel==10) {
			soundSetEffect(SNDFX_NOISE_PITCH, pitch);
			seqSndfx(SNDFX_NOISE_PITCH, pitch);
		}
		pos=0;
	}
}
