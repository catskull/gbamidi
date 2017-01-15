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


//Definition of all the MIDI control channels, plus some knobs you can
//only tweak at the UI.

#include "ccs.h"
#include "sound.h"


struct ccValExplStruct lfoTargetExpl[]={
	{8,			"Ch1 duty"},
	{16,		"Ch3 duty"},
	{24,		"Noise bits"},
	{32,		"Ch2 sample"},
	{40,		"Ch1 pitch"},
	{48,		"Ch2 pitch"},
	{56,		"Noise ptch"},
	{64,		"Ch3 pitch"},
	{72,		"Sweep val"},
	{80,		"Sweep spd"},
	{88,		"Sweep len"},
	{96,		"Sweep env"},
	{104,		"Ch4 sqre"},
	{112,		"Ch4 sq dt"},
	{120,		"Ch4 saw"},
	{128,		"Ch4 pitch"},
	{0, NULL}
};


struct ccValExplStruct waveformExpl[]={
	{0x10,	"square"},
	{0x20,	"triangle"},
	{0x30,	"saw"},
	{0x40,	"2xsquare"},
	{0x50,	"sq gltch"},
	{0x60,	"harmon"},
	{0x70,	"squa alt"},
	{128,	"random"},
	{0, NULL}
};

struct ccValExplStruct glissModeExpl[]={
	{32,		"off"},
	{64,		"quickpress"},
	{96,		"legato"},
	{128,		"always"},
	{0, NULL}
};

struct ccValExplStruct autoMeasExpl[]={
	{32,		"off"},
	{64,		"1/4 meas"},
	{96,		"1/2 meas"},
	{128,		"1 meas"},
	{0, NULL}
};

struct ccValExplStruct noiseBitsExpl[]={
	{64,		"11-bit"},
	{128,		"7-bit"},
	{0, NULL}
};

struct ccValExplStruct dutyCycleExpl[]={
	{43,		"12.5%"},
	{85,		"25%"},
	{128,		"50%"},
	{0, NULL}
};

struct ccValExplStruct sampRateExpl[]={
	{32,		"9bit/32KHz"},
	{64,		"8bit/64KHz"},
	{96,		"7bit/131KHz"},
	{128,		"6bit/262KHz"},
	{0, NULL}
};


struct ccValExplStruct panExpl[]={
	{43,		"Left"},
	{85,		"Center"},
	{128,		"Right"},
	{0, NULL}
};

struct ccValExplStruct midiChanExpl[]={
	{32,		"Ch1-4,10"},
	{64,		"Ch1-5"},
	{96,		"Ch5-9"},
	{128,		"Ch12-16"},
	{0, NULL}
};

