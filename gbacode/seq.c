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


/*
Tiny little sequencer for e.g. drum loops.
*/

#include "sound.h"
#include "seq.h"
#include "nvmem.h"

#define SEQ_REC_NONE 0
#define SEQ_REC_KEYDOWN 1
#define SEQ_REC_SNDFX 2
#define SEQ_REC_END 3

#define NO_TRACKS 8
#define MAX_RECS 128 //Translates to usually 64 notes.
#define MAX_NOTES 8

struct seqRecordStruct { //8 bytes per recotd
	int timestamp;
	char type;
	char chan;
	char val;
	char velocity;
};

static struct seqRecordStruct seqTrack[NO_TRACKS][MAX_RECS];
static struct seqRecordStruct seqRecordingTrack[MAX_RECS];
static int seqRecordingTrackPos;
static int seqRecordingTrackNo=-1;
static int seqTimeStamp=-1;
static int seqTrkPos[NO_TRACKS];
static int seqTrkMuted[NO_TRACKS];
static int seqCurrMeas;
static int seqCurrBeat;
static int seqCurrTick;
static int seqUiHasChanged=1;
static int seqBassTransp=0;
static int seqBassTranspLatch;
static int bassNotesPlaying;

void *seqGetTrackPtr(int track) {
	return (void*)seqTrack[track];
}

int seqGetTimestamp() {
	return seqTimeStamp;
}

int getMeasInfo() {
	return (soundGetEffect(SNDFX_SEQ_NOMEAS)<<8)+soundGetEffect(SNDFX_SEQ_BPMEAS);
}

int getCurrMeas() {
	return ((seqCurrMeas+1)<<8)+(seqCurrBeat+1);
}

int getTrackStatus(int track) {
	int r=0;
	if (track<0 || track>=NO_TRACKS) return TRACK_NA;
	if (seqTrack[track][0].type==SEQ_REC_END) r|=TRACK_EMPTY;
	if (seqTimeStamp!=-1) {
		if (track==seqRecordingTrackNo) {
			r|=TRACK_RECORDING;
		} else if (!(r&TRACK_EMPTY)) {
			if (seqTrack[track][seqTrkPos[track]].type!=SEQ_REC_END) r|=TRACK_PLAYING;
		}
	}
	if (seqTrkMuted[track]) r|=TRACK_MUTED;
	return r;
}

void seqInit() {
	int x, y;
	for (x=0; x<MAX_RECS; x++) {
		for (y=0; y<NO_TRACKS; y++) seqTrack[y][x].type=SEQ_REC_END;
	}
}

static void startTrack() {
	int y;
	seqTimeStamp=0; seqCurrTick=0; seqCurrMeas=0; seqCurrBeat=0;
	for (y=0; y<NO_TRACKS; y++) {
		if (seqTrkMuted[y]) {
			seqTrkPos[y]=MAX_RECS-1;
		} else {
			seqTrkPos[y]=0;
		}
	}
}

int seqUiChanged() {
	int r=seqUiHasChanged;
	seqUiHasChanged=0;
	return r;
}

void seqRecordTrack(int trackNo) {
	seqRecordingTrackNo=trackNo;
	seqRecordingTrackPos=0;
	seqRecordingTrack[0].type=SEQ_REC_END;
	seqTrack[trackNo][0].type=SEQ_REC_END;
	startTrack();
	seqUiHasChanged=1;
}

//Validate track, replace with an empty one if invalid.
void seqValidateTrack(int trackno) {
	int valid=1;

	//Really stupid check. 
	if (seqTrack[trackno][0].type>3) valid=0;

	if (!valid) {
		seqTrack[trackno][0].type=SEQ_REC_END;
		seqUiHasChanged=1;
	}
}

void seqPlayBack() {
	seqRecordingTrackNo=-1;
	startTrack();
	seqUiHasChanged=1;
}

void seqToggleMuteTrack(int track) {
	seqTrkMuted[track]=!seqTrkMuted[track];
}

