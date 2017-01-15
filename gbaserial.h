unsigned int gbaSerXfer(unsigned int data);
void gbaSerInit(void);
void gbaSerTx(unsigned int data);
unsigned int gbaSerRx(void);
//For 8-bit GB Classic SPI style comms
void gbaSerSpiInit(void);
unsigned char gbaSerSpiTxRx(unsigned char data);
