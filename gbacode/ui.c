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

//This code displays and handles the user interface.

#include "gba.h"
#include "ccs.h"
#include "seq.h"
#include "sound.h"
#include "gfxloader.h"
#include "tiles.h"

#define KEY_RELEASED 0
#define KEY_PRESSED 1
#define KEY_CLICK 2
static int keys[10];

#define KEY_A			0x0000      // A Button
#define KEY_B			0x0001      // B Button
#define KEY_SELECT	    0x0002      // select button
#define KEY_START		0x0003      // START button
#define KEY_RIGHT       0x0004      // Right key
#define KEY_LEFT        0x0005      // Left key
#define KEY_UP          0x0006      // Up key
#define KEY_DOWN        0x0007      // Down key
#define KEY_SRIGHT	    0x0008      // R shoulder Button
#define KEY_SLEFT       0x0009		// L shoulder Button

#define REPDELAY 20
#define REPFREQ 2

#define TILE_BAR_START (129-32)
#define TILE_MID_START (133-32)
#define TILE_EMPTY (0)
#define TILE_ARROW (132-32)


//Parses any pressed keys into the keys[]-array. Stuff like debouncing and
//repeating keys are handled here.
static void parseKeys() {
	static int keyTime[10];
	int b=REG_KEYPAD;
	int x;
	for (x=0; x<10; x++) {
		if (b&(1<<x)) {
			//Key released.
			keyTime[x]=0;
			keys[x]=KEY_RELEASED;
		} else {
			keyTime[x]++;
			if (keyTime[x]<2) {
				keys[x]=KEY_RELEASED;
			} else if (keyTime[x]==2) {
				keys[x]=KEY_CLICK;
			} else if (keyTime[x]>REPDELAY+REPFREQ) {
				keys[x]=KEY_CLICK;
				keyTime[x]=REPDELAY;
			} else {
				keys[x]=KEY_PRESSED;
			}
		}
	}
}

static unsigned short *palRam=(unsigned short *)0x05000000;
static unsigned char *videoRam=(unsigned char *)0x06008000;
static unsigned short *tiles=(unsigned short *)0x06000000;

static void printStr(int x, int y, const char *str) {
	while (*str!=0) {
		tiles[x+y*32]=(*str)-32;
		str++;
		x++;
	}
}

static void printInt(int x, int y, int n) {
	int v;
	int t;
	if (n<0) n=0;
	if (n>9999) n=9999;
	t=n;
	while (t!=0) {
		x++;
		t/=10;
	}
	do {
		v=n%10;
		n=n/10;
		tiles[x+y*32]=('0')-32+v;
		x--;
	} while (n>0);
}

//val = (0..127)
static void showBar(int x, int y, int val) {
	int i;
	val/=2;
	tiles[x+y*32]=TILE_BAR_START;
	x++;
	for (i=0; i<8; i++) {
		if (val==8) {
			tiles[x+y*32]=TILE_MID_START+8;
		} else if (val<=7 && val>=0) {
			tiles[x+y*32]=TILE_MID_START+val;
		} else {
			tiles[x+y*32]=TILE_BAR_START+1;
		}
		x++;
		val-=8;
	}
	tiles[x+y*32]=TILE_BAR_START+2;
}

void showExpl(int x, int y, struct ccValExplStruct *expl, int val) {
	int oldValTo=0;
	int i;
	for (i=0; expl[i].desc!=NULL; i++) {
		if (expl[i].valTo>val && oldValTo<=val) {
			printStr(x, y, expl[i].desc);
			return;
		}
		oldValTo=expl[i].valTo;
	}
}

static int selectedChan=0;
static int selectedSlider=0;
static int selectedSndFx=0;
static int selectedTrack=0;
static int inSequencer=0;

void redrawSliders() {
	int i, x, y, n;
	int currCc=0;
	const char *names[]={"MIDI 1 (DUO)", "MIDI 2 (SAMPLE)", "MIDI 3 (SWEEPED)", "MIDI 4 (POLY)", "MIDI 10 (NOISE)", "GLOBAL", "SEQUENCER"};
	const int channels[]={1<<1,1<<2,1<<3,1<<4,1<<10,0xFFFF, 0};

	for (x=32*3; x<32*20; x++) tiles[x]=0;

	printStr(0, 3, names[selectedChan]);

	y=4; n=0;
	for (i=0; ccDefs[i].desc!=NULL; i++) {
		if (((ccDefs[i].midiChannels&channels[selectedChan]) && ccDefs[i].midiChannels!=0xffff && channels[selectedChan]!=0xffff) || (channels[selectedChan]==0xffff && ccDefs[i].midiChannels==0xffff) || (channels[selectedChan]==0 && ccDefs[i].midiChannels==0)) {
			if (selectedSlider==n) {
				selectedSndFx=ccDefs[i].sndFxNo;
				currCc=ccDefs[i].ccNo;
				tiles[y*32]=TILE_ARROW;
			} else {
				tiles[y*32]=TILE_EMPTY;
			}

			if (ccDefs[i].expl!=NULL) {
				showExpl(1, y, ccDefs[i].expl, soundGetEffect(ccDefs[i].sndFxNo));
			} else {
				showBar(1, y, soundGetEffect(ccDefs[i].sndFxNo));
			}

			printStr(12, y, ccDefs[i].desc);
			y++; n++;
		}
	}
	if (selectedSlider>=n) {
		selectedSlider=0;
		tiles[4*32]=TILE_ARROW;
	}
	if (selectedSlider<0) {
		selectedSlider=n-1;
		tiles[(3+n)*32]=TILE_ARROW;
	}
	y++;

	printStr(0, y, "Selected CC: ");
	if (currCc!=0) {
		printInt(13, y, currCc);
	} else {
		printStr(13, y, "n/a");
	}
	printStr(18, y, "Val: ");
	printInt(23, y, soundGetEffect(selectedSndFx));
	printStr(0, 19, "A/B=PREV/NEXT PAGE SEL=SEQ");

}

