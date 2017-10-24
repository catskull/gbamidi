#define NULL (0L)


struct ccValExplStruct {
	unsigned char valTo;
	const char *desc;
};


struct CcDefStruct {
	char sndFxNo;
	unsigned short midiChannels;
	char ccNo;
	const char *desc;
	struct ccValExplStruct *expl;
} ;


struct CcDefStruct *ccDefs;

const int ccPlusChanToSndfxNo(int cc, int chan);
