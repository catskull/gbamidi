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
The main sound code.
Apologies for the massive amount of spaghetti-logic here, it sortta grew this way
and is a bit too intermingled to easily split up into multiple files. (Or maybe
not; actually I don't really feel like refactoring it.)
*/

#include "gba.h"
#include "midinotefreq.h"
#include "sound.h"
#include "seq.h"
#include "nvmem.h"
#include "serial.h"

#define MAXNOTES 8

struct dsndStruct {
	unsigned int pos;
	unsigned int inc;
	signed char *dsoundSampleBank;
};

static struct {
	unsigned char note;
	unsigned char velocity;
	unsigned char age;
	unsigned char decaying;
	unsigned char channel;
	union {
		unsigned char chord;
		struct dsndStruct dsnd;
	};
} notesPlaying[MAXNOTES];


static struct {
	int h;
	int l;
	int x;
} sndChanReg[4];

static struct {
	int active;
	int pos; //0-64K
	int freqFrom;
	int freqCurr;
} glissando[4]; //per midi channel

#define MAXLFO 3

static struct {
	int value;
	unsigned char tick;
	int range;
	unsigned char appliesTo;
	unsigned short oldTimerPos;
	unsigned short tickLen;
} lfoData[MAXLFO];

static char nowPlaying[4]={0,0,0,0};

static int doingArpeggio[2]={0, 0};

static int arpeggioTicklen=262144L/50;

static int effectValues[64];

static int sweepChanInUse=0;

static int soundMetronomeTick=0;
static int soundMetronomePos=-1;

static int ch0LegatoTarget;

//Swap around bytes to get the samples in the logical order.
#define SAMP(x) (((x&0xFF)<<24)|(((x>>8)&0xff)<<16)|(((x>>16)&0xff)<<8)|(((x>>24)&0xff)))
static const unsigned int chan3Samples[]={
	SAMP(0xffff0000), SAMP(0xffff0000), SAMP(0xffff0000), SAMP(0xffff0000),
	SAMP(0x048CC840), SAMP(0x048CC840), SAMP(0x048CC840), SAMP(0x048CC840),
	SAMP(0x02468ACE), SAMP(0x02468ACE), SAMP(0x02468ACE), SAMP(0x02468ACE),
	SAMP(0x00008888), SAMP(0x00008888), SAMP(0x8888FFFF), SAMP(0x8888FFFF),
	SAMP(0x888F0000), SAMP(0x888F0000), SAMP(0x888F0000), SAMP(0x888F0000),
	SAMP(0xA8868664), SAMP(0x86646442), SAMP(0x86646442), SAMP(0x64424220),
	SAMP(0xf8f8f8f8), SAMP(0xf8f8f8f8), SAMP(0x80808080), SAMP(0x80808080),
	SAMP(0x89327458), SAMP(0x08234623), SAMP(0x17263469), SAMP(0x52764010)
};
#undef SAMP

#define TICKLEN (65546L/50)
#define GLISSTICKLEN (65546L/500)
#define CHORDTRESH 2
#define LEGATOTRESH 6

//The waveform spitted out by the directsound channel
static signed char dsoundSamplesA[256];
static signed char dsoundSamplesB[256];
static signed char *dsoundCurSampleBank;


//DMA front- and backbuffers for directsound
#define DSNDBUFFSZ 128
static volatile char dsndBuffA[DSNDBUFFSZ], dsndBuffB[DSNDBUFFSZ];
static volatile char *dsndBuffFront, *dsndBuffBack;
static volatile int dsndBuffNeedsRefill;

static int effectValueEff(int effectNo);
const static int midiPlusPitchToFreq(int note, int pitch);
const static int midiPlusPitchToNoise(int note, int pitch);
static void soundDirectGenSamples(int square, int saw, int duty);
static void soundDirectRefillBuff();
static void switchSweepChan();
static volatile int soundDirectCount;

static int decayTicks;
static int oldTimerPosDecay;

static int seqTicklen;
static int oldTimerPosSeq;

#define GLISS_MODE_OFF 0
#define GLISS_MODE_LEGATO 1
#define GLISS_MODE_DOWN 2
#define GLISS_MODE_ALWAYS 3
static int glissandoMode[4]; //per midi channel
static int legatoDetected[4]; //per GB channel

static int someEffectChanged=1;


static int gbChanToGlissIdx(int gbchan) {
	//Figure out the corresponding midi channel.
	if (gbchan==ch0LegatoTarget) return 0;
	if (gbchan==2) return 2;
	return -1;
}

//Returns >0 when channel needs glissando.
//Returns 2 when channel _always_ needs glissando (aka: don't reset note on noteUp).
static int needsGliss(int gbchan) {
	int i;
	i=gbChanToGlissIdx(gbchan);
	if (i==-1) return 0;
	if (glissandoMode[i]==GLISS_MODE_LEGATO) return legatoDetected[gbchan];
	if (glissandoMode[i]==GLISS_MODE_DOWN) return 1;
	if (glissandoMode[i]==GLISS_MODE_ALWAYS) return 2;
	return 0;
}

static void updateGbChannelParams(int gbChannel, int reset) {
	int freq=0, i;
	if (gbChannel==0) {
		if (!sweepChanInUse) {
			freq=midiPlusPitchToFreq(nowPlaying[0], effectValueEff(SNDFX_PITCH));
		} else {
			freq=midiPlusPitchToFreq(nowPlaying[0], effectValueEff(SNDFX_SWEEP_PITCH));
		}
	} else if (gbChannel==1) {
		freq=midiPlusPitchToFreq(nowPlaying[1], effectValueEff(SNDFX_PITCH));
	} else if (gbChannel==2) {
		freq=midiPlusPitchToFreq(nowPlaying[2], effectValueEff(SNDFX_SAMPLE_PITCH));
	} else if (gbChannel==3) {
		freq=midiPlusPitchToNoise(nowPlaying[3], effectValueEff(SNDFX_NOISE_PITCH));
	}

	i=gbChanToGlissIdx(gbChannel);
	if (i!=-1 && glissando[i].active && glissando[i].pos!=65536 && nowPlaying[gbChannel]!=0) {
		freq=((freq*glissando[i].pos)+(glissando[i].freqFrom*(65536-glissando[i].pos)))>>16;
	}

	if (i!=-1 && nowPlaying[gbChannel]!=0) glissando[i].freqCurr=freq;

	if (gbChannel==0) {
		REG_SOUND1CNT_L=sndChanReg[0].l;
		REG_SOUND1CNT_H=(REG_SOUND1CNT_H&0xF000)|sndChanReg[0].h;
		REG_SOUND1CNT_X=sndChanReg[0].x|freq|(reset?0x8000:0);
	} else if (gbChannel==1) {
		REG_SOUND2CNT_L=(REG_SOUND2CNT_L&0xF000)|sndChanReg[1].l;
		REG_SOUND2CNT_H=sndChanReg[1].h|freq|(reset?0x8000:0);
	} else if (gbChannel==2) {
		REG_SOUND3CNT_L=sndChanReg[2].l;
		REG_SOUND3CNT_H=(REG_SOUND3CNT_H&0xF000)|sndChanReg[2].h;
		REG_SOUND3CNT_X=sndChanReg[2].x|freq|(reset?0x8000:0);
	} else if (gbChannel==3) {
		REG_SOUND4CNT_L=(REG_SOUND4CNT_L&0xF000)|sndChanReg[3].l;
		REG_SOUND4CNT_H=sndChanReg[3].h|freq|(reset?0x8000:0);
	}
}


static void chan3LoadSample(int sampno) {
	sampno*=4;
	REG_WAVE_RAM0=chan3Samples[sampno++];
	REG_WAVE_RAM1=chan3Samples[sampno++];
	REG_WAVE_RAM2=chan3Samples[sampno++];
	REG_WAVE_RAM3=chan3Samples[sampno++];
	sndChanReg[2].l^=0x40; //swap sample banks
//	updateGbChannelParams(3, 0); //Don't; resets note
	REG_SOUND3CNT_L=sndChanReg[2].l; //Manually is better here.
}

static void soundReset() {
	int x;
	unsigned static const char ccDefaults[]={
		SNDFX_PITCH, 64,
		SNDFX_SAMPLE_PITCH, 64, 
		SNDFX_SWEEP_PITCH, 64, 
		SNDFX_NOISE_PITCH, 64, 
		SNDFX_DSOUND_PITCH, 64, 
		SNDFX_DUTYCYCLE, 127,
		SNDFX_ARPEGGIATOR, 100,
		SNDFX_NOISE_BITS, 60,
		SNDFX_SAMPLE_SETSAMP, 0,
		SNDFX_SAMPLE_SNDLEN, 127,
		SNDFX_SNDLEN, 127,
		SNDFX_ENVELOPE, 63,
		SNDFX_NOISE_ENVELOPE, 32,
		SNDFX_NOISE_SNDLEN, 127,
		SNDFX_SWEEP_DUTYCYCLE, 127,
		SNDFX_SWEEP_ENVELOPE, 63,
		SNDFX_SWEEP_SNDLEN, 127,
		SNDFX_SWEEP_VAL, 0,
		SNDFX_SWEEP_SPEED, 44,
		SNDFX_DSNDSQUARE, 127,
		SNDFX_DSNDSAW, 0,
		SNDFX_DSNDDUTY, 127,
		SNDFX_DECAY, 10,
		SNDFX_GLISS_SPEED, 64,
		SNDFX_GLISS_MODE, 50,
		SNDFX_SAMPLE_GLISS_MODE, 40,
		SNDFX_LFO1_TARGET, 32,
		SNDFX_LFO1_RANGE, 0,
		SNDFX_LFO1_FREQ, 110,
		SNDFX_LFO2_TARGET, 43,
		SNDFX_LFO2_RANGE, 0,
		SNDFX_LFO2_FREQ, 78,
		SNDFX_LFO3_TARGET, 0,
		SNDFX_LFO3_RANGE, 0,
		SNDFX_LFO3_FREQ, 28,
		SNDFX_SEQ_SPEED, 64,
		SNDFX_SEQ_BPMEAS, 4,
		SNDFX_SEQ_NOMEAS, 4,
		SNDFX_SEQ_AUTOTO, 64,
		SNDFX_SEQ_AUTOPCT, 127,
		SNDFX_SEQ_BSPLIT, 0,
		SNDFX_SEQ_BOFF, 20,
		SNDFX_PAN_CH1, 64,
		SNDFX_PAN_CH2, 64,
		SNDFX_PAN_CH3, 64,
		SNDFX_PAN_CH4, 64,
		SNDFX_PAN_CH10, 64,
		SNDFX_SAMPR, 0,
		0,0
	};

	REG_SOUNDCNT_L=0xFF77; //enable channel 1-4 at full volume
	
	sndChanReg[0].l=0x00;
	sndChanReg[0].h=0x780;
	sndChanReg[0].x=0x0;

	sndChanReg[1].l=0x780;
	sndChanReg[1].h=0;

	sndChanReg[2].l=0x80;
	sndChanReg[2].h=0xF;
	sndChanReg[2].x=0;

	sndChanReg[3].l=0x80;
	sndChanReg[3].h=0;

	x=0;
	sweepChanInUse=1;
	while (ccDefaults[x]!=0) {
		soundSetEffect(ccDefaults[x], ccDefaults[x+1]);
		x+=2;
	}
	sweepChanInUse=0;
	switchSweepChan();

	for (x=0; x<4; x++) glissando[x].active=0;

	dsndBuffFront=dsndBuffA; dsndBuffBack=dsndBuffB; dsndBuffNeedsRefill=1; soundDirectRefillBuff();
	REG_SOUNDCNT_H=0xF00E; //enable directsound on timer 1
	//Init timer 0 for arpeggiator etc
	REG_TM0CNT_H=0x82; //feed with 65546.875KHz
	//Init timer 1 for directsound sample rate
	REG_TM1CNT_L=0xFD06; //22KHz
	REG_TM1CNT_H=0x80;
	//Chain timer 2 to timer 1, for dma refill.
	REG_TM2CNT_L=0x10000-DSNDBUFFSZ;
	REG_TM2CNT_H=0xC4;
	//Set up DMA to provide directsound channel with samples.
	REG_DMA2SAD=(unsigned long)dsndBuffA;
	REG_DMA2DAD=0x040000a4;
	REG_DMA2CNT_H=0xB640;
	dsndBuffFront=dsndBuffB; dsndBuffBack=dsndBuffA;

	//Clear note memory
	for (x=0; x<MAXNOTES; x++) {
		notesPlaying[x].note=0;
		notesPlaying[x].velocity=0;
		notesPlaying[x].age=0;
		notesPlaying[x].chord=0;
		notesPlaying[x].channel=0;
	}
}

void soundInit() {
	REG_SOUNDCNT_X=0x80; //enable sound
	soundReset();
}

//Grab the effective value of an effect (SNDFX_*) after the LFOs are applied to it.
static int effectValueEff(int effectNo) {
	int x, val;
	val=effectValues[effectNo];

	for (x=0; x<MAXLFO; x++) {
		if (lfoData[x].appliesTo==effectNo && lfoData[x].range!=0) {
			val+=lfoData[x].value>>7;
		}
	}
	if (val>127) val=127;
	if (val<0) val=0;

	return val;
}

const static int midiPlusPitchToFreq(int note, int pitch) {
	int from, diff;
	if (pitch<0) pitch=0;
	if (pitch>127) pitch=127;
	if (pitch<64) {
		from=midiToFreq(note-2);
		diff=midiToFreq(note)-from;
	} else if (pitch>=64) {
		pitch-=64;
		from=midiToFreq(note);
		diff=midiToFreq(note+2)-from;
	}
	return from+((diff*pitch)/64);
}

const static int midiPlusPitchToNoise(int note, int pitch) {
	int m, n;
	note=(103-note)/2;
	note+=((pitch-64)>>3);
	m=(note&7);
	n=(note>>3)*3; //steps of /8^0, /8^1, /8^2 etc
	return m|(n<<4);
}

void setPan(int ch, int value) {
	int lb, rb;
	rb=8+(ch-1);
	lb=12+(ch-1);
	if (value>85) REG_SOUNDCNT_L&=~(1<<lb); else REG_SOUNDCNT_L|=(1<<lb);
	if (value<43) REG_SOUNDCNT_L&=~(1<<rb); else REG_SOUNDCNT_L|=(1<<rb);
}

static void soundSetEffectHw(int effectNo, int value) {
	if (effectNo==SNDFX_DUTYCYCLE) {
		int w;
		if (value<43) {
			w=0x0;
		} else if (value<85) {
			w=0x40;
		} else {
			w=0x80;
		}
		if (!sweepChanInUse) {
			sndChanReg[0].h=(sndChanReg[0].h&0xFF3F)|w;
			updateGbChannelParams(0, 0);
		}
		sndChanReg[1].l=(sndChanReg[1].l&0xFF3F)|w;
		updateGbChannelParams(1, 0);
	} else if (effectNo==SNDFX_SWEEP_DUTYCYCLE && sweepChanInUse) {
		int w;
		if (value<43) {
			w=0x0;
		} else if (value<85) {
			w=0x40;
		} else {
			w=0x80;
		}
		sndChanReg[0].h=(sndChanReg[0].h&0xFF3F)|w;
		updateGbChannelParams(0, 0);
	} else if (effectNo==SNDFX_PITCH) {
		if (!sweepChanInUse) updateGbChannelParams(0, 0);
		updateGbChannelParams(1, 0);
	} else if (effectNo==SNDFX_SAMPLE_PITCH) {
		updateGbChannelParams(2, 0);
	} else if (effectNo==SNDFX_NOISE_PITCH) {
		updateGbChannelParams(3, 0);
	} else if (effectNo==SNDFX_ENVELOPE) {
		value+=16; if (value>127) value=0;
		if (!sweepChanInUse) sndChanReg[0].h=(sndChanReg[0].h&0xF8FF)|((value>>4)<<8);
		sndChanReg[1].l=(sndChanReg[1].l&0xF0FF)|((value>>4)<<8);
		updateGbChannelParams(1, 0);
	} else if (effectNo==SNDFX_NOISE_ENVELOPE) {
		value+=16; if (value>127) value=0;
		sndChanReg[3].l=(sndChanReg[3].l&0xF0FF)|((value>>4)<<8);
		updateGbChannelParams(3, 0);
	} else if (effectNo==SNDFX_SNDLEN) {
		if (!sweepChanInUse) {
			if (value==127) sndChanReg[0].x&=0xBFFF; else sndChanReg[0].x|=0x4000;
			sndChanReg[0].h=(sndChanReg[0].h&0xFFC0)|((127-value)>>1);
			updateGbChannelParams(0, 0);
		}
		if (value==127) sndChanReg[1].h&=0xBFFF; else sndChanReg[1].h|=0x4000;
		sndChanReg[1].l=(sndChanReg[1].l&0xFFC0)|((127-value)>>1);
		updateGbChannelParams(1, 0);
	} else if (effectNo==SNDFX_SAMPLE_SNDLEN) {
		if (value==127) sndChanReg[2].x&=0xBFFF; else sndChanReg[2].x|=0x4000;
		sndChanReg[2].h=(sndChanReg[2].h&0xFF00)|((127-value)<<1);
		updateGbChannelParams(2, 0);
	} else if (effectNo==SNDFX_NOISE_SNDLEN) {
		if (value==127) sndChanReg[3].h&=0xBFFF; else sndChanReg[3].h|=0x4000;
		sndChanReg[3].l=(sndChanReg[3].l&0xFFC0)|((127-value)>>1);
		updateGbChannelParams(3, 0);
	} else if (effectNo==SNDFX_SWEEP_SNDLEN && sweepChanInUse) {
		if (value==127) sndChanReg[0].x&=0xBFFF; else sndChanReg[0].x|=0x4000;
		sndChanReg[0].h=(sndChanReg[0].h&0xFFC0)|((127-value)>>1);
	} else if (effectNo==SNDFX_ARPEGGIATOR) {
		if (value!=0) {
			arpeggioTicklen=((128-value)*128);
		} else {
			arpeggioTicklen=0;
		}
	} else if (effectNo==SNDFX_SWEEP_VAL) { //sweep step size
		if (!sweepChanInUse) value=0;
		if (value>111) value=111; //make sure register contents != 0, which disables sweep
		sndChanReg[0].l=(sndChanReg[0].l&0xFFF8)|((127-value)>>4);
	} else if (effectNo==SNDFX_SWEEP_SPEED) {
		if (!sweepChanInUse) value=64;
		if (value<8) value=8; //0 disables sweep altogether
		if (value>119) value=119; //0 disables sweep altogether
		value-=64;
		if (value<=0) {
			value=value+64;
			if (value==64) value=0;
			sndChanReg[0].l=(sndChanReg[0].l&0xF)|((value>>3)<<4)|0x8;
		} else {
			value=64-value;
			sndChanReg[0].l=(sndChanReg[0].l&0xF)|((value>>3)<<4);
		}
	} else if (effectNo==SNDFX_SWEEP_PITCH && sweepChanInUse) {
		updateGbChannelParams(0, 0);
	} else if (effectNo==SNDFX_SWEEP_ENVELOPE && sweepChanInUse) {
		value+=16; if (value>127) value=0;
		sndChanReg[0].h=(sndChanReg[0].h&0xF0FF)|((value>>4)<<8);
		updateGbChannelParams(0, 0);
	} else if (effectNo==SNDFX_NOISE_BITS) {
		sndChanReg[3].h=(sndChanReg[3].h&0xFFF7)|((value>>6)<<3);
		updateGbChannelParams(3, 0);
	} else if (effectNo==SNDFX_SAMPLE_SETSAMP) {
		chan3LoadSample(value/16);
	} else if (effectNo==SNDFX_DSNDSQUARE || effectNo==SNDFX_DSNDSAW || effectNo==SNDFX_DSNDDUTY) {
		soundDirectGenSamples(effectValueEff(SNDFX_DSNDSQUARE), effectValueEff(SNDFX_DSNDSAW), effectValueEff(SNDFX_DSNDDUTY));
	} else if (effectNo==SNDFX_LFO1_TARGET) {
		soundSetEffectHw(lfoData[0].appliesTo, effectValues[lfoData[0].appliesTo]);
		lfoData[0].appliesTo=(value>>3)+1;
	} else if (effectNo==SNDFX_LFO1_RANGE) {
		lfoData[0].range=value;
	} else if (effectNo==SNDFX_LFO1_FREQ) {
		lfoData[0].tickLen=(128-value)*4;
	} else if (effectNo==SNDFX_LFO2_TARGET) {
		soundSetEffectHw(lfoData[1].appliesTo, effectValues[lfoData[1].appliesTo]);
		lfoData[1].appliesTo=(value>>3)+1;
	} else if (effectNo==SNDFX_LFO2_RANGE) {
		lfoData[1].range=value;
	} else if (effectNo==SNDFX_LFO2_FREQ) {
		lfoData[1].tickLen=(128-value)*4;
	} else if (effectNo==SNDFX_LFO3_TARGET) {
		soundSetEffectHw(lfoData[2].appliesTo, effectValues[lfoData[2].appliesTo]);
		lfoData[2].appliesTo=(value>>3)+1;
	} else if (effectNo==SNDFX_LFO3_RANGE) {
		lfoData[2].range=value;
	} else if (effectNo==SNDFX_LFO3_FREQ) {
		lfoData[2].tickLen=(128-value)*4;
	} else if (effectNo==SNDFX_DECAY) {
		decayTicks=value*20;
	} else if (effectNo==SNDFX_GLISS_MODE || effectNo==SNDFX_SAMPLE_GLISS_MODE) {
		int n=(effectNo==SNDFX_GLISS_MODE)?0:2;
		if (value<32) {
			glissandoMode[n]=GLISS_MODE_OFF;
		} else if (value<64) {
			glissandoMode[n]=GLISS_MODE_LEGATO;
		} else if (value<96) {
			glissandoMode[n]=GLISS_MODE_DOWN;
		} else {
			glissandoMode[n]=GLISS_MODE_ALWAYS;
		}
	} else if (effectNo==SNDFX_SEQ_SPEED) {
		if (value!=0) {
			seqTicklen=((128-value)*4);
		} else {
			seqTicklen=0;
		}
	} else if (effectNo==SNDFX_PAN_CH1) {
		if (!sweepChanInUse) setPan(1, value);
		setPan(2, value);
	} else if (effectNo==SNDFX_PAN_CH2) {
		setPan(3, value);
	} else if (effectNo==SNDFX_PAN_CH3) {
		if (sweepChanInUse) setPan(1, value);
	} else if (effectNo==SNDFX_PAN_CH4) {
		if (value>85) REG_SOUNDCNT_H&=~(1<<13); else REG_SOUNDCNT_H|=(1<<13);
		if (value<43) REG_SOUNDCNT_H&=~(1<<12); else REG_SOUNDCNT_H|=(1<<12);
	} else if (effectNo==SNDFX_PAN_CH10) {
		setPan(4, value);
	} else if (effectNo==SNDFX_SAMPR) {
		REG_SOUNDBIAS=REG_SOUNDBIAS&0x3FFF|((value>>5)<<14);
	} else if (effectNo==SNDFX_MIDICHAN) {
		serialSetChanSet(value>>5);
	}
}

static void switchSweepChan() {
	//Switch the properties of hw chan 1 dependant on sweepChanInUse
	if (sweepChanInUse) {
		soundSetEffectHw(SNDFX_SWEEP_VAL, effectValueEff(SNDFX_SWEEP_VAL));
		soundSetEffectHw(SNDFX_SWEEP_PITCH, effectValueEff(SNDFX_SWEEP_PITCH));
		soundSetEffectHw(SNDFX_SWEEP_ENVELOPE, effectValueEff(SNDFX_SWEEP_ENVELOPE));
		soundSetEffectHw(SNDFX_SWEEP_SNDLEN, effectValueEff(SNDFX_SWEEP_SNDLEN));
		soundSetEffectHw(SNDFX_SWEEP_DUTYCYCLE, effectValueEff(SNDFX_SWEEP_DUTYCYCLE));
		soundSetEffectHw(SNDFX_SWEEP_SPEED, effectValueEff(SNDFX_SWEEP_SPEED));
		soundSetEffectHw(SNDFX_PAN_CH3, effectValueEff(SNDFX_PAN_CH3));
	} else {
		soundSetEffectHw(SNDFX_PITCH, effectValueEff(SNDFX_PITCH));
		soundSetEffectHw(SNDFX_ENVELOPE, effectValueEff(SNDFX_ENVELOPE));
		soundSetEffectHw(SNDFX_SNDLEN, effectValueEff(SNDFX_SNDLEN));
		soundSetEffectHw(SNDFX_DUTYCYCLE, effectValueEff(SNDFX_DUTYCYCLE));
		soundSetEffectHw(SNDFX_PAN_CH1, effectValueEff(SNDFX_PAN_CH1));
		sndChanReg[0].l=(sndChanReg[0].l&0xFFF8); //disable sweep
	}
}

int soundGetEffectsChanged() {
	int r=someEffectChanged;
	someEffectChanged=0;
	return r;
}

void soundSetEffect(int effectNo, int value) {
	effectValues[effectNo]=value;
	soundSetEffectHw(effectNo, effectValueEff(effectNo));
	nvmemStoreCc(effectNo, value);
	someEffectChanged=1;
}

const int soundGetEffect(int effectNo) {
	return effectValues[effectNo];
}


static void soundPlayNoteHw(int chan, int midiNote, int velocity) {
	int i=gbChanToGlissIdx(chan);
	velocity>>=2;
	if (velocity>15) velocity=15;
	
	if (i!=-1 && velocity!=0) {
		if (nowPlaying[chan]==0 && needsGliss(chan)!=2) {
			//First note: don't glide unless glissando is always active.
			glissando[i].active=0;
		} else if (needsGliss(chan)) { 			//2nd note: activate gliss when needed.
			glissando[i].freqFrom=glissando[i].freqCurr;
			glissando[i].pos=0;
			glissando[i].active=1;
		}
	}

	if (velocity==0) {
		if (midiNote!=nowPlaying[chan]) return;
		nowPlaying[chan]=0;
	} else {
		if (midiNote==nowPlaying[chan]) return;
		nowPlaying[chan]=midiNote;
	}

	if (chan==0) {
		REG_SOUND1CNT_H=(velocity<<12)|sndChanReg[0].h;
		updateGbChannelParams(0, 1);
		if (sweepChanInUse) {
			REG_SOUND1CNT_X=0x8000|midiPlusPitchToFreq(midiNote, effectValueEff(SNDFX_SWEEP_PITCH))|sndChanReg[0].x;
		} else {
			REG_SOUND1CNT_X=0x8000|midiPlusPitchToFreq(midiNote, effectValueEff(SNDFX_PITCH))|sndChanReg[0].x;
		}
	} else if (chan==1) {
		REG_SOUND2CNT_L=(velocity<<12)|sndChanReg[1].l;
		updateGbChannelParams(1, 1);
	} else if (chan==2) {
		static const unsigned char velXlateTab[]={0, 0, 0, 3, 3, 3, 3, 2, 2, 2, 4, 4, 4, 1, 1, 1};
		velocity=velXlateTab[velocity];
		REG_SOUND3CNT_H=(velocity<<13)|sndChanReg[2].h;
		updateGbChannelParams(2, 1);
	} else if (chan==3) {
		REG_SOUND4CNT_L=(velocity<<12)|sndChanReg[3].l;
		updateGbChannelParams(3, 1);
	}
}


static int validArpeggioPos(int arpeggioPos, int chan) {
	if (notesPlaying[arpeggioPos].note==0) return 0;
	if (notesPlaying[arpeggioPos].chord==0) return 0;
	if (notesPlaying[arpeggioPos].channel!=chan) return 0;
	return 1;
}

static int findArpeggioForChannel(int arpeggioPos, int channel) {
	int oldArpeggioPos;
	if (arpeggioPos>MAXNOTES || arpeggioPos<0) arpeggioPos=0;

	oldArpeggioPos=arpeggioPos;
	do {
		arpeggioPos++;
		if (arpeggioPos>=MAXNOTES) arpeggioPos=0;
	} while (arpeggioPos!=oldArpeggioPos && !validArpeggioPos(arpeggioPos, channel));

	if (!validArpeggioPos(arpeggioPos, channel)) {
		doingArpeggio[channel]=0;
		return -1;
	} else {
		doingArpeggio[channel]=1;
		return arpeggioPos;
	}
}


void soundTick() {
	static int arpeggioPosChan0=0, arpeggioPosChan1=0;
	static unsigned short oldTimerPosTick=0;
	static unsigned short oldTimerPosGliss=0;
	static unsigned short oldTimerPosArpeggio=0;
	int x, i;

	//Keep directsound buffer filled.
	soundDirectRefillBuff();

	//Handle glissando.
	if ((unsigned short)(REG_TM0CNT_L-oldTimerPosGliss)>=GLISSTICKLEN) {
		oldTimerPosGliss+=GLISSTICKLEN;
		for (x=0; x<4; x++) {
			if (glissando[x].active && glissando[x].pos<65536) {
				glissando[x].pos+=(127-effectValueEff(SNDFX_GLISS_SPEED))*128+1;
				if (glissando[x].pos>65536) glissando[x].pos=65536;
				for (i=0; i<4; i++) {
					if (gbChanToGlissIdx(i)==x) updateGbChannelParams(i, 0);
				}
			}
		}
	}


	//Check if it's time already to do the next tick, for aging of the notes, glissando etc
	//Called 50 times a second.
	if ((unsigned short)(REG_TM0CNT_L-oldTimerPosTick)>=TICKLEN) {
		oldTimerPosTick+=TICKLEN;
		//Handle age of notes
		for (x=0; x<MAXNOTES; x++) {
			if (notesPlaying[x].age<127) notesPlaying[x].age++;
		}
		//Handle joining of sweep channel to main channel.
		if (sweepChanInUse>0) {
			sweepChanInUse--;
			if (!sweepChanInUse) switchSweepChan();
		}
	}

	//Handle sequencer stuff
	if ((unsigned short)(REG_TM0CNT_L-oldTimerPosSeq)>=seqTicklen) { 
		oldTimerPosSeq+=seqTicklen;
		seqTick();
	}


	//Handle LFOs
	for (x=0; x<MAXLFO; x++) {
		if (lfoData[x].range!=0) {
			if ((unsigned short)(REG_TM0CNT_L-lfoData[x].oldTimerPos)>=lfoData[x].tickLen) {
				lfoData[x].oldTimerPos+=lfoData[x].tickLen;
				lfoData[x].tick++;
				if (lfoData[x].tick<128) lfoData[x].value+=lfoData[x].range; else lfoData[x].value-=lfoData[x].range;
				if (lfoData[x].tick==64) lfoData[x].value=0; //force to middle

				//Do a soundSetEffect to re-apply the value.
				if (lfoData[x].appliesTo==SNDFX_DSNDSQUARE || lfoData[x].appliesTo==SNDFX_DSNDSAW || lfoData[x].appliesTo==SNDFX_DSNDDUTY) {
					//These are quite slow to set. Don't do it that often.
					if ((lfoData[x].tick&31)==0 || lfoData[x].tickLen>400) soundSetEffectHw(lfoData[x].appliesTo, effectValueEff(lfoData[x].appliesTo));
				} else {
					soundSetEffectHw(lfoData[x].appliesTo, effectValueEff(lfoData[x].appliesTo));
				}
			}
		} else {
			lfoData[x].value=0;
		}
	}

	//Do arpeggio stuff
	if (arpeggioTicklen==0) {
		doingArpeggio[0]=0;
		doingArpeggio[1]=0;
	} else if ((unsigned short)(REG_TM0CNT_L-oldTimerPosArpeggio)>=arpeggioTicklen) { //Check if it's time already to do the next arpeggio note
		oldTimerPosArpeggio+=arpeggioTicklen;

		x=findArpeggioForChannel(arpeggioPosChan0, 0);
		if (x>=0) {
			soundPlayNoteHw(1, notesPlaying[x].note, notesPlaying[x].velocity);
			arpeggioPosChan0=x;
		}

		x=findArpeggioForChannel(arpeggioPosChan1, 1);
		if (x>=0) {
			soundPlayNoteHw(2, notesPlaying[x].note, notesPlaying[x].velocity);
			arpeggioPosChan1=x;
		}
	}

	//Keep directsound buffer filled.
	soundDirectRefillBuff();

	//Do decay
	if (decayTicks!=0 && ((unsigned short)(REG_TM0CNT_L-oldTimerPosDecay)>=decayTicks)) {
		oldTimerPosDecay+=decayTicks;
		for (x=0; x<MAXNOTES; x++) {
			if (notesPlaying[x].note!=0 && notesPlaying[x].decaying!=0) {
//				notesPlaying[x].decaying++;
				notesPlaying[x].velocity--;
				if (notesPlaying[x].velocity==0) {
					notesPlaying[x].note=0;
					notesPlaying[x].channel=0;
				}
			}
		}

	}
}

static void notesPlayingRemove(int midiNote, int channel) {
	int x;
	for (x=0; x<MAXNOTES; x++) {
		if (notesPlaying[x].note==midiNote && notesPlaying[x].channel==channel) {
			if (decayTicks!=0 && channel==4) {
				notesPlaying[x].decaying=1;
			} else {
				notesPlaying[x].note=0;
				notesPlaying[x].chord=0;
				notesPlaying[x].velocity=0;
			}
		}
	}
}

static int notesPlayingAdd(int midiNote, int velocity, int channel) {
	int x=0;
	//Find a place in notesPlaying that's still free
	while (x<MAXNOTES && notesPlaying[x].note!=0 && !(notesPlaying[x].note==midiNote && notesPlaying[x].channel==channel)) {
		x++;
	}
	if (x>=MAXNOTES) {
		//Damn, all slots filled :/ Yank out oldest decaying note.
		int oldest=0, oldval=0;
		for (x=0; x<MAXNOTES; x++) {
			if (notesPlaying[x].decaying>oldval) {
				oldval=notesPlaying[x].decaying;
				oldest=x;
			}
		}
		x=oldest;
	}
	notesPlaying[x].note=midiNote;
	notesPlaying[x].velocity=velocity;
	notesPlaying[x].age=0;
	notesPlaying[x].decaying=0;
	notesPlaying[x].chord=0;
	notesPlaying[x].channel=channel;
	return x;
}


void soundPlayNote(int midiNote, int velocity) {
	int x,y;
	int forceToChan=-1;
	if (velocity==0) {
		if (nowPlaying[0]==midiNote) legatoDetected[0]=0;
		if (nowPlaying[1]==midiNote) legatoDetected[1]=0;
		notesPlayingRemove(midiNote, 0);
		soundPlayNoteHw(0, midiNote, 0);
		soundPlayNoteHw(1, midiNote, 0);
	} else {
		x=notesPlayingAdd(midiNote, velocity, 0);
		//See if we should chord or legato this note.
		for (y=0; y<MAXNOTES; y++) {
			if (x!=y && notesPlaying[y].note!=0 && notesPlaying[y].age<CHORDTRESH && (notesPlaying[y].channel==0 || notesPlaying[y].channel==1) && arpeggioTicklen!=0) {
				notesPlaying[x].chord=1;
				//Silence first channel, this note will be taken to the arpeggio unit.
				if (notesPlaying[y].note==nowPlaying[0] && !sweepChanInUse) {
					soundPlayNoteHw(0, notesPlaying[y].note, 0);
				}
				notesPlaying[y].chord=1;
				legatoDetected[0]=0;
				legatoDetected[1]=0;
			} else if (x!=y && notesPlaying[y].note!=0 && notesPlaying[y].age<LEGATOTRESH && notesPlaying[y].channel==0 && notesPlaying[y].chord==0) {
				if (notesPlaying[y].note==nowPlaying[0] && !sweepChanInUse) {
					legatoDetected[0]=1;
					if (needsGliss(0)) soundPlayNoteHw(0, nowPlaying[0], 0);
					forceToChan=0;
					ch0LegatoTarget=0;
				} else if (notesPlaying[y].note==nowPlaying[1]) {
					legatoDetected[1]=1;
					if (needsGliss(0)) soundPlayNoteHw(1, nowPlaying[1], 0);
					forceToChan=1;
					ch0LegatoTarget=1;
				}
			}
		}

		if (glissandoMode[0]>=GLISS_MODE_DOWN) {
			forceToChan=1;
			ch0LegatoTarget=1;
		}

		//If single note, play now.
		if (forceToChan!=-1) {
			soundPlayNoteHw(forceToChan, midiNote, velocity);
		} else if (!notesPlaying[x].chord) {
			if (sweepChanInUse) { //Chan1 is in use on it's own midi channel. Force to chan2, if that's not doing an arpeggio.
				if (!doingArpeggio[0]) soundPlayNoteHw(1, midiNote, velocity);
			} else if (doingArpeggio[0]) {
				//Force to channel 1, channel 2 is doing the arpeggio'ing
				soundPlayNoteHw(0, midiNote, velocity);
			} else {
				//Go find oldest channel and kill that.
				int ch1age=127, ch2age=127;
				int x;
				for (x=0; x<MAXNOTES; x++) {
					if (notesPlaying[x].note==nowPlaying[0] && notesPlaying[x].channel==0) ch1age=notesPlaying[x].age;
					if (notesPlaying[x].note==nowPlaying[1] && notesPlaying[x].channel==0) ch2age=notesPlaying[x].age;
				}
				//Default to ch1
				if (ch1age>ch2age) {
					soundPlayNoteHw(0, midiNote, velocity);
				} else {
					soundPlayNoteHw(1, midiNote, velocity);
				}
			}
		}
	}
}

void soundPlayNoteSweep(int midiNote, int velocity) {
	sweepChanInUse=3*50; //after 3 secs of inactivity, free hw chan 0 for soundPlayNote use again.
	switchSweepChan();
	soundPlayNoteHw(0, midiNote, velocity);
}

void soundPlaySample(int midiNote, int velocity) {
	int x;
	int forceArp;
	if (velocity==0) {
		if (nowPlaying[2]==midiNote) legatoDetected[2]=0;
		notesPlayingRemove(midiNote, 1);
		soundPlayNoteHw(2, midiNote, 0);
	} else {
		//See if we should legato this note.
		legatoDetected[2]=0;
		forceArp=0;
		for (x=0; x<MAXNOTES; x++) {
			if (notesPlaying[x].note!=0 && notesPlaying[x].age>=CHORDTRESH && notesPlaying[x].age<LEGATOTRESH && notesPlaying[x].channel==1) {
				legatoDetected[2]=1;
				if (needsGliss(2)) notesPlayingRemove(notesPlaying[x].note, 1);
			} else if (notesPlaying[x].note!=0 && notesPlaying[x].age<CHORDTRESH && notesPlaying[x].channel==1) {
				forceArp=1;
				notesPlaying[x].chord=1;
			}
		}
		x=notesPlayingAdd(midiNote, velocity, 1);
		if (!needsGliss(2)) {
			notesPlaying[x].chord=1;
		} else {
			soundPlayNoteHw(2, midiNote, velocity);
			if (forceArp) notesPlaying[x].chord=1;
		}
	}
}

void soundPlayNoise(int note, int vel) {
	soundPlayNoteHw(3, note, vel);
}

void soundMetronome(int tonetype) {
	soundMetronomeTick=tonetype;
}

// --- DirectSound stuff ---

static int midiPlusPitchToDsound(int note, int pitch) {
	long long from, diff;
	if (pitch==64) return midiToDdsVal(note);
	if (pitch<0) pitch=0;
	if (pitch>127) pitch=127;
	if (pitch<64) {
		from=midiToDdsVal(note-2);
		diff=midiToDdsVal(note)-from;
	} else if (pitch>=64) {
		pitch-=64;
		from=midiToDdsVal(note);
		diff=midiToDdsVal(note+2)-from;
	}
	return from+((diff*pitch)/64);
}


void soundPlayDirect(int midiNote, int velocity) {
	int x;
	if (velocity==0) {
		notesPlayingRemove(midiNote, 4);
	} else {
		x=notesPlayingAdd(midiNote, velocity, 4);
		notesPlaying[x].dsnd.pos=0;
		notesPlaying[x].dsnd.inc=midiPlusPitchToDsound(midiNote,  effectValueEff(SNDFX_DSOUND_PITCH));
		notesPlaying[x].dsnd.dsoundSampleBank=dsoundCurSampleBank;
	}
}

const int getSinVal(int x) {
	static const char quarterSin[]={ 0, 3, 6, 9, 12, 15, 18, 21, 24, 28,
	 31, 34, 37, 40, 43, 46, 48, 51, 54, 57, 60, 63, 65, 68, 71, 73, 76, 
	78, 81, 83, 85, 88, 90, 92, 94, 96, 98, 100, 102, 104, 106, 108, 109, 
	111, 112, 114, 115, 117, 118, 119, 120, 121, 122, 123, 124, 124, 125, 
	126, 126, 127, 127, 127, 127, 127 };
	x&=255;
	if (x>=128) return -getSinVal(x-128);
	if (x>=64) {
		return quarterSin[127-x];
	} else {
		return quarterSin[x];
	}
}

static void soundDirectGenSamples(int square, int saw, int duty) __attribute__ ((section(".iwram")));
static void soundDirectGenSamples(int square, int saw, int duty) {
	int x;
	int sine;
	int samp;
	//Swap banks
	if (dsoundCurSampleBank==dsoundSamplesA) dsoundCurSampleBank=dsoundSamplesB; else dsoundCurSampleBank=dsoundSamplesA;
	sine=128-square;
	sine-=saw;
	if (sine<0) {
		square+=sine;
		sine=0;
	}
	for (x=0; x<256; x++) {
		samp=getSinVal(x)*sine;
		samp+=(128-x)*saw;
		if (x<duty) samp+=127*square; else samp-=127*square;
		dsoundCurSampleBank[x]=(samp/128);
		soundDirectRefillBuff();
	}
}

//THIS IS HEAVILY CALLED! Optimize here!
static unsigned char soundDirectGetMixedSample()  __attribute__ ((section(".iwram")));
static unsigned char soundDirectGetMixedSample() {
	int val=0;
	int x;
	int smpno;
	int oldpos;
	for (x=0; x<MAXNOTES; x++) {
		if (notesPlaying[x].channel==4 && notesPlaying[x].note!=0) {
			oldpos=notesPlaying[x].dsnd.pos;
			notesPlaying[x].dsnd.pos+=notesPlaying[x].dsnd.inc;
			if (notesPlaying[x].dsnd.pos<oldpos) {
				//Zero-crossing. Change to new sample
				notesPlaying[x].dsnd.dsoundSampleBank=dsoundCurSampleBank;
				notesPlaying[x].dsnd.inc=midiPlusPitchToDsound(notesPlaying[x].note, effectValueEff(SNDFX_DSOUND_PITCH));
			}
			smpno=(unsigned int)(notesPlaying[x].dsnd.pos>>24);
			val+=(notesPlaying[x].dsnd.dsoundSampleBank[smpno&255]*notesPlaying[x].velocity)>>7;
		}
	}
	//Dirty way to make a sound... ah well.
	soundMetronomePos++;
	if (soundMetronomePos & soundMetronomeTick) val+=511;

	val=(val/4);
	//clip
	if (val>127) val=127;
	if (val<-128) val=-128;
	return val;
}

void soundDirectInt()  __attribute__ ((section(".iwram")));
void soundDirectInt() {
	volatile char *t;
	t=dsndBuffBack; dsndBuffBack=dsndBuffFront; dsndBuffFront=t;
	dsndBuffNeedsRefill=1;
	REG_DMA2CNT=0;
	REG_DMA2SAD=(unsigned long)dsndBuffFront;
	REG_DMA2CNT_H=0xB640;
}

void soundDirectRefillBuff()  __attribute__ ((section(".iwram")));
void soundDirectRefillBuff() {
	int x;
	if (!dsndBuffNeedsRefill) return;
	for (x=0; x<DSNDBUFFSZ; x++) {
		dsndBuffBack[x]=soundDirectGetMixedSample();
	}
	dsndBuffNeedsRefill=0;
	soundMetronomeTick=0;
}