struct CcDefStruct myCcDefs[]={
	{SNDFX_ARPEGGIATOR,	(1<<1)|(1<<2),	12,	"Arpeggiator speed",	NULL},
	{SNDFX_SWEEP_VAL, 	(1<<3),			12,	"Sweep step size",		NULL},
	{SNDFX_DSNDSQUARE,	(1<<4), 		12,	"Square waveform pct",	NULL},
	{SNDFX_NOISE_BITS,	(1<<10),		12,	"Noise LFSR length",	noiseBitsExpl},
	{SNDFX_DUTYCYCLE,	(1<<1),			13,	"Duty cycle",			dutyCycleExpl},
	{SNDFX_SAMPLE_SETSAMP,	(1<<2),		13,	"Sample type",			waveformExpl},
	{SNDFX_SWEEP_DUTYCYCLE,	(1<<3),		13,	"Duty cycle",			dutyCycleExpl},
	{SNDFX_DSNDDUTY,	(1<<4),			13,	"Duty cycle of sq w",	NULL},
	{SNDFX_SWEEP_SPEED,	(1<<3), 		14,	"Sweep speed",			NULL},
	{SNDFX_DSNDSAW,		(1<<4), 		14,	"Saw wave pct",			NULL},
	{SNDFX_DECAY,		(1<<4),			72,	"Decay length",			NULL},
	{SNDFX_ENVELOPE,	(1<<1), 		73,	"Envelope length",		NULL},
	{SNDFX_SWEEP_ENVELOPE,	(1<<3), 	73,	"Envelope length",		NULL},
	{SNDFX_NOISE_ENVELOPE,	(1<<10), 	73,	"Envelope length",		NULL},
	{SNDFX_SNDLEN,		(1<<1), 		74,	"Sound length",			NULL},
	{SNDFX_SAMPLE_SNDLEN,(1<<2), 		74,	"Sound length",			NULL},
	{SNDFX_SWEEP_SNDLEN,(1<<3), 		74,	"Sound length",			NULL},
	{SNDFX_NOISE_SNDLEN,(1<<10), 		74,	"Sound length",			NULL},
	{SNDFX_GLISS_MODE,		(1<<1), 	75,	"Glissando mode",		glissModeExpl},
	{SNDFX_SAMPLE_GLISS_MODE,(1<<2), 	75,	"Glissando mode",		glissModeExpl},
	{SNDFX_GLISS_SPEED,	(1<<1)|(1<<2),	76,	"Glissando length",		NULL},
	{SNDFX_PAN_CH1,		(1<<1),			10, "Pan",					panExpl},
	{SNDFX_PAN_CH2,		(1<<2),			10, "Pan",					panExpl},
	{SNDFX_PAN_CH3,		(1<<3),			10, "Pan",					panExpl},
	{SNDFX_PAN_CH4,		(1<<4),			10, "Pan",					panExpl},
	{SNDFX_PAN_CH10,	(1<<10),		10, "Pan",					panExpl},
	{SNDFX_LFO1_TARGET,	0xFFFF,			16,	"LFO1 target",			lfoTargetExpl},
	{SNDFX_LFO1_RANGE,	0xFFFF,			17,	"LFO1 range",			NULL},
	{SNDFX_LFO1_FREQ,	0xFFFF,			18,	"LFO1 frequency",		NULL},
	{SNDFX_LFO2_TARGET,	0xFFFF,			19,	"LFO2 target",			lfoTargetExpl},
	{SNDFX_LFO2_RANGE,	0xFFFF,			20,	"LFO2 range",			NULL},
	{SNDFX_LFO2_FREQ,	0xFFFF,			21,	"LFO2 frequency",		NULL},
	{SNDFX_LFO3_TARGET,	0xFFFF,			22,	"LFO3 target",			lfoTargetExpl},
	{SNDFX_LFO3_RANGE,	0xFFFF,			23,	"LFO3 range",			NULL},
	{SNDFX_LFO3_FREQ,	0xFFFF,			24,	"LFO3 frequency",		NULL},
	{SNDFX_SAMPR, 		0xFFFF,			0,	"Audio samp rate",		sampRateExpl},
	{SNDFX_MIDICHAN,	0xFFFF,			0,	"MIDI channels",		midiChanExpl},
	{SNDFX_SEQ_SPEED,	0x0,			0,	"Speed",				NULL},
	{SNDFX_SEQ_BPMEAS,	0x0,			0,	"Bts/meas",				NULL},
	{SNDFX_SEQ_NOMEAS,	0x0,			0,	"Measures",				NULL},
	{SNDFX_SEQ_AUTOTO,	0x0,			0,	"Quantize to",			autoMeasExpl},
	{SNDFX_SEQ_AUTOPCT,	0x0,			0,	"Quantize rate",		NULL},
	{SNDFX_SEQ_BSPLIT,	0x0,			0,	"Bassline split",		NULL},
	{SNDFX_SEQ_BOFF,	0x0,			0,	"Bassline offset",		NULL},
	{0, 0, 0, NULL, NULL}
};

struct CcDefStruct *ccDefs=myCcDefs;

const int ccPlusChanToSndfxNo(int cc, int chan) {
	int m=(1<<chan);
	int x=0;
	while (ccDefs[x].desc!=NULL) {
		if ((ccDefs[x].midiChannels&m) && ccDefs[x].ccNo==cc) {
			return ccDefs[x].sndFxNo;
		}
		x++;
	}
	return -1;
}

