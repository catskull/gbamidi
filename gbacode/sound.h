#define SNDFX_DUTYCYCLE 1
#define SNDFX_SWEEP_DUTYCYCLE 2
#define SNDFX_NOISE_BITS 3
#define SNDFX_SAMPLE_SETSAMP 4
#define SNDFX_PITCH 5
#define SNDFX_SAMPLE_PITCH 6
#define SNDFX_NOISE_PITCH 7
#define SNDFX_SWEEP_PITCH 8
#define SNDFX_SWEEP_VAL 9
#define SNDFX_SWEEP_SPEED 10
#define SNDFX_SWEEP_SNDLEN 11
#define SNDFX_SWEEP_ENVELOPE 12
#define SNDFX_DSNDSQUARE 13
#define SNDFX_DSNDDUTY 14
#define SNDFX_DSNDSAW 15
#define SNDFX_DSOUND_PITCH 16
#define SNDFX_SAMPLE_SNDLEN 17
#define SNDFX_SNDLEN 18
#define SNDFX_ENVELOPE 19
#define SNDFX_NOISE_ENVELOPE 20
#define SNDFX_NOISE_SNDLEN 21
#define SNDFX_DECAY 22
#define SNDFX_GLISS_MODE 23
#define SNDFX_SAMPLE_GLISS_MODE 24
#define SNDFX_GLISS_SPEED 25
#define SNDFX_ARPEGGIATOR 26

#define SNDFX_SEQ_SPEED 30
#define SNDFX_SEQ_BPMEAS 31
#define SNDFX_SEQ_NOMEAS 32
#define SNDFX_SEQ_AUTOTO 33
#define SNDFX_SEQ_AUTOPCT 34
#define SNDFX_SEQ_BSPLIT 35
#define SNDFX_SEQ_BOFF 36

#define SNDFX_LFO1_TARGET 40
#define SNDFX_LFO1_RANGE 41
#define SNDFX_LFO1_FREQ 42
#define SNDFX_LFO2_TARGET 43
#define SNDFX_LFO2_RANGE 44
#define SNDFX_LFO2_FREQ 45
#define SNDFX_LFO3_TARGET 46
#define SNDFX_LFO3_RANGE 47
#define SNDFX_LFO3_FREQ 48

#define SNDFX_PAN_CH1 49
#define SNDFX_PAN_CH2 50
#define SNDFX_PAN_CH3 51
#define SNDFX_PAN_CH4 52
#define SNDFX_PAN_CH10 53
#define SNDFX_SAMPR 54
#define SNDFX_MIDICHAN 55

void soundPlayNote(int midiNote, int velocity);
void soundPlayNoise(int midiNote, int velocity);
void soundPlaySample(int midiNote, int velocity);
void soundPlayNoteSweep(int midiNote, int velocity);
void soundPlayDirect(int midiNote, int velocity);
void soundInit();
void soundTick(void);
void soundSetEffect(int effectNo, int value);
const int soundGetEffect(int effectNo);
void soundDirectInt();
int soundGetEffectsChanged();
void soundMetronome(int tonetype);