void seqStop() {
	int x,y,z;
	int quantDiff;

	//Mute ALL THE THINGS!!!
	for (x=0; x<127; x++) {
		soundPlayNote(x, 0);
		soundPlaySample(x, 0);
		soundPlayNoteSweep(x, 0);
		soundPlayDirect(x, 0);
		soundPlayNoise(x, 0);
	}
	bassNotesPlaying=0;

	if ((soundGetEffect(SNDFX_SEQ_AUTOTO)/32)==0) quantDiff=0;
	if ((soundGetEffect(SNDFX_SEQ_AUTOTO)/32)==1) quantDiff=128/4;
	if ((soundGetEffect(SNDFX_SEQ_AUTOTO)/32)==2) quantDiff=128/2;
	if ((soundGetEffect(SNDFX_SEQ_AUTOTO)/32)==3) quantDiff=128;

	if (seqRecordingTrackNo!=-1) {
		//Ok, we have the unsorted seqRecordingTrack and need to put it sorted into seqTrack[seqRecordingTrackNo].
		//Stupid sorting algorithm, but hey, it works :)
		//We apply quantizing here too.
		for (x=0; x<seqRecordingTrackPos; x++) {
			unsigned int minTsPos=-1, minTs=65535;
			unsigned int tgtTs;
			int diff;
			//Find the minimum timestamp.
			for (y=0; y<seqRecordingTrackPos; y++) {
				if (seqRecordingTrack[y].type!=SEQ_REC_END && seqRecordingTrack[y].timestamp<minTs) {
					minTsPos=y;
					minTs=seqRecordingTrack[y].timestamp;
				}
			}
			if (seqRecordingTrack[minTsPos].type==SEQ_REC_KEYDOWN && seqRecordingTrack[minTsPos].velocity!=0) {
				diff=seqRecordingTrack[minTsPos].timestamp%quantDiff;
				if (diff>(quantDiff/2)) diff-=quantDiff;
				diff=((diff*128)/soundGetEffect(SNDFX_SEQ_AUTOPCT));
				tgtTs=seqRecordingTrack[minTsPos].timestamp-diff;
				//Find noteUp, adjust by the same amount.
				for (z=x; z<seqRecordingTrackPos; z++) {
					if (seqRecordingTrack[z].type==SEQ_REC_KEYDOWN && seqRecordingTrack[z].velocity==0 &&
					 seqRecordingTrack[minTsPos].chan==seqRecordingTrack[z].chan &&
					 seqRecordingTrack[minTsPos].val==seqRecordingTrack[z].val) {
						//Found it.
						seqRecordingTrack[z].timestamp-=diff;
						break;
					}
				}
			} else {
				tgtTs=seqRecordingTrack[minTsPos].timestamp;
			}
			//Handle wraparounds
			if (tgtTs>=soundGetEffect(SNDFX_SEQ_BPMEAS)*soundGetEffect(SNDFX_SEQ_NOMEAS)*128) tgtTs=0;
			seqTrack[seqRecordingTrackNo][x].timestamp=tgtTs;
			seqTrack[seqRecordingTrackNo][x].type=seqRecordingTrack[minTsPos].type;
			seqTrack[seqRecordingTrackNo][x].chan=seqRecordingTrack[minTsPos].chan;
			seqTrack[seqRecordingTrackNo][x].val=seqRecordingTrack[minTsPos].val;
			seqTrack[seqRecordingTrackNo][x].velocity=seqRecordingTrack[minTsPos].velocity;
			seqRecordingTrack[minTsPos].type=SEQ_REC_END;
		}
		seqTrack[seqRecordingTrackNo][x].type=SEQ_REC_END;
		//Dump to AVR for storage in nvmem
		nvmemStoreTrack(seqRecordingTrackNo, (void *)seqTrack[seqRecordingTrackNo]);
	}
	seqRecordingTrackNo=-1;
	seqTimeStamp=-1;
	seqUiHasChanged=1;
}


