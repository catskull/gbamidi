#define TRACK_NA 1
#define TRACK_EMPTY 2
#define TRACK_RECORDING 4
#define TRACK_PLAYING 8
#define TRACK_MUTED 16

int getMeasInfo();
int getCurrMeas();
int getTrackStatus(int track);
void seqInit();
int seqUiChanged();
void seqTick();
void seqRecordTrack(int trackNo);
void seqPlayBack();
void seqStop();
int seqNote(int chan, int note, int vel);
void seqSndfx(int no, int val);
int seqGetTimestamp();
void seqToggleMuteTrack(int track);