void redrawSequencer() {
	int x;
	for (x=32*3; x<32*20; x++) tiles[x]=0;
	printStr(10, 3, "SEQUENCER");
	printStr(0, 4, "Range:    meas of     beats.");
	printInt(7, 4, getMeasInfo()>>8);
	printInt(19, 4, getMeasInfo()&0xff);
	x=0;
	while (getTrackStatus(x)!=TRACK_NA) {
		if (selectedTrack==x) {
			tiles[(x+5)*32]=TILE_ARROW;
		} else {
			tiles[(x+5)*32]=TILE_EMPTY;
		}
		if (x!=7) {
			printInt(1, x+5, x+1);
		} else {
			printStr(2, x+5, "B");
		}
		if (getTrackStatus(x)&TRACK_EMPTY) printStr(9, x+5, "empty");
		if (getTrackStatus(x)&TRACK_RECORDING) printStr(4, x+5, "REC");
		if (getTrackStatus(x)&TRACK_PLAYING) printStr(4, x+5, "PLAY");
		if (getTrackStatus(x)&TRACK_MUTED) printStr(15, x+5, "MUTED");
		x++;
	}
	printInt(0, x+6, getCurrMeas()>>8);
	printStr(3, x+6, "/");
	printInt(4, x+6, getCurrMeas()&0xff);
	printStr(0, 19, "L/R=MUTE A=PLAY B=REC SEL=CCS");
}

void uiTick() {
	static int didThisVbl=0;
	int redrawNeeded;
	if (REG_DISPSTAT&1) {
		if (didThisVbl==1) return;
		didThisVbl=1;
	} else {
		didThisVbl=0;
		return;
	}
	
	parseKeys();
	
	if (keys[KEY_SELECT]==KEY_CLICK) {
		inSequencer=!inSequencer;
		redrawNeeded=1;
	}

	if (inSequencer) {
		redrawNeeded|=seqUiChanged();
		if (keys[KEY_UP]==KEY_CLICK) {
			selectedTrack--;
			if (selectedTrack<0) selectedTrack=0;
			redrawNeeded=1;
		}
		if (keys[KEY_DOWN]==KEY_CLICK) {
			selectedTrack++;
			if (getTrackStatus(selectedTrack)==TRACK_NA) selectedTrack--;
			redrawNeeded=1;
		}
		if (keys[KEY_LEFT]==KEY_CLICK || keys[KEY_RIGHT]==KEY_CLICK) {
			seqToggleMuteTrack(selectedTrack);
		}
		if (keys[KEY_A]==KEY_CLICK || keys[KEY_B]==KEY_CLICK) {
			redrawNeeded=1;
			if (seqGetTimestamp()!=-1) {
				seqStop();
			} else {
				if (keys[KEY_A]==KEY_CLICK) {
					seqPlayBack();
				} else {
					seqRecordTrack(selectedTrack);
				}
			}
		}
		if (redrawNeeded) redrawSequencer();
	} else {
		redrawNeeded|=soundGetEffectsChanged();
		if (keys[KEY_A]==KEY_CLICK) {
			selectedChan++;
			if (selectedChan>6) selectedChan=0;
			selectedSlider=0;
			redrawNeeded=1;
		}
		if (keys[KEY_B]==KEY_CLICK) {
			selectedChan--;
			if (selectedChan<0) selectedChan=6;
			selectedSlider=0;
			redrawNeeded=1;
		}
		if (keys[KEY_UP]==KEY_CLICK) {
			selectedSlider--;
			redrawNeeded=1;
		}
		if (keys[KEY_DOWN]==KEY_CLICK) {
			selectedSlider++;
			redrawNeeded=1;
		}
	
		if (keys[KEY_RIGHT] || keys[KEY_LEFT]) {
			int p=soundGetEffect(selectedSndFx);
			if (keys[KEY_RIGHT]) p++;
			if (keys[KEY_LEFT]) p--;
			if (p<0) p=0;
			if (p>127) p=127;
			soundSetEffect(selectedSndFx, p);
			redrawNeeded|=soundGetEffectsChanged();
		}
		if (redrawNeeded) redrawSliders();
	}
}


void uiInit() {
	int x;

	REG_DISPCNT=0x100; //text mode, bg0 enabled
	REG_BG0CNT=0x8; //tilemap @ 0, tiledata @ 32K
	REG_BG0HOFS=0;
	REG_BG0VOFS=0;
	gfxLoadTiles(videoRam, tileData);
	for (x=0; x<16; x++) palRam[x]=paletteData[x];

	printStr(0, 0, "     GBAMIDI BY SPRITE_TM     ");
	printStr(0, 1, "      (SPRITESMODS.COM)  v1.0 ");
	redrawSliders();
}
