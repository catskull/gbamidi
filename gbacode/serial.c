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
#include "ui.h"
#include "ccs.h"
#include "seq.h"
#include "sound.h"

//Fairly sizey buffer: especially at the start and end of song (setup) the data can overwhelm the GBA.
#define SERBUFFSZ 128

static volatile char serialBuff[SERBUFFSZ];
static volatile char *serialBuffWptr=serialBuff, *serialBuffRptr=serialBuff;
static volatile int serialInBuffSize=0;

static volatile char serialOutBuff[SERBUFFSZ];
static volatile char *serialOutBuffWptr=serialOutBuff, *serialOutBuffRptr=serialOutBuff;
static volatile int serialOutBuffSize=0;

static volatile int maxPressure=0;

static volatile int midiChanSet;

static struct {
	int ch1;
	int ch2;
	int ch3;
	int ch4;
	int ch10;
} mChan[]={
	{1, 2, 3, 4, 10},
	{1, 2, 3, 4, 5},
	{5, 6, 7, 8, 9},
	{12, 13, 14, 15, 16}
};

int serialGetMaxPressure() {
	int r=maxPressure;
	maxPressure=0;
	return r;
}

void serialInt() {
	char recved=REG_SCCNT_H;

	*serialBuffWptr=recved;
	serialBuffWptr++;
	if (serialBuffWptr==serialBuff+SERBUFFSZ) serialBuffWptr=serialBuff;
	serialInBuffSize++;
	
	if (serialOutBuffWptr==serialOutBuffRptr) {
		REG_SCCNT_H=0xFE;
	} else {
		REG_SCCNT_H=*serialOutBuffRptr;
		serialOutBuffRptr++;
		if (serialOutBuffRptr==serialOutBuff+SERBUFFSZ) serialOutBuffRptr=serialOutBuff;
		serialOutBuffSize--;
	}
	REG_SCCNT_L|=0x80; //re-enable xfer
}

static int serialGetChar() {
	int r;
	if (serialBuffWptr==serialBuffRptr) {
		serialInBuffSize=0;
		return -1;
	}
	r=*serialBuffRptr;
	serialBuffRptr++;
	if (serialBuffRptr==serialBuff+SERBUFFSZ) serialBuffRptr=serialBuff;
	serialInBuffSize--;
	if (maxPressure<serialInBuffSize) maxPressure=serialInBuffSize;

	return r;
}

char serialRead(void) {
	int r;
	do {
		r=serialGetChar();
	} while (r==-1);
	return r;
}

void serialWrite(char byte) {
	while(serialOutBuffSize>=SERBUFFSZ-1) ;
	*serialOutBuffWptr=byte;
	serialOutBuffWptr++;
	if (serialOutBuffWptr==serialOutBuff+SERBUFFSZ) serialOutBuffWptr=serialOutBuff;
	serialOutBuffSize++;
}


void serialInit() {
	REG_SCCNT=0x4080; //8bit SPI, ext clock, int enabled
//	REG_SCCNT|=(1<<0xE); //enable int on completion
}

void serialSetChanSet(int chanSet) {
	midiChanSet=chanSet;
}

//Handles normal communication: midi messages etc
//Returns 1 if byte has been received, 0 if not
int serialTick() { 
	static int pos=0;
	static unsigned char recved[7];
	static char channel;
	static char escaped=0;
	static char prevFirstByte=0;
	static char inSysex=0;
	unsigned int r;

	r=serialGetChar();
	if (r==-1) return 0;
	if (r==0xff && !escaped) { //escape character
		escaped=1;
		return 1;
	}
	if (r==0xFE && !escaped) { //un-escaped filler character
		return 1;
	}
	escaped=0;
	uiDbgMidiIn(r);

	//If we're receiving a sysex message, ignore everything up to the 0xf7
	if (inSysex && r!=0xF7) return 1;
	inSysex=0;

	//Ignore realtime messages. These are one byte and can be interspersed within midi messages.
	if ((r&0xF8)==0xF8) return 1;

	recved[pos]=r;
	if (pos==0) {
		if ((r&0x80)==0) {
			//'Running status' -> the first byte was omitted. We should use the first byte of the previous message.
			if (prevFirstByte!=0) {
				recved[0]=prevFirstByte;
				recved[1]=r;
				pos++;
			} else {
				//Can't do running status; no previous command known.
				pos=-1; //so it'll get reset to 0 later
			}
		}
		//From here on, we can interpret recved[] as if the sending device doesn't use running status.
		if (recved[0]<=0xEF && pos!=-1) {
			//Voice message -> store status
			prevFirstByte=recved[0];
		} else if (recved[0]<=0xF7 && pos!=-1) {
			//System Common -> reset status.
			prevFirstByte=0;
			//Reset pos so all data bytes after this get ignored.
			pos=-1; //so it'll get reset to 0 later
		}
		channel=(recved[0]&0xF)+1;
		recved[0]&=0xf0; //We already know this is our channel.
		//CHANNEL CONVERSION
		//The rest of the software is hardcoded to use channel1-4,10. Do translation from whatever is set
		//as the channel set to that.
		if (channel==mChan[midiChanSet].ch1) {
			channel=1;
		} else if (channel==mChan[midiChanSet].ch2) {
			channel=2;
		} else if (channel==mChan[midiChanSet].ch3) {
			channel=3;
		} else if (channel==mChan[midiChanSet].ch4) {
			channel=4;
		} else if (channel==mChan[midiChanSet].ch10) {
			channel=10;
		} else {
			channel=-1;
		}
	}
	pos++;
	if (pos>7) pos=7; //don't overflow the receive buffer

	//See if we have to handle something
	if ((recved[0]==0x80 || recved[0]==0x90) && pos==3) { //note on/off
		if (recved[0]==0x80) recved[2]=0; //note off = note on with velocity of 0
		pos=0;
		if (seqNote(channel, recved[1], recved[2])) return 1; //seqNote may eat up the note
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
	} else if (recved[0]==0xA0 && pos==3) { //aftertouch
		pos=0;
		//Unimplemented.
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
	} else if (recved[0]==0xC0 && pos==2) { //program change
		pos=0; //unimplemented
	} else if (recved[0]==0xD0 && pos==2) { //channel pressure
		pos=0; //unimplemented
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
	} else if (recved[0]==0xF0 && pos==1) {
		inSysex=1;
		pos=0;
	}
	return 1;
}
