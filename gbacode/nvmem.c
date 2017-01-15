//Routines to load/store stuff to/from the nvram in the AVR

#include "serial.h"
#include "nvmem.h"
#include "seq.h"
#include "sound.h"
#include "gba.h"

#define LINK_ESCAPE 0xFF
#define LINK_FILLER 0xFE
#define LINK_STORE_CTR 0x1
#define LINK_REQ_CTR 0x2
#define LINK_STORE_TRACK 0x10
#define LINK_REQ_TRACK 0x20

#define PRESET_OFFSET 65
#define PRESET_LEN 64

static int disableWriteback=0; 

void nvmemStoreCc(int cc, char val) {
	if (disableWriteback) return;
	serialWrite(LINK_STORE_CTR);
	serialWrite(cc&0xff);
	serialWrite(cc>>8);
	serialWrite(val);
}

static unsigned char nvmemLoadCc(int cc) {
	unsigned char r;
	serialWrite(LINK_REQ_CTR);
	serialWrite(cc&0xff);
	serialWrite(cc>>8);
	//Ignore the ESC val sent before this
	while (serialRead()!=LINK_REQ_CTR) ;
	r=serialRead(); //controller no, ignore
	r=serialRead(); //controller value
	return r;
}

static void nvmemLoadTrack(int trackNo, void* trackDataPtr) {
	char *p;
	int n;
	p=(char *)trackDataPtr;
	serialWrite(LINK_REQ_TRACK|trackNo);
	while (serialRead()!=(LINK_REQ_TRACK|trackNo)) ;
	for (n=0; n<1024; n++) {
		*p=serialRead();
		p++;
	}
}

void nvmemStoreTrack(int trackno, void *trackDataPtr) {
	char *p;
	int n;
	p=(char *)trackDataPtr;
	serialWrite(LINK_STORE_TRACK|trackno);
	for (n=0; n<1024; n++) {
		serialWrite(*p);
		p++;
	}
}

void nvmemPresetSave(int preset) {
	int x;
	for (x=0; x<PRESET_LEN; x++) {
		nvmemStoreCc(x+PRESET_OFFSET+(PRESET_LEN*preset), nvmemLoadCc(x));
	}
}

void nvmemPresetLoad(int preset) {
	int x;
	for (x=0; x<PRESET_LEN; x++) {
		soundSetEffect(x, nvmemLoadCc(x+PRESET_OFFSET+(PRESET_LEN*preset)));
	}
}

void nvmemLoadAll() {
	int x;
	//Don't load anything if start+select button is pressed.
	if ((REG_KEYPAD&(BUTTON_START|BUTTON_SELECT))==0) return;
	//make sure the link is OK; this is usually the first thing we send over the line.
	serialRead();
	serialRead();
	serialRead();
	serialRead();

	char magicVal=nvmemLoadCc(0);
	if (magicVal==0xA5) { //Nvmem is valid.
		disableWriteback=1; //Make sure the cc values don't get immediately written back
		for (x=1; x<64; x++) {
			soundSetEffect(x, nvmemLoadCc(x));
		}
		disableWriteback=0;
		for (x=0; x<8; x++) {
			nvmemLoadTrack(x, seqGetTrackPtr(x));
			seqValidateTrack(x);
		}
	} else {
		nvmemStoreCc(0, 0xA5);
		for (x=1; x<64; x++) {
			nvmemStoreCc(x, soundGetEffect(x));
		}
	}
}