//To be called with a speed dependent on seqSpeed. (Is atm called from sound.c)
void seqTick() {
	int x, p, v, s;
	if (seqTimeStamp!=-1) {
		//See if we need to play a note or change a sndfx.
		for (x=0; x<NO_TRACKS; x++) {
			while (seqTrack[x][seqTrkPos[x]].type!=SEQ_REC_END && seqTrack[x][seqTrkPos[x]].timestamp<=seqTimeStamp && x!=seqRecordingTrackNo) {
				p=seqTrkPos[x];
				//Play out recorded thingamajig.
				if (seqTrack[x][p].type==SEQ_REC_KEYDOWN) {
					v=seqTrack[x][p].val;
					s=seqTrack[x][p].velocity;
					//Apply changes in baseline offset only after all bassline keys have been released.. This
					//fixes the problem of the wrong noteUp commands sent after a baseline change, but only
					//if just one note is playing at the time...
					if (x==7) { //bass channel
						if (bassNotesPlaying==0) seqBassTransp=seqBassTranspLatch;
						v+=seqBassTransp;
					}
					if (v<0 || v>127) v=0;
					if (seqTrack[x][p].chan==1) {
						soundPlayNote(v, s);
					} else if (seqTrack[x][p].chan==2) {
						soundPlaySample(v, s);
					} else if (seqTrack[x][p].chan==3) {
						soundPlayNoteSweep(v, s);
					} else if (seqTrack[x][p].chan==4) {
						soundPlayDirect(v, s);
					} else if (seqTrack[x][p].chan==10) {
						soundPlayNoise(v, s);
					}
					if (x==7 && s==0) bassNotesPlaying--; else bassNotesPlaying++;
				} else if (seqTrack[x][p].type==SEQ_REC_SNDFX) {
					soundSetEffect(seqTrack[x][p].chan, seqTrack[x][p].val);
				}
				seqTrkPos[x]++;
			}
		}

		//Increment timestamp
		seqTimeStamp++;
		seqCurrTick++;
		if (seqCurrTick>=128) {
			seqUiHasChanged=1;
			seqCurrTick=0; seqCurrBeat++;
			if (seqCurrBeat>=soundGetEffect(SNDFX_SEQ_BPMEAS)) {
				seqCurrBeat=0; seqCurrMeas++;
				if (seqCurrMeas>=soundGetEffect(SNDFX_SEQ_NOMEAS)) {
					//Restart loop.
					startTrack();
				}
			}
		}
		//If needed, play metronome sounds.
		if (seqRecordingTrackNo!=-1) {
			if (seqCurrTick==0) {
				if (seqCurrBeat==0) {
					if (seqCurrMeas==0) {
						soundMetronome(32);
					} else {
						soundMetronome(8);
					}
				} else {
					soundMetronome(16);
				}
			}
		}
	}
}


//serial.c passes a note into this when it's pressed. This routine can
//return non-zero, in that case the note shouldn't be passed through to 
//the sound generation hardware.
int seqNote(int chan, int note, int vel) {
	int n, track, y;
	if (note<soundGetEffect(SNDFX_SEQ_BSPLIT)) {
		//Bass transpose event.
		seqBassTranspLatch=note+soundGetEffect(SNDFX_SEQ_BOFF)-76;
		return 1;
	}
	if (seqRecordingTrackNo!=-1 && seqTimeStamp>=0) {
		//Record if we're recording.
		if (seqRecordingTrackPos>=MAX_RECS-1) return 0;
		n=seqRecordingTrackPos++;
		seqRecordingTrack[n].timestamp=seqTimeStamp;
		seqRecordingTrack[n].type=SEQ_REC_KEYDOWN;
		seqRecordingTrack[n].chan=chan;
		seqRecordingTrack[n].val=note;
		seqRecordingTrack[n].velocity=vel;
		seqRecordingTrack[n+1].type=SEQ_REC_END;
	}
	return 0;
}

//Serial.c passes a sound effect (cc) here. We need to record this too in case
//of e.g. pitch bends.
void seqSndfx(int no, int val) {
	int n;
	
	if (seqRecordingTrackNo!=-1 && seqTimeStamp>=0) {
		if (seqRecordingTrackPos>=MAX_RECS-1) return;
		n=seqRecordingTrackPos++;
		seqRecordingTrack[n].timestamp=seqTimeStamp;
		seqRecordingTrack[n].type=SEQ_REC_SNDFX;
		seqRecordingTrack[n].chan=no;
		seqRecordingTrack[n].val=val;
		seqRecordingTrack[n+1].type=SEQ_REC_END;
	}
}

